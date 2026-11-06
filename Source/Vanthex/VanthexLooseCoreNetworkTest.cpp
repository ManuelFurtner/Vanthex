#include "VanthexPlayerController.h"
#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexCore.h"
#include "VanthexExtractionZone.h"
#include "VanthexGameState.h"
#include "VanthexPlayerState.h"
#include "VanthexBotController.h"
#include "HitscanWeapon.h"
#include "HealthComponent.h"
#include "VanthexEquipmentComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
namespace {
struct FLooseCoreNetworkProbe {
 FTimerHandle Timer; TWeakObjectPtr<AVanthexCharacter> Pawn;
 double Started=0,SetupAt=-1,WonAt=-1;float FirstLoose=-1,LastRemaining=16;
 bool Dropped=false,SawHeld=false,SawLoose=false,Advanced=false,Monotonic=true,TeamCorrect=true,Done=false;
 bool Eliminated=false,SawAllDead=false,KnifeRequested=false,SawKnifeBonus=false;
 bool PickedUpAgain=false,SawFriendlyRecovery=false;
};
}
#endif
void VanthexStartLooseCoreNetworkTest(AVanthexPlayerController* PC)
{
#if !UE_BUILD_SHIPPING
 if(!PC||!PC->IsLocalController()||!PC->GetWorld())return;
 const bool Host=PC->HasAuthority();auto S=MakeShared<FLooseCoreNetworkProbe>();S->Started=FPlatformTime::Seconds();
 TWeakObjectPtr<AVanthexPlayerController> WeakPC=PC;
 PC->GetWorld()->GetTimerManager().SetTimer(S->Timer,FTimerDelegate::CreateLambda([S,WeakPC,Host](){
  const auto KeepAlive=S;auto* Owner=WeakPC.Get();if(!Owner||S->Done)return;
  auto* World=Owner->GetWorld();const double Now=FPlatformTime::Seconds();
  const bool Elimination=FParse::Param(FCommandLine::Get(),TEXT("EliminationNetworkTest"));
  const bool Friendly=FParse::Param(FCommandLine::Get(),TEXT("FriendlyPickupNetworkTest"));
  auto Finish=[&](bool OK){S->Done=true;const bool Pass=OK&&S->SawHeld&&S->SawLoose&&S->Advanced&&S->Monotonic&&S->TeamCorrect&&(!Elimination||(S->SawAllDead&&S->SawKnifeBonus))&&(!Friendly||S->SawFriendlyRecovery);
   const FString Report=FString::Printf(TEXT("%s role=%s held=%d loose=%d countdown_advanced=%d monotonic=%d team_correct=%d victory=%d eliminated_team_extraction=%d knife_bonus=%d friendly_recovery=%d\n"),Pass?TEXT("PASS"):TEXT("FAIL"),Host?TEXT("host"):TEXT("client"),S->SawHeld,S->SawLoose,S->Advanced,S->Monotonic,S->TeamCorrect,OK,S->SawAllDead,S->SawKnifeBonus,S->SawFriendlyRecovery);
   const FString Name=FString(Friendly?TEXT("FriendlyPickupNetwork"):Elimination?TEXT("EliminationNetwork"):TEXT("LooseCoreNetwork"))+(Host?TEXT("Host.txt"):TEXT("Client.txt"));
   FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/Name));UE_LOG(LogVanthex,Display,TEXT("LOOSE_CORE_NETWORK %s"),*Report);World->GetTimerManager().ClearTimer(S->Timer);FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);};
  if(Now-S->Started>60){Finish(false);return;}
  auto* Match=World->GetGameState<AVanthexGameState>();if(!Match)return;
  if(Host&&S->SetupAt<0) for(TActorIterator<AVanthexCharacter> It(World);It;++It){
   if(auto* Bot=Cast<AVanthexBotController>(It->GetController()))Bot->SetActorTickEnabled(false);
   if(It->GetWeapon())It->GetWeapon()->StopFire();
   It->GetCharacterMovement()->StopMovementImmediately();It->GetCharacterMovement()->DisableMovement();
  }
  if(!S->Pawn.IsValid()){
   if(!Host)S->Pawn=Cast<AVanthexCharacter>(Owner->GetPawn());
   else for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It)
    if(auto* Other=It->Get();Other&&!Other->IsLocalController())S->Pawn=Cast<AVanthexCharacter>(Other->GetPawn());
  }
  auto* Pawn=S->Pawn.Get();auto* PS=Pawn?Pawn->GetPlayerState<AVanthexPlayerState>():nullptr;if(!PS)return;
  if(Elimination){
   if(!Host&&!S->KnifeRequested&&(Match->GetPhase()==EVanthexRoundPhase::BuyPhase||Match->GetPhase()==EVanthexRoundPhase::RoundActive)){Pawn->GetEquipment()->SelectKnife();S->KnifeRequested=true;}
   if(Pawn->GetEquipment()->GetSelectedSlot()==EVanthexWeaponSlot::Knife&&!Pawn->GetEquipment()->IsSwitching()&&FMath::IsNearlyEqual(Pawn->GetCharacterMovement()->MaxWalkSpeed,690.f))S->SawKnifeBonus=true;
   if(Host&&!S->SawKnifeBonus)return;
  }
  TActorIterator<AVanthexCore> CoreIterator(World);
  AVanthexCore* Core=CoreIterator?*CoreIterator:nullptr;if(!Core)return;
  AVanthexExtractionZone* Zone=nullptr;for(TActorIterator<AVanthexExtractionZone> It(World);It;++It)if(It->GetId()==EVanthexExtractionId::A){Zone=*It;break;}if(!Zone)return;
  if(Host&&S->SetupAt<0&&Match->GetPhase()==EVanthexRoundPhase::RoundActive){
   Core->ResetToSpawn();Pawn->SetActorLocation(Core->GetActorLocation()+FVector(0,0,60),false,nullptr,ETeleportType::TeleportPhysics);
   if(!Core->TryPickup(Pawn)){Finish(false);return;}
   Pawn->GetController()->SetControlRotation(FRotator::ZeroRotator);Pawn->SetActorRotation(FRotator::ZeroRotator);
   Pawn->SetActorLocation(Zone->GetActorLocation()+FVector(-60,0,96),false,nullptr,ETeleportType::TeleportPhysics);S->SetupAt=Now;
  }
  if(Host&&S->SetupAt>=0&&!S->Dropped&&Now-S->SetupAt>=2){
   Core->Drop();S->Dropped=true;Pawn->SetActorLocation(Zone->GetActorLocation()+FVector(1000,0,96),false,nullptr,ETeleportType::TeleportPhysics);
  }
  if(Match->GetCoreReadout()==EVanthexCoreReadout::ExtractingA){
   if(Elimination){
    if(Host&&S->Dropped&&!S->Eliminated&&Core->GetCoreState()==EVanthexCoreState::AtRest){
     for(TActorIterator<AVanthexCharacter> It(World);It;++It){if(Friendly&&It->GetPlayerState<AVanthexPlayerState>()->GetTeam()==PS->GetTeam())continue;FVanthexDamageRequest Fatal;Fatal.Damage=1000;It->GetHealthComponent()->ApplyDamage(Fatal);}S->Eliminated=true;
    }
    int32 Dead=0;for(TActorIterator<AVanthexCharacter> It(World);It;++It)if(It->GetHealthComponent()->IsDead())++Dead;
    if(Dead==(Friendly?3:6)&&Match->GetPhase()==EVanthexRoundPhase::RoundActive)S->SawAllDead=true;
   }
   const float Remaining=Match->GetExtractionRemaining();
   if(Friendly&&Host&&S->Eliminated&&!S->PickedUpAgain&&Now-S->SetupAt>6){Pawn->SetActorLocation(Core->GetActorLocation()+FVector(-50,0,76),false,nullptr,ETeleportType::TeleportPhysics);S->PickedUpAgain=Core->TryPickup(Pawn);}
   if(Friendly&&S->SawLoose&&Core->GetCarrier()==Pawn){S->SawFriendlyRecovery=true;S->Monotonic&=Remaining<=S->LastRemaining+.15f;S->LastRemaining=Remaining;}
   if(Core->GetCarrier())S->SawHeld=true;
   if(!Core->GetCarrier()&&Core->GetCoreState()==EVanthexCoreState::AtRest){
    S->SawLoose=true;S->TeamCorrect&=Core->GetExtractionTeam()==PS->GetTeam();
    if(S->FirstLoose<0)S->FirstLoose=Remaining;
    if(Remaining<S->FirstLoose-2)S->Advanced=true;
    S->Monotonic&=Remaining<=S->LastRemaining+.15f;S->LastRemaining=Remaining;
   }
  }
  if(Core->GetCoreState()==EVanthexCoreState::Extracted&&Match->GetRoundWinner()==PS->GetTeam()){
   if(S->WonAt<0)S->WonAt=Now;
   if(!Host||Now-S->WonAt>.7)Finish(true);
  }
 }),.05f,true,.2f);
#endif
}
