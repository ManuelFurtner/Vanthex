#include "VanthexGameMode.h"
#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexBuyMenuComponent.h"
#include "VanthexPlayerController.h"
#include "VanthexMapLayout.h"
#include "HitscanWeapon.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
namespace {
struct FAimShots {
 TWeakObjectPtr<UWorld> World;
 double Start=0,Pose=-1; int32 Index=0;
 bool Initialized=false,RequestedAim=false,Pending=false,Done=false;
 FString Report; TArray<FString> Files;
};
const TCHAR* AimModels[]={TEXT("M4"),TEXT("M4"),TEXT("MP7"),TEXT("Pistol"),TEXT("MP5"),TEXT("AK"),TEXT("Shotgun"),TEXT("AUG"),TEXT("DMR")};
}
#endif

void VanthexRunAimShots(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode||!Mode->HasAuthority()) return;
 auto* World=Mode->GetWorld(); static FAimShots S;
 const double Now=FPlatformTime::Seconds(); const FString Dir=FPaths::ProjectSavedDir()/TEXT("AimShots");
 if(S.World.Get()!=World) { S=FAimShots(); S.World=World; S.Start=Now; IFileManager::Get().MakeDirectory(*Dir,true); }
 if(S.Done) return;
 auto Finish=[&](bool OK,const FString& Reason) {
  S.Done=true; FFileHelper::SaveStringToFile((OK?TEXT("PASS "):TEXT("FAIL "))+Reason+TEXT("\n")+S.Report+FString::Join(S.Files,TEXT("\n")),*(Dir/TEXT("capture.txt")));
  UE_LOG(LogVanthex,Display,TEXT("AIM_SHOTS %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),*Reason);
  World->GetTimerManager().ClearTimer(Mode->VisualShotTimer); FPlatformMisc::RequestExitWithStatus(false,OK?0:1);
 };
 if(Now-S.Start>90) { Finish(false,TEXT("90-second capture timeout")); return; }
 auto* Pawn=Mode->FindCombatant(EVanthexTeam::Alpha,0);
 auto* PC=Pawn?Cast<AVanthexPlayerController>(Pawn->GetController()):nullptr;
 if(!Pawn||!PC||!PC->IsLocalController()||!Pawn->GetWeapon()||!PC->GetHUD()) return;
 if(!S.Initialized) {
  if(Mode->IsBuyPhase()) Mode->ActivateRound();
  if(!Mode->IsRoundActive()) return;
  World->GetTimerManager().ClearTimer(Mode->FlowTimer);
  PC->GetBuyMenu()->Close(); PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true); PC->GetHUD()->bShowHUD=true;
  for(TActorIterator<AVanthexCharacter> It(World);It;++It) {
   It->GetCharacterMovement()->StopMovementImmediately(); It->GetCharacterMovement()->DisableMovement();
   if(*It!=Pawn) { It->SetActorHiddenInGame(true); if(It->GetWeapon()) It->GetWeapon()->SetActorHiddenInGame(true); }
  }
  if(VanthexMap::IsGridV3(World)) Pawn->SetActorLocationAndRotation(FVector(-3600,-2400,98),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
  PC->SetControlRotation(FRotator(0,0,0)); PC->SetViewTarget(Pawn);
  S.Initialized=true;
 }
 if(FScreenshotRequest::IsScreenshotRequested()) return;
 if(S.Pending) {
  if(IFileManager::Get().FileSize(*S.Files.Last())<=0) return;
  S.Pending=false; ++S.Index; S.Pose=-1; S.RequestedAim=false;
 }
 if(S.Index>=UE_ARRAY_COUNT(AimModels)) { Finish(true,TEXT("9 fresh own-pawn hip, iron, reflex and scope views")); return; }
 const FName Id(AimModels[S.Index]);
 if(S.Pose<0) {
  Pawn->SetAiming(false); Pawn->SetSprinting(false); Pawn->GetWeapon()->CancelReload();
  Pawn->GetEquipment()->SyncLoadout(Id==TEXT("Pistol")?NAME_None:Id,TEXT("Pistol"),true);
  Pawn->GetEquipment()->SelectRoundWeapon(); PC->SetViewTarget(Pawn);
  S.Pose=Now; return;
 }
 if(Now-S.Pose>1&&!S.RequestedAim) {
  Pawn->GetFirstPersonCamera()->SetFieldOfView(90);
  Pawn->SetAiming(S.Index>0); S.RequestedAim=true;
 }
 if(Now-S.Pose<2.5) return;
 if(Pawn->GetWeapon()->GetWeaponId()!=Id||PC->GetViewTarget()!=Pawn||(S.Index>0&&(!Pawn->IsAuthoritativelyAiming()||Pawn->GetAimBlend()<.99f))) { Finish(false,TEXT("actual equipment/view/ADS did not settle")); return; }
 const FString File=Dir/FString::Printf(TEXT("%02d_%s_%s.png"),S.Index+1,AimModels[S.Index],S.Index?TEXT("sight"):TEXT("hip"));
 IFileManager::Get().Delete(*File,false,true); S.Files.Add(File); S.Pending=true;
 S.Report+=FString::Printf(TEXT("%s ownPawn=1 ads=%d alpha=%.3f fov=%.2f\n"),*Id.ToString(),Pawn->IsAiming(),Pawn->GetAimBlend(),Pawn->GetFirstPersonCamera()->FieldOfView);
 FScreenshotRequest::RequestScreenshot(File,true,false);
#endif
}
