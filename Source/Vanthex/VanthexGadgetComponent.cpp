#include "VanthexGadgetComponent.h"
#include "VanthexGadgetActor.h"
#include "VanthexGadgetRules.h"
#include "VanthexCharacter.h"
#include "VanthexPlayerState.h"
#include "VanthexGameMode.h"
#include "HitscanWeapon.h"
#include "HealthComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

UVanthexGadgetComponent::UVanthexGadgetComponent(){SetIsReplicatedByDefault(true);}
float UVanthexGadgetComponent::GetServerTime() const
{
 const auto* W=GetWorld(); const auto* GS=W?W->GetGameState():nullptr;
 return GS?GS->GetServerWorldTimeSeconds():(W?W->GetTimeSeconds():0.f);
}
void UVanthexGadgetComponent::UseSlot(int32 Slot){if(++NextRequest==0)++NextRequest; ServerThrow(Slot,NextRequest);}
void UVanthexGadgetComponent::ServerThrow_Implementation(int32 Slot,uint32 RequestId){TryThrow(Slot,RequestId);}
bool UVanthexGadgetComponent::TryThrow(int32 Slot,uint32 RequestId)
{
 auto* C=Cast<AVanthexCharacter>(GetOwner());
 if(!C||!C->HasAuthority()||!RequestId) return false;
 if(RequestOwner.Get()!=C->GetController()){RequestOwner=C->GetController();LastRequest=0;}
 if(RequestId<=LastRequest)return false;
 LastRequest=RequestId;
 auto* PS=C->GetPlayerState<AVanthexPlayerState>(); const auto* Mode=GetWorld()->GetAuthGameMode<AVanthexGameMode>();
 if(!PS||!Mode||!Mode->IsRoundActive()||!PS->IsAlive()||PS->GetTeam()==EVanthexTeam::None||!C->GetController()
  ||C->GetHealthComponent()->IsDead()||Slot<0||!PS->GetEconomy().Gadgets.IsValidIndex(Slot)||GetServerTime()<NextThrow) return false;
 const FName Id=PS->GetEconomy().Gadgets[Slot]; if(!VanthexGadgetRules::Find(Id))return false;
 const FVector Eye=C->GetPawnViewLocation(); const FVector Forward=C->GetControlRotation().Vector();
 if(Eye.ContainsNaN()||Forward.ContainsNaN())return false;
 FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(GadgetSpawn),false,C);
 const FVector Desired=Eye+Forward*50.f;
 const bool Blocked=GetWorld()->SweepSingleByChannel(Hit,Eye,Desired,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(8.f),Q);
 const FVector Position=Blocked?Hit.Location-Forward*2.f:Desired;
 if(GetWorld()->OverlapBlockingTestByChannel(Position,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(8.f),Q)) return false;
 const FTransform Transform(C->GetControlRotation(),Position);
 auto* Gadget=GetWorld()->SpawnActorDeferred<AVanthexGadgetActor>(AVanthexGadgetActor::StaticClass(),Transform,C,C,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!Gadget) return false;
 Gadget->Configure(Id,C);
 if(!PS->ConsumeGadget(Slot,Id)){Gadget->Destroy();return false;}
 Gadget->FinishSpawning(Transform); Gadget->Launch(Forward*1150.f+FVector(0,0,180.f));
 NextThrow=GetServerTime()+.8f;
 if(auto* W=C->GetWeapon()){W->StopFire();W->CancelReload();}
 return true;
}
void UVanthexGadgetComponent::ResetForRound()
{
 if(!GetOwner()->HasAuthority())return;
 FlashEnd=JamEnd=FlashPower=NextThrow=0.f; Marks.Reset(); GetOwner()->ForceNetUpdate();
 // Keep the request high-water mark across rounds: delayed reliable packets cannot consume new purchases.
}
void UVanthexGadgetComponent::ApplyFlash(float Strength)
{
 if(!GetOwner()->HasAuthority()||!FMath::IsFinite(Strength)||Strength<=0)return;
 Strength=FMath::Clamp(Strength,0.f,1.f);
 FlashPower=FMath::Max(GetFlashOpacity(),Strength);FlashEnd=FMath::Max(FlashEnd,GetServerTime()+4.f*Strength);GetOwner()->ForceNetUpdate();
}
void UVanthexGadgetComponent::ApplyEMP(float Duration)
{
 if(!GetOwner()->HasAuthority()||!FMath::IsFinite(Duration)||Duration<=0)return;
 JamEnd=FMath::Max(JamEnd,GetServerTime()+Duration);Marks.Reset();GetOwner()->ForceNetUpdate();
}
float UVanthexGadgetComponent::GetFlashOpacity() const
{
 const float Left=FlashEnd-GetServerTime();
 return Left>0?FMath::Clamp(FlashPower*FMath::Min(Left/.7f,1.f),0.f,1.f):0.f;
}
bool UVanthexGadgetComponent::IsBlinded()const{return GetFlashOpacity()>.12f;}
bool UVanthexGadgetComponent::IsJammed()const{return JamEnd>GetServerTime();}
void UVanthexGadgetComponent::AddSensorMark(FVector Position)
{
 if(!GetOwner()->HasAuthority()||IsJammed()||Position.ContainsNaN())return;
 const float Now=GetServerTime(); Marks.RemoveAll([Now](const auto& M){return M.Expires<=Now;});
 for(auto& M:Marks) if(FVector::DistSquared(M.Position,Position)<FMath::Square(100.f)){M.Position=Position;M.Expires=Now+1.25f;GetOwner()->ForceNetUpdate();return;}
 if(Marks.Num()>=8)Marks.RemoveAt(0);
 FVanthexSensorMark M;M.Position=Position;M.Expires=Now+1.25f;Marks.Add(M);GetOwner()->ForceNetUpdate();
}
void UVanthexGadgetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME_CONDITION(UVanthexGadgetComponent,FlashEnd,COND_OwnerOnly);
 DOREPLIFETIME_CONDITION(UVanthexGadgetComponent,FlashPower,COND_OwnerOnly);
 DOREPLIFETIME_CONDITION(UVanthexGadgetComponent,JamEnd,COND_OwnerOnly);
 DOREPLIFETIME_CONDITION(UVanthexGadgetComponent,Marks,COND_OwnerOnly);
}
