#include "VanthexGameMode.h"

#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexBotController.h"
#include "VanthexCharacter.h"
#include "VanthexOperatorComponent.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexPlayerState.h"
#include "HitscanWeapon.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
struct FOperatorShotState
{
 TWeakObjectPtr<UWorld> World;
 TWeakObjectPtr<ACameraActor> Camera;
 TWeakObjectPtr<APlayerController> Viewer;
 TWeakObjectPtr<AVanthexCharacter> Alpha, Bravo;
 TArray<FString> Files;
 double Started=0., PoseTime=-1., StrikeTime=-1.;
 int32 View=0;
 bool WaitingForFile=false, Finished=false;
};
struct FOperatorView
{
 FVector Eye, Target;
 float FOV;
 const TCHAR* File;
};
const FOperatorView Views[] = {
 {FVector(140,-820,155),FVector(140,-350,105),52.f,TEXT("01_lineup.png")},
 {FVector(-390,-620,155),FVector(0,-350,105),45.f,TEXT("02_side.png")},
 {FVector(0,-510,168),FVector(0,-350,165),38.f,TEXT("03_helmet.png")},
 {FVector(140,-820,135),FVector(140,-350,95),52.f,TEXT("04_crouch.png")},
 {FVector(-260,-600,150),FVector(0,-350,125),42.f,TEXT("05_knife_ready.png")},
 {FVector(-260,-600,150),FVector(0,-350,125),42.f,TEXT("06_knife_strike.png")},
 {FVector::ZeroVector,FVector::ZeroVector,90.f,TEXT("07_first_person_knife.png")},
 {FVector::ZeroVector,FVector::ZeroVector,90.f,TEXT("08_first_person_rifle.png")}
};
}
#endif

// Explicit diagnostic only. Called every 0.1 seconds by -OperatorShots.
void VanthexRunOperatorShots(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode || !Mode->HasAuthority() || !Mode->GetWorld()) return;
 UWorld* World=Mode->GetWorld();
 static FOperatorShotState State;
 const double Now=World->GetTimeSeconds();
 const FString Directory=FPaths::ProjectSavedDir()/TEXT("OperatorShots");
 if(State.World.Get()!=World) { State=FOperatorShotState(); State.World=World; State.Started=Now; if(FParse::Param(FCommandLine::Get(),TEXT("OperatorFirstPersonShots"))) State.View=6; }
 if(State.Finished) return;
 if(Now-State.Started>90.)
 {
  State.Finished=true;
  UE_LOG(LogVanthex,Error,TEXT("OPERATOR_SHOTS timeout at view %d"),State.View+1);
  FPlatformMisc::RequestExitWithStatus(false,1);
  return;
 }
 if(!State.Camera.IsValid())
 {
  if(!Mode->IsRoundActive()) return;
  for(TActorIterator<AVanthexCharacter> It(World);It;++It)
  {
   const auto* PS=It->GetPlayerState<AVanthexPlayerState>();
   if(PS && PS->GetTeam()==EVanthexTeam::Alpha && PS->GetSpawnSlot()==1) State.Alpha=*It;
   if(PS && PS->GetTeam()==EVanthexTeam::Bravo && PS->GetSpawnSlot()==0) State.Bravo=*It;
   if(auto* PC=Cast<APlayerController>(It->GetController())) if(PC->IsLocalController()) State.Viewer=PC;
  }
  if(!State.Alpha.IsValid() || !State.Bravo.IsValid() || !State.Viewer.IsValid()) return;
  for(auto* Character:{State.Alpha.Get(),State.Bravo.Get()})
  {
   const auto* Operator=Character->FindComponentByClass<UVanthexOperatorComponent>();
   if(!Operator || !Operator->IsPresentationReady()) return;
  }
  for(TActorIterator<AVanthexCharacter> It(World);It;++It)
  {
   if(auto* Bot=Cast<AVanthexBotController>(It->GetController())) Bot->SetActorTickEnabled(false);
   It->GetCharacterMovement()->StopMovementImmediately();
   It->GetCharacterMovement()->DisableMovement();
   It->SetActorHiddenInGame(*It!=State.Alpha.Get() && *It!=State.Bravo.Get());
   if(auto* Weapon=It->GetWeapon()) { Weapon->StopFire(); Weapon->SetActorHiddenInGame(true); }
  }
  State.Alpha->SetActorLocationAndRotation(FVector(0,-350,98),FRotator(0,-90,0),false,nullptr,ETeleportType::TeleportPhysics);
  State.Bravo->SetActorLocationAndRotation(FVector(280,-350,98),FRotator(0,-90,0),false,nullptr,ETeleportType::TeleportPhysics);
  for(auto* Character:{State.Alpha.Get(),State.Bravo.Get()})
  {
   Character->GetController()->SetControlRotation(FRotator(0,-90,0));
   Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
   Character->SetCrouching(false);
  }
  auto* PC=State.Viewer.Get();
  PC->SetIgnoreMoveInput(true);
  PC->SetIgnoreLookInput(true);
  if(PC->GetHUD()) PC->GetHUD()->bShowHUD=false;
  State.Camera=World->SpawnActor<ACameraActor>();
  if(!State.Camera.IsValid()) return;
  PC->SetViewTarget(State.Camera.Get());
  IFileManager::Get().MakeDirectory(*Directory,true);
  UE_LOG(LogVanthex,Display,TEXT("OPERATOR_SHOTS %d actual operator/equipment views; 8 seconds settling each"),UE_ARRAY_COUNT(Views)-State.View);
 }
 if(FScreenshotRequest::IsScreenshotRequested()) return;
 if(State.WaitingForFile)
 {
  if(IFileManager::Get().FileSize(*State.Files.Last())<=0) return;
  State.WaitingForFile=false;
  ++State.View;
  State.PoseTime=-1.;
 }
 if(State.View>=UE_ARRAY_COUNT(Views))
 {
  State.Finished=true;
  FFileHelper::SaveStringToFile(FString::Printf(TEXT("PASS %d operator/equipment views rendered\n"),State.Files.Num())+FString::Join(State.Files,TEXT("\n"))+TEXT("\n"),*(Directory/TEXT("capture.txt")));
  UE_LOG(LogVanthex,Display,TEXT("OPERATOR_SHOTS complete; %d files"),State.Files.Num());
  FPlatformMisc::RequestExitWithStatus(false,0);
  return;
 }
 const auto& View=Views[State.View];
 if(State.PoseTime<0.)
 {
  State.Bravo->SetActorHiddenInGame(State.View==1 || State.View==2 || State.View>=4);
  State.Alpha->SetCrouching(State.View==3);
  if(State.View>=4 && State.View<7) State.Alpha->GetEquipment()->SelectKnife();
  if(State.View==7) State.Alpha->GetEquipment()->SelectRifle();
  State.Viewer->SetViewTarget(State.View>=6?static_cast<AActor*>(State.Alpha.Get()):static_cast<AActor*>(State.Camera.Get()));
  State.Camera->SetActorLocationAndRotation(View.Eye,(View.Target-View.Eye).Rotation());
  auto* Camera=State.Camera->GetCameraComponent();
  Camera->SetFieldOfView(View.FOV);
  Camera->bConstrainAspectRatio=false;
  Camera->PostProcessBlendWeight=1.f;
  Camera->PostProcessSettings.bOverride_AutoExposureMinBrightness=true;
  Camera->PostProcessSettings.bOverride_AutoExposureMaxBrightness=true;
  Camera->PostProcessSettings.AutoExposureMinBrightness=1.f;
  Camera->PostProcessSettings.AutoExposureMaxBrightness=1.f;
  State.PoseTime=Now;
  UE_LOG(LogVanthex,Display,TEXT("OPERATOR_SHOTS settling %s"),View.File);
  return;
 }
 if(Now-State.PoseTime<8.) return;
 if(State.View==5) {
  if(State.StrikeTime<0.) { State.Alpha->GetEquipment()->Attack(); State.StrikeTime=Now; return; }
  if(Now-State.StrikeTime<.15) return;
 }
 const FString File=Directory/View.File;
 // Remove only this diagnostic's previous image so an old capture cannot pass.
 IFileManager::Get().Delete(*File,false,true);
 State.Files.Add(File);
 State.WaitingForFile=true;
 FScreenshotRequest::RequestScreenshot(File,false,false);
 UE_LOG(LogVanthex,Display,TEXT("OPERATOR_SHOTS capture %s crouched=%d"),View.File,State.Alpha->bIsCrouched);
#endif
}
