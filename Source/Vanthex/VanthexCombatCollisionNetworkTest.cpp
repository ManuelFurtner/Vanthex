#include "VanthexPlayerController.h"
#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexBotController.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
namespace {
struct FCombatCollisionProbe {
 FTimerHandle Timer; TWeakObjectPtr<AVanthexCharacter> Target;
 double Started=0,Ready=-1; int32 Stage=0,Deaths=0,Revivals=0;
 bool LastDead=false,SawAlive=false,CollisionCorrect=true,Done=false;
};
}
#endif

void VanthexStartCombatCollisionNetworkTest(AVanthexPlayerController* PC)
{
#if !UE_BUILD_SHIPPING
 if(!PC||!PC->IsLocalController()||!PC->GetWorld()) return;
 const bool Host=PC->HasAuthority(); auto State=MakeShared<FCombatCollisionProbe>(); State->Started=FPlatformTime::Seconds();
 TWeakObjectPtr<AVanthexPlayerController> WeakPC=PC;
 PC->GetWorld()->GetTimerManager().SetTimer(State->Timer,FTimerDelegate::CreateLambda([State,WeakPC,Host]() {
  const auto KeepAlive=State;
  auto* OwnerPC=WeakPC.Get(); if(!OwnerPC||State->Done) return;
  auto* World=OwnerPC->GetWorld(); const double Now=FPlatformTime::Seconds();
  auto Finish=[&](bool Timeout) {
   State->Done=true; const bool Pass=!Timeout&&State->SawAlive&&State->Deaths==2&&State->Revivals==2&&State->CollisionCorrect;
   const FString Report=FString::Printf(TEXT("%s role=%s deaths=%d revivals=%d initial_alive=%d collision_and_traces=%d timeout=%d\n"),
    Pass?TEXT("PASS"):TEXT("FAIL"),Host?TEXT("host"):TEXT("client"),State->Deaths,State->Revivals,State->SawAlive,State->CollisionCorrect,Timeout);
   const bool Saved=FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/(Host?TEXT("CombatCollisionNetworkHost.txt"):TEXT("CombatCollisionNetworkClient.txt"))));
   UE_LOG(LogVanthex,Display,TEXT("COMBAT_COLLISION_NETWORK %s"),*Report);
   World->GetTimerManager().ClearTimer(State->Timer); FPlatformMisc::RequestExitWithStatus(false,Pass&&Saved?0:1);
  };
  if(Now-State->Started>60) { Finish(true); return; }
  if(!State->Target.IsValid()) {
   if(!Host) State->Target=Cast<AVanthexCharacter>(OwnerPC->GetPawn());
   else for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It)
    if(auto* Other=It->Get();Other&&!Other->IsLocalController()) State->Target=Cast<AVanthexCharacter>(Other->GetPawn());
  }
  auto* Target=State->Target.Get(); if(!Target||!Target->GetHealthComponent()) return;
  if(State->Ready<0) {
   if(!Target->CanPerformRoundActions()) return;
   State->Ready=Now;
   if(Host) {
    for(TActorIterator<AVanthexCharacter> It(World);It;++It) {
     if(auto* Bot=Cast<AVanthexBotController>(It->GetController())) Bot->SetActorTickEnabled(false);
     if(It->GetWeapon()) It->GetWeapon()->StopFire();
     It->GetCharacterMovement()->StopMovementImmediately(); It->GetCharacterMovement()->DisableMovement();
    }
    Target->SetActorLocation(FVector(50000,0,5000),false,nullptr,ETeleportType::TeleportPhysics); Target->ForceNetUpdate();
   }
  }
  const double Age=Now-State->Ready;
  if(Host&&State->Stage<4&&Age>=2.+State->Stage*2.) {
   if(State->Stage%2==0) { FVanthexDamageRequest Fatal; Fatal.Damage=1000; Target->GetHealthComponent()->ApplyDamage(Fatal); }
   else Target->GetHealthComponent()->ResetHealth();
   ++State->Stage;
  }
  // Ignore the initial teleport replication window; test real client collision after arrival.
  if(Target->GetActorLocation().X>49000) {
   const bool Dead=Target->GetHealthComponent()->IsDead();
   if(Dead&&!State->LastDead) ++State->Deaths;
   if(!Dead&&State->LastDead) ++State->Revivals;
   if(!Dead&&State->Deaths==0) State->SawAlive=true;
   State->LastDead=Dead;
   // Trace the actual posed torso. The movement capsule centre can be empty
   // between articulated bodies, especially after a disconnected client becomes a bot.
   const auto* Body=Target->GetMesh()->GetBodyInstance(TEXT("pelvis"));
   FHitResult Hit; const FVector P=Body?Body->GetCOMPosition():Target->GetActorLocation();
   World->LineTraceSingleByChannel(Hit,P-FVector(150,0,0),P+FVector(150,0,0),ECC_Visibility);
   const bool Correct=Body&&Target->GetActorEnableCollision()!=Dead && (Dead?!Hit.bBlockingHit:Hit.GetActor()==Target&&Hit.GetComponent()==Target->GetMesh()&&!Hit.BoneName.IsNone());
   if(!Correct&&State->CollisionCorrect)UE_LOG(LogVanthex,Error,TEXT("COMBAT_COLLISION first mismatch age=%.3f dead=%d enabled=%d hit=%s bone=%s"),Age,Dead,Target->GetActorEnableCollision(),*GetNameSafe(Hit.GetActor()),*Hit.BoneName.ToString());
   State->CollisionCorrect &= Correct;
  }
  if(Host?Age>=12:(State->Revivals==2&&Age>=9)) Finish(false);
 }),.05f,true,.2f);
#endif
}
