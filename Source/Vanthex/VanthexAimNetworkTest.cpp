#include "VanthexPlayerController.h"
#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexBuyMenuComponent.h"
#include "VanthexEquipmentComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
namespace {
struct FAimNetworkProbe {
 FTimerHandle Timer; TWeakObjectPtr<AVanthexCharacter> Pawn;
 double Started=0,Ready=-1;
 bool LastAim=false,SawServerAim=false,SawSlow=false,SawSprintCancellation=false,Done=false;
 int32 Stage=0,Rises=0,Falls=0; bool PredictionAccepted=false,PredictionReleased=false;
};
}
#endif

void VanthexStartAimNetworkTest(AVanthexPlayerController* PC)
{
#if !UE_BUILD_SHIPPING
 if(!PC||!PC->IsLocalController()||!PC->GetWorld()) return;
 const bool Host=PC->HasAuthority(); auto S=MakeShared<FAimNetworkProbe>(); S->Started=FPlatformTime::Seconds();
 TWeakObjectPtr<AVanthexPlayerController> WeakPC=PC;
 PC->GetWorld()->GetTimerManager().SetTimer(S->Timer,FTimerDelegate::CreateLambda([S,WeakPC,Host]() {
  const auto Lifetime=S; // Clearing this timer may destroy its delegate during Finish.
  auto* OwnerPC=WeakPC.Get(); if(!OwnerPC||S->Done) return;
  auto* World=OwnerPC->GetWorld(); const double Now=FPlatformTime::Seconds();
  auto Finish=[&](bool Timeout) {
   S->Done=true; const bool Pass=!Timeout&&S->Rises>=2&&S->Falls>=2&&S->SawServerAim&&S->SawSlow&&S->SawSprintCancellation&&(Host||(S->PredictionAccepted&&S->PredictionReleased));
   const FString Report=FString::Printf(TEXT("%s role=%s client_driven=1 rises=%d falls=%d authoritative_aim=%d slowed=%d sprint_cancel=%d prediction_enter=%d prediction_release=%d timeout=%d\n"),Pass?TEXT("PASS"):TEXT("FAIL"),Host?TEXT("host"):TEXT("client"),S->Rises,S->Falls,S->SawServerAim,S->SawSlow,S->SawSprintCancellation,S->PredictionAccepted,S->PredictionReleased,Timeout);
   const FString File=FPaths::ProjectSavedDir()/(Host?TEXT("AimNetworkHost.txt"):TEXT("AimNetworkClient.txt"));
   const bool Saved=FFileHelper::SaveStringToFile(Report,*File); UE_LOG(LogVanthex,Display,TEXT("AIM_NETWORK %s"),*Report);
   World->GetTimerManager().ClearTimer(S->Timer); FPlatformMisc::RequestExitWithStatus(false,Pass&&Saved?0:1);
  };
  if(Now-S->Started>75) { Finish(true); return; }
  if(!S->Pawn.IsValid()) {
   if(!Host) S->Pawn=Cast<AVanthexCharacter>(OwnerPC->GetPawn());
   else for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It) {
    auto* Other=It->Get(); if(Other&&!Other->IsLocalController()) if(auto* Remote=Cast<AVanthexCharacter>(Other->GetPawn())) { S->Pawn=Remote; break; }
   }
  }
  auto* Pawn=S->Pawn.Get(); if(!Pawn||!Pawn->GetWeapon()||!Pawn->CanPerformRoundActions()||Pawn->GetEquipment()->IsSwitching()) return;
  if(S->Ready<0) { S->Ready=Now; if(!Host&&OwnerPC->GetBuyMenu()) OwnerPC->GetBuyMenu()->Close(); }
  const double Age=Now-S->Ready;
  if(!Host) {
   if(S->Stage==0&&Age>=1) { Pawn->SetAiming(true); S->PredictionAccepted=Pawn->IsAiming(); ++S->Stage; }
   else if(S->Stage==1&&Age>=3) { Pawn->SetAiming(false); S->PredictionReleased=!Pawn->IsAiming(); ++S->Stage; }
   else if(S->Stage==2&&Age>=4) { Pawn->SetAiming(true); ++S->Stage; }
   else if(S->Stage==3&&Age>=6) { Pawn->SetSprinting(true); ++S->Stage; }
  }
  const bool Aim=Host?Pawn->IsAuthoritativelyAiming():Pawn->IsAiming();
  if(Aim&&!S->LastAim) ++S->Rises;
  if(!Aim&&S->LastAim) ++S->Falls;
  S->LastAim=Aim;
  if(Pawn->IsAuthoritativelyAiming()) S->SawServerAim=true;
  if(Aim&&Pawn->GetCharacterMovement()->MaxWalkSpeed<400) S->SawSlow=true;
  if(S->Rises>=2&&!Aim&&Pawn->IsSprintWanted()&&Pawn->GetCharacterMovement()->MaxWalkSpeed>800) S->SawSprintCancellation=true;
  if(Age>=(Host?10.:9.)) Finish(false);
 }),.05f,true,.2f);
#endif
}
