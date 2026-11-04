#include "VanthexGameMode.h"
#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexBotController.h"
#include "VanthexBuyMenuComponent.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexPlayerController.h"
#include "VanthexPlayerState.h"
#include "VanthexOperatorComponent.h"
#include "HitscanWeapon.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace {
struct FWeaponModelShotState {
 TWeakObjectPtr<UWorld> World; TWeakObjectPtr<ACameraActor> Camera;
 double Start=0,Settled=-1; int32 Index=0; bool Initialized=false,Pending=false,Done=false;
 TArray<FString> Files;
};
const TCHAR* ModelIds[]={TEXT("Pistol"),TEXT("P250"),TEXT("MP5"),TEXT("MP7"),TEXT("M4"),TEXT("AK"),TEXT("AUG"),TEXT("DMR"),TEXT("Shotgun"),TEXT("Knife"),TEXT("AUG"),TEXT("AUG")};
}
#endif

void VanthexRunWeaponModelShots(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode||!Mode->HasAuthority()) return;
 auto* World=Mode->GetWorld(); static FWeaponModelShotState S;
 const double Now=FPlatformTime::Seconds(); const FString Dir=FPaths::ProjectSavedDir()/TEXT("WeaponModelShots");
 if(S.World.Get()!=World) { S=FWeaponModelShotState(); S.World=World; S.Start=Now; IFileManager::Get().MakeDirectory(*Dir,true); }
 if(S.Done) return;
 auto Finish=[&](bool OK,const FString& Reason) {
  S.Done=true; FFileHelper::SaveStringToFile((OK?TEXT("PASS "):TEXT("FAIL "))+Reason+TEXT("\n")+FString::Join(S.Files,TEXT("\n")),*(Dir/TEXT("capture.txt")));
  FPlatformMisc::RequestExitWithStatus(false,OK?0:1);
 };
 if(Now-S.Start>120) { Finish(false,TEXT("120-second capture timeout")); return; }
 AVanthexPlayerController* PC=nullptr;
 for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It)
  if(auto* Candidate=Cast<AVanthexPlayerController>(It->Get());Candidate&&Candidate->IsLocalController()) { PC=Candidate; break; }
 auto* Human=PC?Cast<AVanthexCharacter>(PC->GetPawn()):nullptr;
 auto* Other=Mode->FindCombatant(EVanthexTeam::Alpha,1);
 if(!Human||!Other||!Human->GetWeapon()||!Other->GetWeapon()||!PC->GetBuyMenu()) return;
 if(!S.Initialized) {
  if(!Mode->IsBuyPhase()) return;
  Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
  PC->GetBuyMenu()->Close(); PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
  if(PC->GetHUD()) PC->GetHUD()->bShowHUD=false;
  for(TActorIterator<AVanthexCharacter> It(World);It;++It) {
   if(auto* Bot=Cast<AVanthexBotController>(It->GetController())) Bot->SetActorTickEnabled(false);
   It->GetCharacterMovement()->StopMovementImmediately(); It->SetActorHiddenInGame(*It!=Human);
  }
  Human->SetActorLocationAndRotation(FVector(0,-350,98),FRotator(0,-90,0),false,nullptr,ETeleportType::TeleportPhysics);
  PC->SetControlRotation(FRotator(0,-90,0)); PC->SetViewTarget(Human);
  S.Camera=World->SpawnActor<ACameraActor>(); S.Initialized=true;
 }
 if(FScreenshotRequest::IsScreenshotRequested()) return;
 if(S.Pending) {
  if(IFileManager::Get().FileSize(*S.Files.Last())<=0) return;
  S.Pending=false; ++S.Index; S.Settled=-1;
 }
 if(S.Index>=UE_ARRAY_COUNT(ModelIds)) { Finish(true,TEXT("12 fresh first-person and crouched-operator model views")); return; }
 const bool Third=S.Index>=10; auto* Pawn=Third?Other:Human; const FName Id(ModelIds[S.Index]);
 if(S.Settled<0) {
  Mode->Phase=EVanthexRoundPhase::BuyPhase; Mode->TimeRemaining=12; Mode->PublishMatch();
  auto* PS=Pawn->GetPlayerState<AVanthexPlayerState>(); if(!PS) return;
  PS->ResetEconomyForMatch(); PS->SetAlive(true); PS->AwardCredits(6200);
  if(Id!=TEXT("Pistol")&&Id!=TEXT("Knife")&&!PS->TryPurchase(Id,44001+S.Index)) { Finish(false,TEXT("purchase rejected")); return; }
  Pawn->SyncEconomyLoadout(true); Mode->ActivateRound(); Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
  Pawn->GetEquipment()->SelectRoundWeapon(); if(Id==TEXT("Knife")) Pawn->GetEquipment()->SelectKnife();
  if(Third) {
   Human->SetActorHiddenInGame(true); Other->SetActorHiddenInGame(false);
   Other->SetActorLocationAndRotation(FVector(0,-350,98),FRotator(0,-90,0),false,nullptr,ETeleportType::TeleportPhysics);
   Other->GetController()->SetControlRotation(FRotator(0,-90,0)); Other->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
   Other->SetCrouching(S.Index==11);
   const FVector Eye(-230,-570,S.Index==11?120:150),Target(0,-350,S.Index==11?96:129);
   S.Camera->SetActorLocationAndRotation(Eye,(Target-Eye).Rotation()); S.Camera->GetCameraComponent()->SetFieldOfView(48);
   PC->SetViewTarget(S.Camera.Get());
  } else { PC->SetViewTarget(Human); PC->SetControlRotation(FRotator(0,-90,0)); }
  S.Settled=Now; return;
 }
 if(Now-S.Settled<2.5) return;
 if(Pawn->GetWeapon()->GetWeaponId()!=Id) { Finish(false,TEXT("equipped model ID did not settle")); return; }
 const FString File=Dir/FString::Printf(TEXT("%02d_%s_%s.png"),S.Index+1,ModelIds[S.Index],Third?(S.Index==11?TEXT("crouch"):TEXT("operator")):TEXT("first_person"));
 IFileManager::Get().Delete(*File,false,true); S.Files.Add(File); S.Pending=true;
 UE_LOG(LogVanthex,Display,TEXT("WEAPON_MODEL_SHOT %s ownPawnView=%d crouched=%d"),*File,PC->GetViewTarget()==Human,Pawn->bIsCrouched);
 FScreenshotRequest::RequestScreenshot(File,false,false);
#endif
}
