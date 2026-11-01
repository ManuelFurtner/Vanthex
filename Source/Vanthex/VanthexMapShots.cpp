#include "VanthexGameMode.h"
#include "VanthexCharacter.h"
#include "HitscanWeapon.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "VanthexShiftDevice.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
void AVanthexGameMode::AdvanceMapShots()
{
 struct FView { FVector Eye,Target; const TCHAR* Name; };
 static const FView Views[]={
  {FVector(-12000,-14500,11000),FVector(0,0,0),TEXT("01_overview.png")},
  {FVector(-1300,-4250,175),FVector(0,-1000,280),TEXT("02_alpha_approach.png")},
  {FVector(-900,-1200,175),FVector(0,0,380),TEXT("03_reactor.png")},
  {FVector(-1600,-1800,570),FVector(600,500,450),TEXT("04_gallery.png")},
  {FVector(-4200,1200,180),FVector(-3500,2300,180),TEXT("05_transfer_a.png")},
  {FVector(3500,3400,180),FVector(2500,-1300,200),TEXT("06_east_route.png")},
  {FVector(-8200,-8100,1400),FVector(-7000,-6500,350),TEXT("07_outer_district.png")},
  {FVector(-6500,-6600,180),FVector(-6500,-6200,155),TEXT("08_power_terminal.png")}
 };
 auto* Human=GetTestCharacter(); if(!Human) return;
 if(VisualShotIndex==0 && FParse::Param(FCommandLine::Get(),TEXT("VanthexTerminalShot"))) VisualShotIndex=7;
 if(Human->GetWeapon()) Human->GetWeapon()->SetActorHiddenInGame(true);
 if(VisualShotIndex>=UE_ARRAY_COUNT(Views)) { if(!FScreenshotRequest::IsScreenshotRequested()) FPlatformMisc::RequestExitWithStatus(false,0); return; }
 const auto& View=Views[VisualShotIndex++];
 Human->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
 Human->SetActorLocation(View.Eye-FVector(0,0,64),false,nullptr,ETeleportType::TeleportPhysics);
 if(auto* PC=Cast<APlayerController>(Human->GetController())) { PC->SetControlRotation((View.Target-View.Eye).Rotation()); if(PC->GetHUD()) PC->GetHUD()->bShowHUD=false; }
 const FString Directory=FPaths::ProjectSavedDir()/TEXT("FoundryShots"); IFileManager::Get().MakeDirectory(*Directory,true);
 FScreenshotRequest::RequestScreenshot(Directory/View.Name,false,false);
}


