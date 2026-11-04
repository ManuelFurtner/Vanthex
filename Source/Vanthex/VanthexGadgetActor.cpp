#include "VanthexGadgetActor.h"
#include "VanthexGadgetRules.h"
#include "VanthexGadgetComponent.h"
#include "VanthexGadgetVisualComponent.h"
#include "VanthexCharacter.h"
#include "VanthexPlayerState.h"
#include "VanthexShiftDevice.h"
#include "VanthexGameMode.h"
#include "HealthComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"

AVanthexGadgetActor::AVanthexGadgetActor()
{
 bReplicates=true;bAlwaysRelevant=true;SetReplicateMovement(true);SetNetUpdateFrequency(30.f);
 PrimaryActorTick.bCanEverTick=true;
 Collision=CreateDefaultSubobject<USphereComponent>(TEXT("GadgetCollision"));SetRootComponent(Collision);
 Collision->InitSphereRadius(8.f);Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
 Collision->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
 Movement=CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Flight"));Movement->UpdatedComponent=Collision;
 Movement->InitialSpeed=0.f;Movement->MaxSpeed=2200.f;Movement->bShouldBounce=true;
 Movement->Bounciness=.35f;Movement->Friction=.35f;Movement->ProjectileGravityScale=1.f;
 Movement->bRotationFollowsVelocity=false;Movement->bForceSubStepping=true;
 Visual=CreateDefaultSubobject<UVanthexGadgetVisualComponent>(TEXT("GadgetVisual"));
}
float AVanthexGadgetActor::ServerTime()const
{
 const auto* GS=GetWorld()->GetGameState();return GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
}
void AVanthexGadgetActor::Configure(FName Id,AVanthexCharacter* Thrower)
{
 if(!HasAuthority())return;
 GadgetId=Id;InstigatorCharacter=Thrower;
 if(Thrower){if(auto* S=Thrower->GetPlayerState<AVanthexPlayerState>())ThrowTeam=S->GetTeam();Collision->IgnoreActorWhenMoving(Thrower,true);}
}
void AVanthexGadgetActor::BeginPlay()
{
 Super::BeginPlay();SpawnTime=ServerTime();Visual->Configure(GadgetId);
 Movement->OnProjectileBounce.AddDynamic(this,&AVanthexGadgetActor::OnBounce);
 if(!HasAuthority()){Movement->Deactivate();Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);}
 OnRep_Effect();
}
void AVanthexGadgetActor::Launch(FVector Velocity){if(HasAuthority()&&!Velocity.ContainsNaN())Movement->Velocity=Velocity;}
bool AVanthexGadgetActor::CanReach(const FVector& End,const AActor* Target)const
{
 FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(GadgetEffect),false,this);Q.AddIgnoredActor(Target);
 // A deployed device is near the floor, so lift its query origin above its shell.
 return !GetWorld()->LineTraceSingleByChannel(Hit,GetActorLocation()+FVector(0,0,12),End,ECC_Visibility,Q);
}
void AVanthexGadgetActor::Detonate()
{
 if(!HasAuthority()||bActivated)return;
 const auto* Profile=VanthexGadgetRules::Find(GadgetId);const auto* Mode=GetWorld()->GetAuthGameMode<AVanthexGameMode>();
 if(!Profile||!Mode||!Mode->IsRoundActive()){Destroy();return;}
 bActivated=true;ActivationServerTime=ServerTime();Movement->StopMovementImmediately();Movement->Deactivate();
 Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);ForceNetUpdate();OnRep_Effect();
 if(GadgetId==TEXT("Flash")||GadgetId==TEXT("Frag")||GadgetId==TEXT("EMP")) {
  for(TActorIterator<AVanthexCharacter> It(GetWorld());It;++It){
   auto* C=*It;auto* PS=C->GetPlayerState<AVanthexPlayerState>();
   if(!PS||C->GetHealthComponent()->IsDead())continue;
   const FVector Target=GadgetId==TEXT("Flash")?C->GetPawnViewLocation():C->GetActorLocation();
   const float Distance=FVector::Dist(GetActorLocation(),Target);
   if(Distance>=Profile->Radius||!CanReach(Target,C))continue;
   if(GadgetId==TEXT("Flash")){
    if(SmokeBlocks(GetWorld(),GetActorLocation(),Target))continue;
    const FVector Toward=(GetActorLocation()-Target).GetSafeNormal();
    const float Facing=FVector::DotProduct(C->GetControlRotation().Vector(),Toward);
    C->GetGadgets()->ApplyFlash(VanthexGadgetRules::FlashStrength(Distance,Facing,Profile->Radius));
   } else if(GadgetId==TEXT("EMP")) C->GetGadgets()->ApplyEMP(Profile->Duration);
   else {
    // Friendly fire is off; self damage remains a meaningful throw risk.
    if(C!=InstigatorCharacter&&PS->GetTeam()==ThrowTeam)continue;
    FVanthexDamageRequest R;R.InstigatorActor=InstigatorCharacter;R.InstigatorController=InstigatorCharacter?InstigatorCharacter->GetController():nullptr;
    R.DamageKind=EVanthexDamageKind::Explosion;R.Damage=VanthexGadgetRules::BlastDamage(Distance,Profile->Radius,Profile->MaxDamage);
    C->GetHealthComponent()->ApplyDamage(R);
   }
  }
 }
 if(GadgetId==TEXT("EMP")){
  for(TActorIterator<AVanthexShiftDevice> It(GetWorld());It;++It)
   if(FVector::Dist(GetActorLocation(),It->GetActorLocation())<Profile->Radius&&CanReach(It->GetActorLocation(),*It))It->ApplyEMP(Profile->Duration);
  for(TActorIterator<AVanthexGadgetActor> It(GetWorld());It;++It)
   if(It->GadgetId==TEXT("Sensor")&&FVector::Dist(GetActorLocation(),It->GetActorLocation())<Profile->Radius&&CanReach(It->GetActorLocation(),*It))It->ApplyEMP(Profile->Duration);
 }
 NextScan=ServerTime();
}
void AVanthexGadgetActor::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 const auto* P=VanthexGadgetRules::Find(GadgetId);if(!P)return;
 if(HasAuthority()){
  const auto* Mode=GetWorld()->GetAuthGameMode<AVanthexGameMode>();
  if(!Mode||!Mode->IsRoundActive()){Destroy();return;}
  if(!bActivated&&ServerTime()-SpawnTime>=P->Fuse)Detonate();
  if(bActivated&&ServerTime()-ActivationServerTime>=P->Duration){Destroy();return;}
  if(IsSensorActive()&&ServerTime()>=NextScan){NextScan=ServerTime()+1.f;Scan();}
 }
 if(bActivated&&GadgetId==TEXT("Sensor")&&(bVisualActive!=IsSensorActive()))OnRep_Effect();
}
void AVanthexGadgetActor::Scan()
{
 if(!HasAuthority()||!IsSensorActive()||ThrowTeam==EVanthexTeam::None)return;
 MulticastPulse();
 TArray<FVector> Positions;
 for(TActorIterator<AVanthexCharacter> It(GetWorld());It;++It){
  auto* S=It->GetPlayerState<AVanthexPlayerState>();
  if(!S||!S->IsAlive()||S->GetTeam()==ThrowTeam||S->GetTeam()==EVanthexTeam::None)continue;
  const FVector P=It->GetActorLocation();
  if(FVector::DistSquared(GetActorLocation(),P)>FMath::Square(GetEffectRadius())||!CanReach(P,*It)||SmokeBlocks(GetWorld(),GetActorLocation(),P))continue;
  Positions.Add(P);
 }
 for(TActorIterator<AVanthexCharacter> It(GetWorld());It;++It){
  auto* S=It->GetPlayerState<AVanthexPlayerState>();if(!S||!S->IsAlive()||S->GetTeam()!=ThrowTeam)continue;
  for(const FVector& P:Positions)It->GetGadgets()->AddSensorMark(P);
 }
}
void AVanthexGadgetActor::ApplyEMP(float Duration)
{
 if(!HasAuthority()||GadgetId!=TEXT("Sensor")||!FMath::IsFinite(Duration)||Duration<=0)return;
 JamUntil=FMath::Max(JamUntil,ServerTime()+Duration);ForceNetUpdate();OnRep_Effect();
}
float AVanthexGadgetActor::GetEffectRadius()const{const auto* P=VanthexGadgetRules::Find(GadgetId);return P?P->Radius:0.f;}
bool AVanthexGadgetActor::IsSmokeActive()const{const auto* P=VanthexGadgetRules::Find(GadgetId);return P&&GadgetId==TEXT("Smoke")&&bActivated&&ServerTime()<ActivationServerTime+P->Duration;}
bool AVanthexGadgetActor::IsSensorActive()const{const auto* P=VanthexGadgetRules::Find(GadgetId);return P&&GadgetId==TEXT("Sensor")&&bActivated&&ServerTime()<ActivationServerTime+P->Duration&&ServerTime()>=JamUntil;}
bool AVanthexGadgetActor::SmokeBlocks(UWorld* World,const FVector& A,const FVector& B)
{
 if(!World)return false;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(SmokeRoomBoundary),false);
 // People do not partition smoke. Match the wall-constrained visual volume and
 // interior HUD: smoke in an adjacent sealed room cannot blind an otherwise clear ray.
 for(TActorIterator<APawn> Pawn(World);Pawn;++Pawn)Query.AddIgnoredActor(*Pawn);
 for(TActorIterator<AVanthexGadgetActor> It(World);It;++It)if(It->IsSmokeActive())
 {
  const FVector Center=It->GetActorLocation()+FVector(0,0,140);
  if(!VanthexGadgetRules::SegmentIntersectsSmoke(A,B,Center,It->GetEffectRadius()))continue;
  const FVector Closest=FMath::ClosestPointOnSegment(Center,A,B);
  Query.AddIgnoredActor(*It);
  if(!World->LineTraceTestByChannel(Center,Closest,ECC_Visibility,Query))return true;
 }
 return false;
}
void AVanthexGadgetActor::ResetAll(UWorld* World){if(World)for(TActorIterator<AVanthexGadgetActor> It(World);It;++It)if(It->HasAuthority())It->Destroy();}
void AVanthexGadgetActor::OnRep_Effect()
{
 if(!HasActorBegunPlay())return;
 Visual->Configure(GadgetId);
 const auto* P=VanthexGadgetRules::Find(GadgetId);if(!P||!bActivated)return;
 const bool Active=GadgetId!=TEXT("Sensor")||IsSensorActive();
 if(Active&&!bVisualActive){Visual->ActivateEffect(P->Duration,P->Radius,FMath::Max(0.f,ServerTime()-ActivationServerTime));bVisualActive=true;}
 else if(!Active&&bVisualActive){Visual->EndEffect();bVisualActive=false;}
}
void AVanthexGadgetActor::OnBounce(const FHitResult& Hit,const FVector& Velocity)
{if(HasAuthority()&&Velocity.Size()>120.f&&ServerTime()>=NextBounce){NextBounce=ServerTime()+.15f;MulticastBounce();}}
void AVanthexGadgetActor::MulticastBounce_Implementation(){Visual->PlayBounce();}
void AVanthexGadgetActor::MulticastPulse_Implementation(){Visual->PulseEffect();}
void AVanthexGadgetActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(AVanthexGadgetActor,GadgetId);DOREPLIFETIME(AVanthexGadgetActor,bActivated);
 DOREPLIFETIME(AVanthexGadgetActor,ActivationServerTime);DOREPLIFETIME(AVanthexGadgetActor,JamUntil);
}
