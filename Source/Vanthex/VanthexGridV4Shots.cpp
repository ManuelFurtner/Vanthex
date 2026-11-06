#include "VanthexGameMode.h"
#include "VanthexBotController.h"
#include "VanthexCharacter.h"
#include "VanthexGridV4Layout.h"
#include "HitscanWeapon.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
struct FGridV4View
{
 FVector Eye;
 FVector Target;
 const TCHAR* Filename;
 bool bFirstPerson = false;
};

const TArray<FGridV4View>& GridV4Views()
{
 // These are player-height views in the authored layout, not detached overview cameras.
 // A separate pose/settle/capture sequence lets streaming and Lumen react to each move.
 static const TArray<FGridV4View> Views = {
  {{850,850,165},{-200,-200,650},TEXT("01_core_chamber.png")},
  {{-2900,500,165},{-2200,1400,170},TEXT("02_data_vault.png")},
  {{2250,-1450,165},{3100,-600,170},TEXT("03_research_laboratory.png")},
  {{2350,2700,165},{3500,3250,180},TEXT("04_power_distribution.png")},
  {{-5300,-3100,-435},{-4300,-2500,-430},TEXT("05_maintenance.png")},
  {{-4400,2600,765},{-3800,3000,765},TEXT("06_upper_gantry.png")},
  {{-650,150,765},{850,-350,500},TEXT("07_observation.png")},
  {{-4700,4500,765},{-3800,5100,770},TEXT("08_extraction_a.png")},
  {{4850,-4400,-435},{5850,-3700,-430},TEXT("09_extraction_b.png")},
  {{-5400,-1500,165},{-3000,-1200,165},TEXT("10_alpha_approach.png")},
  {{5400,1400,165},{3000,1200,165},TEXT("11_bravo_approach.png")},
  {{-1800,-400,165},{600,-400,175},TEXT("12_connected_rooms.png")},
  {{-850,-350,165},{350,0,165},TEXT("13_first_person_core.png"),true},
  {{1000,-600,765},{0,0,240},TEXT("14_first_person_bridge.png"),true},
  {GridV4::VerticalLinks()[0].Bottom+FVector(0,-180,165),(GridV4::VerticalLinks()[0].Bottom+GridV4::VerticalLinks()[0].Top)*.5f+FVector(0,0,65),TEXT("15_stair_1.png")},
  {GridV4::VerticalLinks()[1].Bottom+FVector(0,-180,165),(GridV4::VerticalLinks()[1].Bottom+GridV4::VerticalLinks()[1].Top)*.5f+FVector(0,0,65),TEXT("16_stair_2.png")},
  {GridV4::VerticalLinks()[2].Bottom+FVector(0,-180,165),(GridV4::VerticalLinks()[2].Bottom+GridV4::VerticalLinks()[2].Top)*.5f+FVector(0,0,65),TEXT("17_stair_3.png")},
  {GridV4::VerticalLinks()[3].Bottom+FVector(0,-180,165),(GridV4::VerticalLinks()[3].Bottom+GridV4::VerticalLinks()[3].Top)*.5f+FVector(0,0,65),TEXT("18_stair_4.png")},
  {GridV4::VerticalLinks()[4].Bottom+FVector(0,-180,165),(GridV4::VerticalLinks()[4].Bottom+GridV4::VerticalLinks()[4].Top)*.5f+FVector(0,0,65),TEXT("19_stair_5.png")},
  {GridV4::VerticalLinks()[5].Bottom+FVector(0,-180,165),(GridV4::VerticalLinks()[5].Bottom+GridV4::VerticalLinks()[5].Top)*.5f+FVector(0,0,65),TEXT("20_stair_6.png")},
  {{-250,-450,165},{150,200,180},TEXT("21_inner_containment.png")},
  {{-700,-2700,-435},{-700,-2400,-435},TEXT("22_closed_service_boundary.png")}
 };
 return Views;
}

const TArray<FGridV4View>& CTFArtViews()
{
 static const TArray<FGridV4View> Views = {
  {{850,-1200,195},{0,0,430},TEXT("01_forum.png")},
  {{0,-3040,230},{0,-4300,430},TEXT("02_castra_alpha.png")},
  {{0,3040,230},{0,4300,430},TEXT("03_castra_bravo.png")}
 };
 return Views;
}

struct FGridV4ShotRun
{
 TWeakObjectPtr<UWorld> World;
 double PoseSeconds = 0.;
 uint64 PoseFrame = 0;
 int32 SingleShot = 0;
 TArray<FString> RequestedFiles;
 FString TimingCsv;
};
}

void AVanthexGameMode::AdvanceGridV4Shots()
{
 static FGridV4ShotRun Run;
 auto* Human = GetTestCharacter();
 auto* World = GetWorld();
 if (!Human || !World || FScreenshotRequest::IsScreenshotRequested()) return;

 const bool bCTFArt = IsCTF() && FParse::Param(FCommandLine::Get(), TEXT("CTFArtShots"));
 const FString Directory = FPaths::ProjectSavedDir() / (bCTFArt ? TEXT("CTFArtShots") : TEXT("GridV4Shots"));
 const TCHAR* CaptureName = bCTFArt ? TEXT("CTF ART SHOTS") : TEXT("GRID V4 SHOTS");
 const auto& Views = bCTFArt ? CTFArtViews() : GridV4Views();
 if (Run.World.Get() != World)
 {
  Run = FGridV4ShotRun();
  Run.World = World;
  FParse::Value(FCommandLine::Get(), bCTFArt ? TEXT("CTFArtShot=") : TEXT("GridV4Shot="), Run.SingleShot);
  Run.SingleShot = FMath::Clamp(Run.SingleShot, 0, Views.Num());
  const int32 FirstView = !bCTFArt && FParse::Param(FCommandLine::Get(), TEXT("GridV4StairShots")) ? 14 : 0;
  VisualShotIndex = Run.SingleShot > 0 ? (Run.SingleShot - 1) * 3 : FirstView * 3;
  IFileManager::Get().MakeDirectory(*Directory, true);
  Run.TimingCsv = TEXT("View,Filename,SettlingSeconds,Frames,MeanFrameMs,Eye,Target\n");
  // Keep the capture session stable for the complete camera tour.
  TimeRemaining = 600.f;
  RoundDuration = 600.f;
  Human->SetHoldForward(false);
  Human->SetSprinting(false);
  Human->SetActorEnableCollision(false);
  if (bCTFArt)
  {
   for (TActorIterator<AVanthexBotController> It(World); It; ++It)
   {
    It->SetActorTickEnabled(false);
    if (auto* BotPawn = Cast<AVanthexCharacter>(It->GetPawn()); BotPawn && BotPawn->GetWeapon())
     BotPawn->GetWeapon()->StopFire();
   }
  }
  UE_LOG(LogTemp, Display, TEXT("%s: %d views; 8s settle per camera. Frame intervals describe an uncontested capture session, not a combat benchmark."), CaptureName, Run.SingleShot ? 1 : Views.Num() - FirstView);
 }

 const int32 ViewIndex = VisualShotIndex / 3;
 const int32 Stage = VisualShotIndex % 3;
 if (ViewIndex >= Views.Num() || (Run.SingleShot > 0 && ViewIndex >= Run.SingleShot))
 {
  bool bMissing = false;
  for (const FString& File : Run.RequestedFiles)
  {
   if (IFileManager::Get().FileSize(*File) <= 0)
   {
    bMissing = true;
    UE_LOG(LogTemp, Error, TEXT("%s: missing screenshot %s"), CaptureName, *File);
   }
  }
  FFileHelper::SaveStringToFile(Run.TimingCsv, *(Directory / TEXT("capture_timing.csv")));
  GetWorldTimerManager().ClearTimer(VisualShotTimer);
  UE_LOG(LogTemp, Display, TEXT("%s: complete; %d requested files; result %s"), CaptureName, Run.RequestedFiles.Num(), bMissing ? TEXT("FAILED") : TEXT("OK"));
  FPlatformMisc::RequestExitWithStatus(false, bMissing ? 1 : 0);
  return;
 }

 const FGridV4View& View = Views[ViewIndex];
 if (Stage == 0)
 {
  Human->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
  Human->GetCharacterMovement()->StopMovementImmediately();
  // FirstPersonCamera is 64cm above the capsule origin in VanthexCharacter.cpp.
  Human->SetActorLocation(View.Eye - FVector(0,0,64), false, nullptr, ETeleportType::TeleportPhysics);
  if (auto* PC = Cast<APlayerController>(Human->GetController()))
  {
   PC->SetViewTarget(Human);
   PC->SetControlRotation((View.Target - View.Eye).Rotation());
   PC->SetIgnoreMoveInput(true);
   PC->SetIgnoreLookInput(true);
   if (PC->GetHUD()) PC->GetHUD()->bShowHUD = View.bFirstPerson;
  }
  Human->SetActorHiddenInGame(!View.bFirstPerson);
  if (Human->GetWeapon()) Human->GetWeapon()->SetActorHiddenInGame(!View.bFirstPerson);
  // Scenic architecture views should not be obscured by the stationary test
  // roster at deployment. Combat viewpoints retain the normal visible roster.
  for (TActorIterator<AVanthexCharacter> It(World); It; ++It)
  {
   if (*It == Human) continue;
   It->SetActorHiddenInGame(!View.bFirstPerson);
   if (It->GetWeapon()) It->GetWeapon()->SetActorHiddenInGame(!View.bFirstPerson);
  }
  Run.PoseSeconds = FPlatformTime::Seconds();
  Run.PoseFrame = GFrameCounter;
  UE_LOG(LogTemp, Display, TEXT("%s: settle %02d %s eye=%s"), CaptureName, ViewIndex + 1, View.Filename, *View.Eye.ToString());
 }
 else if (Stage == 2)
 {
  const double Elapsed = FPlatformTime::Seconds() - Run.PoseSeconds;
  const uint64 Frames = GFrameCounter - Run.PoseFrame;
  const double MeanMs = Frames ? 1000. * Elapsed / double(Frames) : 0.;
  const FString File = Directory / View.Filename;
  Run.RequestedFiles.Add(File);
  Run.TimingCsv += FString::Printf(TEXT("%d,%s,%.3f,%llu,%.3f,%s,%s\n"), ViewIndex + 1, View.Filename, Elapsed, static_cast<unsigned long long>(Frames), MeanMs, *View.Eye.ToString(), *View.Target.ToString());
  FFileHelper::SaveStringToFile(Run.TimingCsv, *(Directory / TEXT("capture_timing.csv")));
  FScreenshotRequest::RequestScreenshot(File, false, false);
  UE_LOG(LogTemp, Display, TEXT("%s: capture %02d %s; %.2fs / %llu frames / %.2fms mean (settling window)"), CaptureName, ViewIndex + 1, View.Filename, Elapsed, static_cast<unsigned long long>(Frames), MeanMs);
 }
 ++VisualShotIndex;
}
