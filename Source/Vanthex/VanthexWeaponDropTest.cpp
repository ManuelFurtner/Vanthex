#include "VanthexGameMode.h"
#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexWeaponPickup.h"
#include "VanthexCharacter.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexPlayerState.h"
#include "VanthexBotController.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#endif

void VanthexRunWeaponDropTest(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if (!Mode || !Mode->HasAuthority()) return;
 static int32 Step=0,Checks=0,Failures=0;
 static double Next=0,Started=FPlatformTime::Seconds();
 auto* World=Mode->GetWorld();
 const double Now=FPlatformTime::Seconds();
 auto Finish=[&]() {
  const FString Result=FString::Printf(TEXT("%s WeaponDrop checks=%d failures=%d"),Failures?TEXT("FAIL"):TEXT("PASS"),Checks,Failures);
  UE_LOG(LogVanthex,Display,TEXT("%s"),*Result);
  FFileHelper::SaveStringToFile(Result,*(FPaths::ProjectSavedDir()/TEXT("WeaponDropTest.txt")));
  World->GetTimerManager().ClearTimer(Mode->SelfTestTimer);
  FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
 };
 auto Check=[&](bool OK,const TCHAR* Why) {++Checks;if(!OK)++Failures;UE_LOG(LogVanthex,Display,TEXT("WEAPON_DROP %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Why);};
 if(Now-Started>35) {Check(false,TEXT("timeout"));Finish();return;}
 if(Now<Next || Mode->Combatants.Num()!=Mode->TeamSize()*2) return;
 auto* A=Mode->FindCombatant(EVanthexTeam::Alpha,0);
 auto* Ally=Mode->FindCombatant(EVanthexTeam::Alpha,1);
 auto* B=Mode->FindCombatant(EVanthexTeam::Bravo,0);
 auto* BAlly=Mode->FindCombatant(EVanthexTeam::Bravo,1);
 if(!A||!Ally||!B||!BAlly||!A->GetWeapon()) return;
 auto Drops=[&]() {TArray<AVanthexWeaponPickup*> Found;for(TActorIterator<AVanthexWeaponPickup> It(World);It;++It)if(!It->IsActorBeingDestroyed())Found.Add(*It);return Found;};
 auto Find=[&](FName Id)->AVanthexWeaponPickup* {for(auto* Drop:Drops())if(Drop->GetWeaponData().Id==Id)return Drop;return nullptr;};
 auto Place=[&](AVanthexWeaponPickup* Drop,AVanthexCharacter* Pawn) {
  if(!Drop)return;
  Drop->FindComponentByClass<UProjectileMovementComponent>()->StopMovementImmediately();
  Pawn->GetController()->SetControlRotation(FRotator::ZeroRotator);
  Drop->SetActorLocation(Pawn->GetPawnViewLocation()+FVector(80,0,0),false,nullptr,ETeleportType::TeleportPhysics);
 };
 auto* E=A->GetEquipment();
 auto* W=A->GetWeapon();
 switch(Step++) {
 case 0: {
  World->GetTimerManager().ClearTimer(Mode->FlowTimer);
  AVanthexWeaponPickup::ClearForRound(World);
  Mode->Phase=EVanthexRoundPhase::RoundActive;
  int32 I=0;
  for(AVanthexCharacter* C:Mode->Combatants) {
   if(auto* Bot=Cast<AVanthexBotController>(C->GetController()))Bot->SetActorTickEnabled(false);
   C->GetCharacterMovement()->DisableMovement();C->GetWeapon()->StopFire();
   C->SetActorLocation(FVector(I++*600,0,10000),false,nullptr,ETeleportType::TeleportPhysics);
   C->GetController()->SetControlRotation(FRotator::ZeroRotator);
  }
  E->SyncLoadout(TEXT("M4"),TEXT("P250"),true);E->SelectRoundWeapon();
  W->SetMagazineAmmo(5);W->SetReserveAmmo(23);W->StartReload();
  Check(W->IsReloading(),TEXT("reload fixture active"));
  A->DropSelectedWeapon();
  auto* Drop=Find(TEXT("M4"));
  Check(Drops().Num()==1&&Drop&&Drop->GetWeaponData().Magazine==5&&Drop->GetWeaponData().Reserve==23,TEXT("manual throw preserves ammunition during reload"));
  Check(!W->IsReloading()&&!E->HasSlot(EVanthexWeaponSlot::Rifle)&&A->GetPlayerState<AVanthexPlayerState>()->GetEconomy().PrimaryWeapon.IsNone(),TEXT("removed weapon cannot remain in inventory"));
  Check(!E->DropSelectedWeapon()&&Drops().Num()==1,TEXT("rapid duplicate throw rejected"));
  if(Drop){
   Check(!Drop->TryPickup(B),TEXT("distant pickup rejected"));
   Place(Drop,B);
   B->GetController()->SetControlRotation(FRotator(0,180,0));
   Check(!Drop->TryPickup(B),TEXT("pickup behind view rejected"));
   B->GetController()->SetControlRotation(FRotator::ZeroRotator);
   auto* Wall=World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Box);Wall->AddInstanceComponent(Box);
   Box->SetBoxExtent(FVector(8,80,80));Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();
   Wall->SetActorLocation(B->GetPawnViewLocation()+FVector(40,0,0));
   Check(!Drop->TryPickup(B),TEXT("pickup through wall rejected"));Wall->Destroy();
   B->GetEquipment()->SyncLoadout(TEXT("AK"),TEXT("Pistol"),true);B->GetEquipment()->SelectRoundWeapon();
   B->GetWeapon()->SetMagazineAmmo(2);B->GetWeapon()->SetReserveAmmo(9);
   Check(Drop->TryPickup(B),TEXT("enemy can claim thrown weapon"));
   Check(!Drop->TryPickup(Ally),TEXT("consumed drop cannot be claimed twice"));
   Check(B->GetWeapon()->GetWeaponId()==TEXT("M4")&&B->GetWeapon()->GetAmmoInMagazine()==5&&B->GetWeapon()->GetReserveAmmo()==23,TEXT("pickup equips exact ammunition"));
   auto* Swapped=Find(TEXT("AK"));
   Check(Swapped&&Swapped->GetWeaponData().Magazine==2&&Swapped->GetWeaponData().Reserve==9,TEXT("occupied slot drops old gun with its ammunition"));
   if(Swapped){Place(Swapped,Ally);Check(Swapped->TryPickup(Ally),TEXT("another player can claim swapped weapon"));}
  }
  Next=Now+.7;return;
 }
 case 1:
  W->SetMagazineAmmo(0);W->SetReserveAmmo(0);
  Check(E->DropSelectedWeapon(),TEXT("empty sidearm can be thrown"));
  Check(E->GetSelectedSlot()==EVanthexWeaponSlot::Knife&&!E->HasSlot(EVanthexWeaponSlot::Pistol),TEXT("last gun falls back to knife"));
  A->SyncEconomyLoadout(false);E->SelectPistol();
  Check(!E->HasSlot(EVanthexWeaponSlot::Pistol)&&!E->DropSelectedWeapon(),TEXT("empty slot stays empty and knife cannot be thrown"));
  if(auto* Drop=Find(TEXT("P250")))Check(Drop->GetWeaponData().Magazine==0&&Drop->GetWeaponData().Reserve==0,TEXT("empty ammunition is not refilled"));else Check(false,TEXT("empty sidearm exists"));
  // Isolate death drops from prior swaps (CTF gives every player the same M4).
  AVanthexWeaponPickup::ClearForRound(World);
  B->GetEquipment()->SelectPistol();Next=Now+.7;return;
 case 2: {
  B->GetWeapon()->SetMagazineAmmo(7);B->GetWeapon()->SetReserveAmmo(16);
  B->GetEquipment()->SelectRifle(); // Die before the delayed switch commits.
  const int32 Before=Drops().Num();
  FVanthexDamageRequest Damage;Damage.InstigatorActor=A;Damage.InstigatorController=A->GetController();Damage.Damage=10000;
  B->GetHealthComponent()->ApplyDamage(Damage);
  Check(B->GetHealthComponent()->IsDead()&&Drops().Num()==Before+2,TEXT("death during switch drops both carried guns"));
  B->GetEquipment()->DropWeaponsOnDeath();
  Check(Drops().Num()==Before+2&&!B->GetEquipment()->DropSelectedWeapon(),TEXT("repeated death cannot duplicate guns"));
  auto* Primary=Find(TEXT("M4"));auto* Sidearm=Find(TEXT("Pistol"));
  Check(Primary&&Primary->GetWeaponData().Magazine==5&&Primary->GetWeaponData().Reserve==23&&Sidearm&&Sidearm->GetWeaponData().Magazine==7&&Sidearm->GetWeaponData().Reserve==16,TEXT("death preserves active and holstered ammunition"));
  if(Primary){Place(Primary,B);Check(!Primary->TryPickup(B),TEXT("dead player cannot loot"));Place(Primary,BAlly);Check(Primary->TryPickup(BAlly),TEXT("teammate can loot fallen teammate"));}
  if(Sidearm){Place(Sidearm,A);Check(Sidearm->TryPickup(A),TEXT("enemy can loot fallen opponent"));}
  Next=Now+.7;return;
 }
 case 3: {
  Mode->Phase=EVanthexRoundPhase::MatchEnded;
  Check(!E->DropSelectedWeapon(),TEXT("post-match throwing rejected"));
  {FVanthexDroppedWeapon Fixture;Fixture.Id=TEXT("MP7");Fixture.Magazine=1;Fixture.Reserve=3;
   auto* Drop=AVanthexWeaponPickup::SpawnDrop(A,Fixture,false);Place(Drop,A);
   Check(Drop&&!Drop->TryPickup(A),TEXT("post-match pickup rejected"));}
  Mode->Phase=EVanthexRoundPhase::BuyPhase;
  Check(E->DropSelectedWeapon(),TEXT("buy phase allows sharing guns"));
  // A simple real collision surface verifies ballistic landing, without relying on a map room.
  auto* Floor=World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Floor);Floor->SetRootComponent(Box);Floor->AddInstanceComponent(Box);
  Box->SetBoxExtent(FVector(2000,2000,10));Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();
  Floor->SetActorLocation(A->GetActorLocation()-FVector(0,0,200));
  Next=Now+2;return;
 }
 case 4: {
  auto* Drop=Find(TEXT("Pistol"));
  if(Drop)UE_LOG(LogVanthex,Display,TEXT("WEAPON_DROP landing location=%s velocity=%s"),*Drop->GetActorLocation().ToString(),*Drop->GetVelocity().ToString());
  Check(Drop&&Drop->GetActorLocation().Z>=A->GetActorLocation().Z-185&&Drop->GetActorLocation().Z<A->GetActorLocation().Z-165,TEXT("thrown weapon lands on solid floor"));
  if(Drop&&World->GetNetMode()!=NM_DedicatedServer){TArray<UStaticMeshComponent*> Meshes;Drop->GetComponents(Meshes);Check(!Meshes.IsEmpty(),TEXT("drop builds visible weapon model"));}
  Mode->MatchWinner=EVanthexTeam::None;Mode->AlphaScore=Mode->BravoScore=0;Mode->BeginRound();
  Check(Drops().IsEmpty(),TEXT("new round removes all unclaimed weapons"));
  Check(B->GetEquipment()->HasSlot(EVanthexWeaponSlot::Pistol)&&B->GetEquipment()->HasSlot(EVanthexWeaponSlot::Rifle)==Mode->IsCTF(),TEXT("dead player respawns with mode-specific default loadout"));
  Finish();return;
 }
 default:Finish();return;
 }
#endif
}
