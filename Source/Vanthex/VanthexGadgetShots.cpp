#include "VanthexGameMode.h"

#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexBuyMenuComponent.h"
#include "VanthexCharacter.h"
#include "VanthexGadgetActor.h"
#include "VanthexGadgetComponent.h"
#include "VanthexGadgetVisualComponent.h"
#include "VanthexMapLayout.h"
#include "VanthexPlayerController.h"
#include "HitscanWeapon.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
struct FGadgetShots
{
 TWeakObjectPtr<UWorld> World;
 TWeakObjectPtr<AVanthexGadgetActor> Smoke,Main,Sensor;
 FVector Anchor=FVector::ZeroVector;
 double Started=0.,PoseAt=-1.,ActionAt=-1.;
 int32 View=0;
 bool Initialized=false,Waiting=false,Finished=false;
 FString Report;
 TArray<FString> Files;
};
const TCHAR* Files[]={TEXT("01_gadget_bodies.png"),TEXT("02_smoke_outside.png"),TEXT("03_smoke_inside_hud.png"),
 TEXT("04_emp_sensor_pulses.png"),TEXT("05_flash_hud.png"),TEXT("06_frag_burst.png")};

AVanthexGadgetActor* Spawn(UWorld* World,AVanthexCharacter* Pawn,FName Id,FVector Position,float Scale=1.f)
{
 const FTransform Transform(FRotator::ZeroRotator,Position,FVector(Scale));
 auto* G=World->SpawnActorDeferred<AVanthexGadgetActor>(AVanthexGadgetActor::StaticClass(),Transform,Pawn,Pawn,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!G) return nullptr;
 G->Configure(Id,Pawn); G->FinishSpawning(Transform);
 G->SetActorTickEnabled(false); G->SetActorEnableCollision(false);
 if(auto* Flight=G->FindComponentByClass<UProjectileMovementComponent>()) { Flight->StopMovementImmediately(); Flight->Deactivate(); Flight->SetComponentTickEnabled(false); }
 return G;
}

void Pose(AVanthexPlayerController* PC,AVanthexCharacter* Pawn,FVector Eye,FVector Target,float FOV)
{
 Pawn->SetActorLocation(Eye-FVector(0,0,64),false,nullptr,ETeleportType::TeleportPhysics);
 PC->SetControlRotation((Target-Eye).Rotation()); PC->SetViewTarget(Pawn);
 Pawn->GetCharacterMovement()->StopMovementImmediately(); Pawn->GetCharacterMovement()->DisableMovement();
 if(auto* Camera=Pawn->GetFirstPersonCamera()) Camera->SetFieldOfView(FOV);
}
}
#endif

// Explicit rendered diagnostic. Requires GameMode friend and a 0.05-second -GadgetShots callback.
void VanthexRunGadgetShots(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode || !Mode->HasAuthority() || !Mode->GetWorld()) return;
 static FGadgetShots S;
 auto* World=Mode->GetWorld(); const double Now=FPlatformTime::Seconds();
 const FString Directory=FPaths::ProjectSavedDir()/TEXT("GadgetShots");
 if(S.World.Get()!=World) { S=FGadgetShots(); S.World=World; S.Started=Now; IFileManager::Get().MakeDirectory(*Directory,true); }
 if(S.Finished) return;
 auto Finish=[&](bool Pass,const FString& Why) {
  S.Finished=true;
  FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s %s\n"),Pass?TEXT("PASS"):TEXT("FAIL"),*Why)+S.Report+FString::Join(S.Files,TEXT("\n"))+TEXT("\n"),*(Directory/TEXT("capture.txt")));
  UE_LOG(LogVanthex,Display,TEXT("GADGET_SHOTS %s %s"),Pass?TEXT("PASS"):TEXT("FAIL"),*Why);
  World->GetTimerManager().ClearTimer(Mode->VisualShotTimer); FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
 };
 if(Now-S.Started>90.) { Finish(false,FString::Printf(TEXT("timeout at view %d"),S.View+1)); return; }
 AVanthexPlayerController* PC=nullptr;
 for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator(); It; ++It)
  if(auto* Candidate=Cast<AVanthexPlayerController>(It->Get()); Candidate && Candidate->IsLocalController()) { PC=Candidate; break; }
 auto* Pawn=PC?Cast<AVanthexCharacter>(PC->GetPawn()):nullptr;
 if(!Pawn || !Pawn->GetGadgets() || !PC->GetHUD()) return;
 if(!S.Initialized) {
  if(Mode->IsBuyPhase()) Mode->ActivateRound();
  if(!Mode->IsRoundActive()) return;
  World->GetTimerManager().ClearTimer(Mode->FlowTimer);
  if(PC->GetBuyMenu()) PC->GetBuyMenu()->Close();
  PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true); PC->GetHUD()->bShowHUD=true;
  for(TActorIterator<AVanthexCharacter> It(World); It; ++It) {
   It->GetCharacterMovement()->StopMovementImmediately(); It->GetCharacterMovement()->DisableMovement();
   if(*It!=Pawn) It->SetActorHiddenInGame(true);
   if(auto* Weapon=It->GetWeapon()) { Weapon->StopFire(); Weapon->SetActorHiddenInGame(true); }
  }
  S.Anchor=VanthexMap::IsGridV3(World)?FVector(-3600,-2400,0):Pawn->GetActorLocation()-FVector(0,0,96);
  S.Initialized=true;
 }
 if(FScreenshotRequest::IsScreenshotRequested()) return;
 if(S.Waiting) {
  if(IFileManager::Get().FileSize(*S.Files.Last())<=0) return;
  S.Waiting=false; ++S.View; S.PoseAt=-1.; S.ActionAt=-1.;
 }
 if(S.View>=UE_ARRAY_COUNT(Files)) { Finish(true,TEXT("six actual gadget/gameplay/HUD views rendered")); return; }
 if(S.PoseAt<0.) {
  if(S.View!=2) {
   AVanthexGadgetActor::ResetAll(World); S.Main.Reset(); S.Sensor.Reset(); S.Smoke.Reset();
   Pawn->GetGadgets()->ResetForRound();
  }
  if(S.View==0) {
   Pose(PC,Pawn,S.Anchor+FVector(-420,0,168),S.Anchor+FVector(-180,0,143),64.f);
   const FName Ids[]={TEXT("Flash"),TEXT("Smoke"),TEXT("Frag"),TEXT("EMP"),TEXT("Sensor")};
   for(int32 I=0; I<5; ++I) {
    const FVector P=S.Anchor+FVector(-180,(I-2)*54.,154);
    auto* G=Spawn(World,Pawn,Ids[I],P,3.f);
    if(!G) { Finish(false,TEXT("body lineup spawn failed")); return; }
    auto* Text=NewObject<UTextRenderComponent>(G); Text->SetupAttachment(G->GetRootComponent()); Text->SetAbsolute(true,true,true);
    Text->SetHorizontalAlignment(EHTA_Center); Text->SetVerticalAlignment(EVRTA_TextCenter); Text->SetWorldSize(10.f);
    Text->SetText(FText::FromName(Ids[I])); Text->SetTextRenderColor(FColor(180,224,244)); Text->RegisterComponent();
    Text->SetWorldLocationAndRotation(P+FVector(0,0,-38),FRotator(0,180,0));
   }
  } else if(S.View==1) {
   const FVector SmokePosition=S.Anchor+FVector(200,150,9);
   Pose(PC,Pawn,S.Anchor+FVector(-520,-400,295),SmokePosition+FVector(0,0,150),78.f);
   S.Smoke=Spawn(World,Pawn,TEXT("Smoke"),SmokePosition);
   if(!S.Smoke.IsValid()) { Finish(false,TEXT("smoke spawn failed")); return; }
   S.Smoke->Detonate();
  } else if(S.View==2) {
   if(!S.Smoke.IsValid() || !S.Smoke->IsSmokeActive()) { Finish(false,TEXT("inside smoke lost its real active actor")); return; }
   const FVector Eye=S.Smoke->GetActorLocation()+FVector(0,0,160);
   Pose(PC,Pawn,Eye,Eye+FVector(500,0,0),90.f);
  } else if(S.View==3) {
   Pose(PC,Pawn,S.Anchor+FVector(-500,-320,280),S.Anchor+FVector(150,0,35),88.f);
   S.Main=Spawn(World,Pawn,TEXT("EMP"),S.Anchor+FVector(150,-90,9));
  } else if(S.View==4) {
   const FVector Eye=S.Anchor+FVector(-360,0,185);
   Pose(PC,Pawn,Eye,Eye+FVector(500,0,0),78.f);
   S.Main=Spawn(World,Pawn,TEXT("Flash"),Eye+FVector(180,0,0));
  } else {
   const FVector Eye=S.Anchor+FVector(-360,0,185);
   Pose(PC,Pawn,Eye,Eye+FVector(750,0,0),57.f);
   S.Main=Spawn(World,Pawn,TEXT("Frag"),Eye+FVector(750,0,-25));
  }
  S.PoseAt=Now;
  UE_LOG(LogVanthex,Display,TEXT("GADGET_SHOTS settling %s"),Files[S.View]); return;
 }
 if(Now-S.PoseAt<4.) return;
 if(S.View>=3 && S.ActionAt<0.) {
  if(!S.Main.IsValid()) { Finish(false,TEXT("effect actor missing")); return; }
  S.Main->Detonate();
  if(S.View==3) {
   // Deploy after the EMP burst, so the new sensor is not already jammed by that burst.
   S.Sensor=Spawn(World,Pawn,TEXT("Sensor"),S.Anchor+FVector(150,90,9));
   if(!S.Sensor.IsValid()) { Finish(false,TEXT("sensor spawn failed")); return; }
   S.Sensor->Detonate(); S.Sensor->SetActorTickEnabled(true); // Its actual Scan emits the multicast pulse.
  }
  S.ActionAt=Now; return;
 }
 const double EffectWait=S.View==3?.28:(S.View==4?.12:.16);
 if(S.View>=3 && Now-S.ActionAt<EffectWait) return;
 bool Pass=PC->GetViewTarget()==Pawn;
 int32 SmokeVolumes=0; float Flash=Pawn->GetGadgets()->GetFlashOpacity(); uint32 Pulses=0;
 if(S.View==1 || S.View==2) {
  auto* V=S.Smoke.IsValid()?S.Smoke->FindComponentByClass<UVanthexGadgetVisualComponent>():nullptr;
  SmokeVolumes=V?V->GetSmokeInstanceCount():0;
  Pass &= V && S.Smoke->IsSmokeActive() && SmokeVolumes>0;
  if(S.View==2) Pass &= AVanthexGadgetActor::SmokeBlocks(World,Pawn->GetPawnViewLocation(),Pawn->GetPawnViewLocation()+FVector(100,0,0));
 }
 if(S.View==3) {
  auto* V=S.Sensor.IsValid()?S.Sensor->FindComponentByClass<UVanthexGadgetVisualComponent>():nullptr;
  Pulses=V?V->PulseCount:0; Pass &= S.Main->bActivated && S.Sensor.IsValid() && S.Sensor->IsSensorActive() && Pulses>0;
 }
 if(S.View==4) Pass &= Flash>.2f;
 if(S.View==5) Pass &= S.Main.IsValid() && S.Main->bActivated;
 if(!Pass) { Finish(false,FString::Printf(TEXT("view %d gameplay/presentation state invalid (flash %.3f smoke %d pulse %u)"),S.View+1,Flash,SmokeVolumes,Pulses)); return; }
 S.Report+=FString::Printf(TEXT("VIEW %s ownPawn=1 smokeVolumes=%d flashOpacity=%.3f sensorPulses=%u effectAge=%.3f\n"),Files[S.View],SmokeVolumes,Flash,Pulses,S.ActionAt>=0?Now-S.ActionAt:0.);
 const FString File=Directory/Files[S.View];
 IFileManager::Get().Delete(*File,false,true); S.Files.Add(File); S.Waiting=true;
 FScreenshotRequest::RequestScreenshot(File,true,false);
#endif
}
