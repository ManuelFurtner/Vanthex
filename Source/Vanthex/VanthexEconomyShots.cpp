#include "VanthexGameMode.h"

#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexBuyMenuComponent.h"
#include "VanthexCharacter.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexPlayerController.h"
#include "VanthexPlayerState.h"
#include "VanthexWeaponAudioAcoustics.h"
#include "VanthexMapLayout.h"
#include "HitscanWeapon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
struct FEconomyShotState
{
 TWeakObjectPtr<UWorld> World;
 double Started=0., SettledAt=-1.;
 int32 View=0;
 bool Initialized=false, WaitingForFile=false, Finished=false;
 FString Report;
 TArray<FString> Files;
};
const TCHAR* ShotFiles[]={TEXT("01_buy_start.png"),TEXT("02_buy_aug.png"),TEXT("03_aug_first_person.png"),TEXT("04_mp7_first_person.png")};
}
#endif

// Opt-in diagnostic only. Uses the real owning controller, buy widget and equipped first-person weapon.
void VanthexRunEconomyShots(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode || !Mode->HasAuthority() || !Mode->GetWorld()) return;
 UWorld* World=Mode->GetWorld();
 static FEconomyShotState State;
 const double Now=FPlatformTime::Seconds();
 const FString Directory=FPaths::ProjectSavedDir()/TEXT("EconomyShots");
 if(State.World.Get()!=World) {
  State=FEconomyShotState(); State.World=World; State.Started=Now;
  IFileManager::Get().MakeDirectory(*Directory,true);
  IFileManager::Get().Delete(*(Directory/TEXT("capture.txt")),false,true);
 }
 if(State.Finished) return;
 auto Finish=[&](bool Passed,const FString& Detail) {
  State.Finished=true;
  const FString Report=FString::Printf(TEXT("%s %s\n"),Passed?TEXT("PASS"):TEXT("FAIL"),*Detail)+State.Report+FString::Join(State.Files,TEXT("\n"))+TEXT("\n");
  FFileHelper::SaveStringToFile(Report,*(Directory/TEXT("capture.txt")));
  UE_LOG(LogVanthex,Display,TEXT("ECONOMY_SHOTS %s %s"),Passed?TEXT("PASS"):TEXT("FAIL"),*Detail);
  Mode->GetWorldTimerManager().ClearTimer(Mode->VisualShotTimer);
  FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
 };
 if(Now-State.Started>60.) { Finish(false,FString::Printf(TEXT("timeout at view %d"),State.View+1)); return; }
 AVanthexPlayerController* PC=nullptr;
 for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator(); It; ++It)
  if(auto* Candidate=Cast<AVanthexPlayerController>(It->Get()); Candidate && Candidate->IsLocalController()) { PC=Candidate; break; }
 auto* Pawn=PC?Cast<AVanthexCharacter>(PC->GetPawn()):nullptr;
 auto* PS=PC?PC->GetPlayerState<AVanthexPlayerState>():nullptr;
 if(!PC || !Pawn || !PS || !PS->IsAlive() || !Pawn->GetWeapon() || !Pawn->GetEquipment() || !PC->GetBuyMenu()) return;
 if(!State.Initialized) {
  for(TActorIterator<AVanthexCharacter> It(World);It;++It) if(*It!=Pawn) It->SetActorHiddenInGame(true);
  if(!Mode->IsBuyPhase()) return;
  if(PS->GetEconomy().Credits!=800 || !PS->GetEconomy().PrimaryWeapon.IsNone()) { Finish(false,TEXT("initial player inventory is not the real 800 CR start")); return; }
  // Freeze only this diagnostic's phase clock; normal purchases and activation still use their real paths.
  Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
  Mode->TimeRemaining=25.f; Mode->PublishMatch();
  PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true); PC->SetViewTarget(Pawn);
  if(PC->GetHUD()) PC->GetHUD()->bShowHUD=true;
  if(VanthexMap::IsGridV3(World)) {
   const FVector Source=Pawn->GetActorLocation()+FVector(0.,0.,55.);
   for(const auto& Target: {TPair<const TCHAR*,FVector>(TEXT("core"),FVector(0.,0.,145.)), TPair<const TCHAR*,FVector>(TEXT("upper_observation"),FVector(-400.,0.,745.))}) {
    const auto Route=VanthexWeaponAcoustics::Route(World,Source,Target.Value,Pawn);
    State.Report+=FString::Printf(TEXT("ACOUSTICS actual map spawn->%s direct=%d portal=%d distance=%.1f gain=%.3f lowpass=%.0f hops=%d sourceRoom=%d listenerRoom=%d\n"),
     Target.Key,Route.bDirect,Route.bPortal,Route.Distance,Route.Transmission,Route.LowPassHz,Route.Hops,Route.SourceRoom,Route.ListenerRoom);
   }
  }
  State.Initialized=true;
 }
 if(FScreenshotRequest::IsScreenshotRequested()) return;
 if(State.WaitingForFile) {
  if(IFileManager::Get().FileSize(*State.Files.Last())<=0) return;
  State.WaitingForFile=false; ++State.View; State.SettledAt=-1.;
 }
 if(State.View>=UE_ARRAY_COUNT(ShotFiles)) { Finish(true,TEXT("4 fresh UI and first-person weapon views rendered")); return; }
 if(State.SettledAt<0.) {
  if(State.View==0) {
   if(!PC->GetBuyMenu()->IsOpen()) PC->GetBuyMenu()->Toggle();
  } else if(State.View==1) {
   PS->AwardCredits(6200);
   const bool BoughtAUG=PS->TryPurchase(TEXT("AUG"),31001);
   const bool BoughtHeavy=PS->TryPurchase(TEXT("ArmorHeavy"),31002);
   if(!BoughtAUG || !BoughtHeavy) { Finish(false,TEXT("diagnostic AUG/heavy purchases were rejected")); return; }
   Pawn->SyncEconomyLoadout(false);
   if(!PC->GetBuyMenu()->IsOpen()) PC->GetBuyMenu()->Toggle();
  } else {
   PC->GetBuyMenu()->Close();
   if(State.View==3) {
    Mode->Phase=EVanthexRoundPhase::BuyPhase; Mode->TimeRemaining=25.f; Mode->PublishMatch();
    if(!PS->TryPurchase(TEXT("MP7"),31003)) { Finish(false,TEXT("diagnostic MP7 purchase was rejected")); return; }
    Pawn->SyncEconomyLoadout(false);
   }
   Mode->ActivateRound();
   Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
   PC->SetViewTarget(Pawn); PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
  }
  State.SettledAt=Now;
  UE_LOG(LogVanthex,Display,TEXT("ECONOMY_SHOTS settling %s"),ShotFiles[State.View]);
  return;
 }
 if(Now-State.SettledAt<5.) return;
 const auto& Economy=PS->GetEconomy();
 const bool MenuVisible=PC->GetBuyMenu()->IsOpen();
 bool Correct=PC->GetViewTarget()==Pawn;
 if(State.View==0) Correct &= MenuVisible && Mode->IsBuyPhase() && Economy.Credits==800 && Economy.PrimaryWeapon.IsNone();
 if(State.View==1) Correct &= MenuVisible && Mode->IsBuyPhase() && Economy.PrimaryWeapon==TEXT("AUG") && Economy.ArmorTier==EVanthexArmorTier::Heavy && Economy.Credits==3350;
 if(State.View>=2) Correct &= !MenuVisible && Mode->IsRoundActive() && Pawn->GetWeapon()->GetWeaponId()==(State.View==2?FName(TEXT("AUG")):FName(TEXT("MP7")));
 if(!Correct) { Finish(false,FString::Printf(TEXT("view %d real UI/weapon state did not settle"),State.View+1)); return; }
 State.Report+=FString::Printf(TEXT("VIEW %s credits=%d primary=%s equipped=%s armor=%.0f menu=%d ownPawnView=%d\n"),ShotFiles[State.View],Economy.Credits,
  *Economy.PrimaryWeapon.ToString(),*Pawn->GetWeapon()->GetWeaponId().ToString(),Economy.ArmorPoints,MenuVisible,PC->GetViewTarget()==Pawn);
 const FString File=Directory/ShotFiles[State.View];
 // Remove only the exact previous image owned by this diagnostic; a stale capture cannot satisfy it.
 IFileManager::Get().Delete(*File,false,true);
 State.Files.Add(File); State.WaitingForFile=true;
 FScreenshotRequest::RequestScreenshot(File,true,false);
#endif
}
