#include "VanthexBotController.h"
#include "VanthexCharacter.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexGadgetComponent.h"
#include "VanthexGadgetActor.h"
#include "VanthexPlayerState.h"
#include "VanthexGameMode.h"
#include "VanthexCore.h"
#include "VanthexNavigation.h"
#include "VanthexShiftDevice.h"
#include "VanthexExtractionZone.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AVanthexBotController::AVanthexBotController()
{
 PrimaryActorTick.bCanEverTick=true;
}

void AVanthexBotController::PlanRoute(const FVector& Goal)
{
 if(GetPawn()) VanthexNavigation::FindRoute(GetWorld(),GetPawn()->GetActorLocation(),Goal,Route);
}
void AVanthexBotController::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 if(FParse::Param(FCommandLine::Get(),TEXT("AimTest"))||FParse::Param(FCommandLine::Get(),TEXT("AimShots"))||FParse::Param(FCommandLine::Get(),TEXT("AimNetworkTest")))return;
 if(FParse::Param(FCommandLine::Get(),TEXT("GadgetTest"))||FParse::Param(FCommandLine::Get(),TEXT("GadgetNetworkTest"))||FParse::Param(FCommandLine::Get(),TEXT("GadgetShots"))||FParse::Param(FCommandLine::Get(),TEXT("WeaponModelShots")))return;
 if(FParse::Param(FCommandLine::Get(),TEXT("EconomyShots")) || FParse::Param(FCommandLine::Get(),TEXT("WeaponProfileTest"))) return;
 if(FParse::Param(FCommandLine::Get(),TEXT("CoreEconomyTest"))) return;
 if(FParse::Param(FCommandLine::Get(),TEXT("EconomyTest")) || FParse::Param(FCommandLine::Get(),TEXT("EconomyNetworkTest")) || FParse::Param(FCommandLine::Get(),TEXT("WeaponAudioPlaybackTest"))) return;
 if(FParse::Param(FCommandLine::Get(),TEXT("EquipmentNetworkTest"))) return;
 if(FParse::Param(FCommandLine::Get(),TEXT("GridV3Test")) || FParse::Param(FCommandLine::Get(),TEXT("GridV3GameTest")) || FParse::Param(FCommandLine::Get(),TEXT("GridV3Shots")) || FParse::Param(FCommandLine::Get(),TEXT("BulletAudioNetworkTest")) || FParse::Param(FCommandLine::Get(),TEXT("BulletAudioPlaybackTest"))) return;
 if(FParse::Param(FCommandLine::Get(),TEXT("VanthexEnergyTest"))) return;
 if(FParse::Param(FCommandLine::Get(),TEXT("VanthexNetworkProbe")) || FParse::Param(FCommandLine::Get(),TEXT("VanthexMapTest"))) return;
 if(FParse::Param(FCommandLine::Get(),TEXT("VanthexMapShots"))) return;
 if(!HasAuthority() || FParse::Param(FCommandLine::Get(),TEXT("VanthexSelfTest")) || FParse::Param(FCommandLine::Get(),TEXT("VanthexVisualShots"))) return;
 auto* BotCharacter=Cast<AVanthexCharacter>(GetPawn()); auto* Mode=GetWorld()->GetAuthGameMode<AVanthexGameMode>();
 if(!BotCharacter || !Mode || !BotCharacter->GetWeapon()) return;
 auto* Weapon=BotCharacter->GetWeapon();
 if(BotCharacter->GetHealthComponent()->IsDead() || !Mode->IsRoundActive()) { Weapon->StopFire(); Route.Reset(); return; }
 if(BotCharacter->GetGadgets()->IsBlinded()){Weapon->StopFire();CombatTarget.Reset();TargetVisibleSince=GetWorld()->GetTimeSeconds();return;}
 // A departing human can leave this pawn holding a knife. Bots use rifle combat.
 if(auto* Equipment=BotCharacter->GetEquipment(); Equipment && Equipment->GetSelectedSlot()==EVanthexWeaponSlot::Knife) {
  if(Equipment->HasSlot(EVanthexWeaponSlot::Rifle)) Equipment->SelectRifle(); else Equipment->SelectPistol();
 }
 const float Now=GetWorld()->GetTimeSeconds();
 AVanthexCharacter* Enemy=nullptr; float Best=1800;
 for(TActorIterator<AVanthexCharacter> It(GetWorld());It;++It) {
  if(*It==BotCharacter || It->GetHealthComponent()->IsDead() || !Mode->CanDamage(BotCharacter,*It)) continue;
  float Distance=FVector::Dist(BotCharacter->GetActorLocation(),It->GetActorLocation()); if(Distance>=Best) continue;
  if(AVanthexGadgetActor::SmokeBlocks(GetWorld(),BotCharacter->GetPawnViewLocation(),It->GetActorLocation()))continue;
  FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(BotSight),false,BotCharacter); Params.AddIgnoredActor(Weapon);
  if(!GetWorld()->LineTraceSingleByChannel(Hit,BotCharacter->GetPawnViewLocation(),It->GetActorLocation(),ECC_Visibility,Params) || Hit.GetActor()==*It) { Enemy=*It; Best=Distance; }
 }
 if(Enemy!=CombatTarget.Get()) { CombatTarget=Enemy; TargetVisibleSince=Now; Weapon->StopFire(); }
 if(Enemy) {
  SetControlRotation((Enemy->GetActorLocation()-BotCharacter->GetPawnViewLocation()).Rotation());
  if(Now>=NextGadgetUse&&Best>350.f&&Best<1100.f){
   const auto* State=BotCharacter->GetPlayerState<AVanthexPlayerState>();
   if(State&&State->GetEconomy().Gadgets.Num()){
    const FName Gadget=State->GetEconomy().Gadgets[0];
    if(Gadget!=TEXT("Smoke")||BotCharacter->GetHealthComponent()->GetHealth()<65.f||BotCharacter->IsCarryingCore()){
     BotCharacter->GetGadgets()->UseSlot(0);NextGadgetUse=Now+10.f;NextCombatShot=Now+.9f;
    }
   }
  }
  if (Weapon->GetAmmoInMagazine()==0 && Weapon->GetReserveAmmo()>0) Weapon->StartReload();
  if(Now-TargetVisibleSince>0.5f && !Weapon->IsReloading()) {
   if (Weapon->IsAutomatic()) { if(!Weapon->IsTriggerHeld()) Weapon->StartFire(); }
   else if(Now >= NextCombatShot) { Weapon->StopFire(); Weapon->StartFire(); NextCombatShot = Now + FMath::Max(.22f,Weapon->GetFireInterval()); }
  }
 } else Weapon->StopFire();
 auto* Core=Mode->GetCore(); if(!Core) return;
 FVector Goal=Core->GetActorLocation();
 if(Core->GetCarrier()==BotCharacter) {
  float Closest=TNumericLimits<float>::Max();
  for(TActorIterator<AVanthexExtractionZone> It(GetWorld());It;++It) {
   const float Distance=FVector::DistSquared(BotCharacter->GetActorLocation(),It->GetActorLocation());
   if(Distance<Closest) { Closest=Distance; Goal=It->GetActorLocation()+FVector(0,0,80); }
  }
 } else if(!Core->GetCarrier()) BotCharacter->TryPickupCore();
 else Goal=Core->GetCarrier()->GetActorLocation()+FVector(250,0,0);
 if(FVector::Dist2D(BotCharacter->GetActorLocation(),Goal)<90 && FMath::Abs(BotCharacter->GetActorLocation().Z-Goal.Z)<140) return;
 if(Now>=NextRouteTime) { PlanRoute(Goal); NextRouteTime=Now+1.5f; if(FParse::Param(FCommandLine::Get(),TEXT("VanthexBotRouteProbe"))) UE_LOG(LogTemp,Display,TEXT("BotRoute %s position=%s route=%d next=%s"),*GetName(),*BotCharacter->GetActorLocation().ToString(),Route.Num(),Route.Num()?*Route[0].ToString():TEXT("none")); }
 while(Route.Num() && FVector::Dist2D(BotCharacter->GetActorLocation(),Route[0])<70 && FMath::Abs(BotCharacter->GetActorLocation().Z-Route[0].Z)<90) Route.RemoveAt(0);
 if(Route.Num()) {
  for(TActorIterator<AVanthexShiftDevice> It(GetWorld());It;++It) if(It->Kind==0 && !It->bActive && FVector::Dist2D(BotCharacter->GetActorLocation(),It->GetActorLocation())<450) It->Activate(BotCharacter,500);
  const FVector Direction=(Route[0]-BotCharacter->GetActorLocation()).GetSafeNormal2D();
  if(!Enemy) SetControlRotation(Direction.Rotation());
  BotCharacter->AddMovementInput(Direction,1.f);
 }
}


