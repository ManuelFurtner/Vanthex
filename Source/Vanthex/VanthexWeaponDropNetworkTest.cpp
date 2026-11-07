#include "VanthexPlayerController.h"
#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexLocalSettings.h"
#include "VanthexPlayerState.h"
#include "VanthexGameState.h"
#include "VanthexBotController.h"
#include "VanthexWeaponPickup.h"
#include "HitscanWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#endif

void VanthexStartWeaponDropNetworkTest(AVanthexPlayerController* PC)
{
#if !UE_BUILD_SHIPPING
 if(!PC||!PC->IsLocalController())return;
 struct FProbe {FTimerHandle Timer; double Started=0,Next=0;int32 Step=0;bool SawVisual=false;TWeakObjectPtr<AVanthexCharacter> Pawn;};
 auto Probe=MakeShared<FProbe>();Probe->Started=FPlatformTime::Seconds();
 TWeakObjectPtr<AVanthexPlayerController> Weak(PC);
 PC->GetWorldTimerManager().SetTimer(Probe->Timer,FTimerDelegate::CreateLambda([Weak,Probe]() {
  if(!Weak.IsValid())return;
  auto* Controller=Weak.Get();auto* World=Controller->GetWorld();const bool Host=Controller->HasAuthority();
  const double Now=FPlatformTime::Seconds();
  auto Finish=[&](bool Passed) {
   const FString Result=FString::Printf(TEXT("%s WeaponDropNetwork %s: owner RPC, replicated pickup, exact ammo, no duplicate"),Passed?TEXT("PASS"):TEXT("FAIL"),Host?TEXT("host"):TEXT("client"));
   FFileHelper::SaveStringToFile(Result,*(FPaths::ProjectSavedDir()/(Host?TEXT("WeaponDropNetworkHost.txt"):TEXT("WeaponDropNetworkClient.txt"))));
   UE_LOG(LogVanthex,Display,TEXT("%s"),*Result);World->GetTimerManager().ClearTimer(Probe->Timer);FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
  };
  if(Now-Probe->Started>45){Finish(false);return;}
  if(Now<Probe->Next)return;
  if(!Probe->Pawn.IsValid()) {
   if(!Host)Probe->Pawn=Cast<AVanthexCharacter>(Controller->GetPawn());
   else for(TActorIterator<AVanthexPlayerController> It(World);It;++It) if(!It->IsLocalController()){Probe->Pawn=Cast<AVanthexCharacter>(It->GetPawn());break;}
  }
  auto* Pawn=Probe->Pawn.Get();auto* GS=World->GetGameState<AVanthexGameState>();
  if(!Pawn||!Pawn->GetWeapon()||!GS||GS->GetPhase()!=EVanthexRoundPhase::RoundActive)return;
  auto* E=Pawn->GetEquipment();auto* W=Pawn->GetWeapon();
  AVanthexWeaponPickup* Drop=nullptr;
  for(TActorIterator<AVanthexWeaponPickup> It(World);It;++It)if(It->GetWeaponData().Id==TEXT("P250")){Drop=*It;break;}
  if(Host) {
   if(Probe->Step==0) {
    AVanthexWeaponPickup::ClearForRound(World);
    int32 Index=0;for(TActorIterator<AVanthexCharacter> It(World);It;++It){
     if(auto* Bot=Cast<AVanthexBotController>(It->GetController()))Bot->SetActorTickEnabled(false);
     It->GetCharacterMovement()->DisableMovement();It->GetWeapon()->StopFire();
     It->SetActorLocation(FVector(Index++*600,0,12000),false,nullptr,ETeleportType::TeleportPhysics);
    }
    Pawn->GetController()->SetControlRotation(FRotator::ZeroRotator);Controller->ClientSetRotation(FRotator::ZeroRotator,true);
    E->SyncLoadout(NAME_None,TEXT("P250"),true);E->SelectRoundWeapon();W->SetMagazineAmmo(4);W->SetReserveAmmo(11);
    Probe->Step=1;return;
   }
   if(Drop) {
    Drop->FindComponentByClass<UProjectileMovementComponent>()->StopMovementImmediately();
    Drop->SetActorLocation(Pawn->GetPawnViewLocation()+FVector(80,0,0),false,nullptr,ETeleportType::TeleportPhysics);Drop->ForceNetUpdate();
    if(Drop->GetWeaponData().Magazine!=4||Drop->GetWeaponData().Reserve!=11){Finish(false);return;}
    Probe->Step=2;
   }
   if(Probe->Step==2&&!Drop&&W->GetWeaponId()==TEXT("P250")&&W->GetAmmoInMagazine()==4&&W->GetReserveAmmo()==11&&Pawn->GetPlayerState<AVanthexPlayerState>()->GetEconomy().Sidearm==TEXT("P250")){Probe->Step=3;Probe->Next=Now+4;return;}
   if(Probe->Step==3)Finish(true);
  } else {
   const auto* Settings=UVanthexLocalSettings::Get();
   const FKey DropKey=Settings->GetGameplayKey(EVanthexGameplayAction::DropWeapon);
   const FKey PickupKey=Settings->GetGameplayKey(EVanthexGameplayAction::Pickup);
   if(Probe->Step==0&&W->GetWeaponId()==TEXT("P250")&&W->GetAmmoInMagazine()==4&&!E->IsSwitching()) {
    Controller->SetControlRotation(FRotator::ZeroRotator);
    Controller->InputKey(FInputKeyEventArgs::CreateSimulated(DropKey, IE_Pressed, 1.f));
    Probe->Step=1;return;
   }
   if(Probe->Step==1&&Drop) {
    TArray<UStaticMeshComponent*> Meshes;Drop->GetComponents(Meshes);
    Probe->SawVisual=!Meshes.IsEmpty();
    if(Drop->CanReach(Pawn)&&!E->HasSlot(EVanthexWeaponSlot::Pistol)&&!E->IsSwitching()) {
     Controller->InputKey(FInputKeyEventArgs::CreateSimulated(DropKey, IE_Released, 0.f));
     Controller->InputKey(FInputKeyEventArgs::CreateSimulated(PickupKey, IE_Pressed, 1.f));
     Probe->Step=2;return;
    }
   }
   if(Probe->Step==2&&!Drop&&W->GetWeaponId()==TEXT("P250")&&W->GetAmmoInMagazine()==4&&W->GetReserveAmmo()==11&&E->GetSlotWeaponId(EVanthexWeaponSlot::Pistol)==TEXT("P250")) {
    Controller->InputKey(FInputKeyEventArgs::CreateSimulated(PickupKey, IE_Released, 0.f));
    Finish(Probe->SawVisual);
   }
  }
 }),.05f,true);
#endif
}
