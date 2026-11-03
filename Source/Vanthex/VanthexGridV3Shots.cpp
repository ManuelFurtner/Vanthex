#include "VanthexGameMode.h"
#include "VanthexCharacter.h"
#include "VanthexGridV3Layout.h"
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
struct FGridV3View
{
 FVector Eye;
 FVector Target;
 const TCHAR* Filename;
 bool bFirstPerson = false;
};

const TArray<FGridV3View>& GridV3Views()
{
 // These are player-height views in the authored layout, not detached overview cameras.
 // A separate pose/settle/capture sequence lets streaming and Lumen react to each move.
 static const TArray<FGridV3View> Views = {
  {{-650,-500,165},{300,-300,180},TEXT("01_core_chamber.png")},
  {{-2160,-850,165},{-1650,-200,175},TEXT("02_server_room.png")},
  {{1800,-800,165},{2500,-300,180},TEXT("03_research_laboratory.png")},
  {GridV3::Rooms()[9].Center+FVector(0,-250,165),GridV3::Rooms()[9].Center+FVector(0,250,175),TEXT("04_power_distribution.png")},
  {{600,-1750,-435},{600,-500,-435},TEXT("05_maintenance_tunnel.png")},
  {{-950,3350,765},{800,3200,765},TEXT("06_upper_catwalk.png")},
  {{-700,-320,765},{700,-650,700},TEXT("07_observation_room.png")},
  {{-3750,1800,165},{-3000,2250,190},TEXT("08_extraction_a.png")},
  {{3350,-2630,165},{4000,-2000,190},TEXT("09_extraction_b.png")},
  {{-3750,-2400,165},{-1850,-2400,165},TEXT("10_alpha_approach.png")},
  {GridV3::Rooms()[11].Center+FVector(250,0,165),GridV3::Rooms()[10].Center+FVector(0,0,165),TEXT("11_bravo_approach.png")},
  {{-1800,-600,165},{700,-600,165},TEXT("12_connected_rooms.png")},
  {{0,-1250,165},{450,350,165},TEXT("13_first_person_core.png"),true},
  {{700,-240,765},{0,-800,350},TEXT("14_first_person_bridge.png"),true},
  {{-1700,-2100,-435},{-1700,-2650,-435},TEXT("15_generator_hall.png")},
  {GridV3::VerticalLinks()[0].Bottom+FVector(0,-180,165),(GridV3::VerticalLinks()[0].Bottom+GridV3::VerticalLinks()[0].Top)*.5f+FVector(0,0,65),TEXT("16_stair_01_bottom.png")},
  {GridV3::VerticalLinks()[0].Top+FVector(0,150,165),(GridV3::VerticalLinks()[0].Bottom+GridV3::VerticalLinks()[0].Top)*.5f+FVector(0,0,65),TEXT("17_stair_01_top.png")},
  {GridV3::VerticalLinks()[1].Bottom+FVector(0,-180,165),(GridV3::VerticalLinks()[1].Bottom+GridV3::VerticalLinks()[1].Top)*.5f+FVector(0,0,65),TEXT("18_stair_02_bottom.png")},
  {GridV3::VerticalLinks()[1].Top+FVector(0,150,165),(GridV3::VerticalLinks()[1].Bottom+GridV3::VerticalLinks()[1].Top)*.5f+FVector(0,0,65),TEXT("19_stair_02_top.png")},
  {GridV3::VerticalLinks()[2].Bottom+FVector(0,-180,165),(GridV3::VerticalLinks()[2].Bottom+GridV3::VerticalLinks()[2].Top)*.5f+FVector(0,0,65),TEXT("20_stair_03_bottom.png")},
  {GridV3::VerticalLinks()[2].Top+FVector(0,150,165),(GridV3::VerticalLinks()[2].Bottom+GridV3::VerticalLinks()[2].Top)*.5f+FVector(0,0,65),TEXT("21_stair_03_top.png")},
  {GridV3::VerticalLinks()[3].Bottom+FVector(0,-180,165),(GridV3::VerticalLinks()[3].Bottom+GridV3::VerticalLinks()[3].Top)*.5f+FVector(0,0,65),TEXT("22_stair_04_bottom.png")},
  {GridV3::VerticalLinks()[3].Top+FVector(0,150,165),(GridV3::VerticalLinks()[3].Bottom+GridV3::VerticalLinks()[3].Top)*.5f+FVector(0,0,65),TEXT("23_stair_04_top.png")}
 };
 return Views;
}

struct FGridV3ShotRun
{
 TWeakObjectPtr<UWorld> World;
 double PoseSeconds = 0.;
 uint64 PoseFrame = 0;
 int32 SingleShot = 0;
 TArray<FString> RequestedFiles;
 FString TimingCsv;
};
}

void AVanthexGameMode::AdvanceGridV3Shots()
{
 static FGridV3ShotRun Run;
 auto* Human = GetTestCharacter();
 auto* World = GetWorld();
 if (!Human || !World || FScreenshotRequest::IsScreenshotRequested()) return;

 const FString Directory = FPaths::ProjectSavedDir() / TEXT("GridV3Shots");
 const auto& Views = GridV3Views();
 if (Run.World.Get() != World)
 {
  Run = FGridV3ShotRun();
  Run.World = World;
  FParse::Value(FCommandLine::Get(), TEXT("GridV3Shot="), Run.SingleShot);
  Run.SingleShot = FMath::Clamp(Run.SingleShot, 0, Views.Num());
  const int32 FirstView = FParse::Param(FCommandLine::Get(), TEXT("GridV3StairShots")) ? 15 : 0;
  VisualShotIndex = Run.SingleShot > 0 ? (Run.SingleShot - 1) * 3 : FirstView * 3;
  IFileManager::Get().MakeDirectory(*Directory, true);
  Run.TimingCsv = TEXT("View,Filename,SettlingSeconds,Frames,MeanFrameMs,Eye,Target\n");
  // A complete camera tour is longer than a normal round. Keep this diagnostic
  // session stable; normal match timing remains unchanged outside -GridV3Shots.
  TimeRemaining = 600.f;
  RoundDuration = 600.f;
  Human->SetHoldForward(false);
  Human->SetSprinting(false);
  Human->SetActorEnableCollision(false);
  UE_LOG(LogTemp, Display, TEXT("GRID V3 SHOTS: %d views; 8s settle per camera. Frame intervals describe an uncontested capture session, not a combat benchmark."), Run.SingleShot ? 1 : Views.Num() - FirstView);
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
    UE_LOG(LogTemp, Error, TEXT("GRID V3 SHOTS: missing screenshot %s"), *File);
   }
  }
  FFileHelper::SaveStringToFile(Run.TimingCsv, *(Directory / TEXT("capture_timing.csv")));
  GetWorldTimerManager().ClearTimer(VisualShotTimer);
  UE_LOG(LogTemp, Display, TEXT("GRID V3 SHOTS: complete; %d requested files; result %s"), Run.RequestedFiles.Num(), bMissing ? TEXT("FAILED") : TEXT("OK"));
  FPlatformMisc::RequestExitWithStatus(false, bMissing ? 1 : 0);
  return;
 }

 const FGridV3View& View = Views[ViewIndex];
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
  UE_LOG(LogTemp, Display, TEXT("GRID V3 SHOTS: settle %02d %s eye=%s"), ViewIndex + 1, View.Filename, *View.Eye.ToString());
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
  UE_LOG(LogTemp, Display, TEXT("GRID V3 SHOTS: capture %02d %s; %.2fs / %llu frames / %.2fms mean (settling window)"), ViewIndex + 1, View.Filename, Elapsed, static_cast<unsigned long long>(Frames), MeanMs);
 }
 ++VisualShotIndex;
}
