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
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

AVanthexBotController::AVanthexBotController()
{
 PrimaryActorTick.bCanEverTick=true;
}

void AVanthexBotController::PlanRoute(const FVector& Goal)
{
 if(GetPawn()) VanthexNavigation::FindRoute(GetWorld(),GetPawn()->GetActorLocation(),Goal,Route);
}
void AVanthexBotController::TurnToward(const FVector& Point,float DeltaSeconds)
{
 auto* ControlledPawn=Cast<AVanthexCharacter>(GetPawn()); if(!ControlledPawn)return;
 const FRotator Desired=(Point-ControlledPawn->GetPawnViewLocation()).Rotation();
 const FRotator Rotation=FMath::RInterpConstantTo(GetControlRotation(),Desired,DeltaSeconds,150.f);
 SetControlRotation(Rotation);
 // This controller derives from AController, which does not perform the pawn-facing
 // update supplied by AAIController/APlayerController. Replicated actor yaw drives
 // the body, weapon mount and remote animation together.
 ControlledPawn->FaceRotation(Rotation,DeltaSeconds);
}
void AVanthexBotController::MoveSafely(const FVector& Direction,float Strength)
{
 auto* ControlledPawn=Cast<AVanthexCharacter>(GetPawn()); if(!ControlledPawn)return;
 FVector Move=Direction.GetSafeNormal2D(); if(Move.IsNearlyZero())return;
 const FVector Start=ControlledPawn->GetActorLocation();
 // Separate squad members without pushing them into walls or off ledges.
 FVector Separation=FVector::ZeroVector;
 for(TActorIterator<AVanthexCharacter> It(GetWorld());It;++It)if(*It!=ControlledPawn&&!It->GetHealthComponent()->IsDead()) {
  const FVector Away=(Start-It->GetActorLocation()).GetSafeNormal2D();
  const float Distance=FVector::Dist2D(Start,It->GetActorLocation());
  if(Distance<160&&FMath::Abs(Start.Z-It->GetActorLocation().Z)<100)Separation+=Away*(1-Distance/160.f);
 }
 Move=(Move+Separation*.85f).GetSafeNormal2D();
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BotLocalSteering),false,ControlledPawn); Q.AddIgnoredActor(ControlledPawn->GetWeapon());
 for(const float Angle:{0.f,35.f,-35.f,70.f,-70.f}) {
  const FVector Candidate=Move.RotateAngleAxis(Angle,FVector::UpVector),End=Start+Candidate*110;
  FHitResult Floor;
  const float Step=ControlledPawn->GetCharacterMovement()->MaxStepHeight;
  const float Feet=Start.Z-ControlledPawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  if(!GetWorld()->LineTraceSingleByChannel(Floor,End+FVector(0,0,Step),End-FVector(0,0,160),ECC_Visibility,Q)||Floor.ImpactNormal.Z<.65f)continue;
  const float Rise=Floor.ImpactPoint.Z-Feet;
  if(Rise>Step+2.f||Rise< -80.f)continue;
  // Test at the permissible step height so ordinary stairs are not treated as walls.
  const FVector Lift(0,0,FMath::Max(0.f,Rise+2.f));
  if(GetWorld()->SweepTestByChannel(Start+Lift,End+Lift,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(38,75),Q))continue;
  ControlledPawn->AddMovementInput(Candidate,Strength); return;
 }
}
void AVanthexBotController::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 if(FParse::Param(FCommandLine::Get(),TEXT("BodyViewShots"))||FParse::Param(FCommandLine::Get(),TEXT("HUDShots"))||FParse::Param(FCommandLine::Get(),TEXT("CoreShots")))return;
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
 if(BotCharacter->GetHealthComponent()->IsDead() || !Mode->IsRoundActive()) { Weapon->StopFire(); Route.Reset(); CombatTarget.Reset(); BurstEnds=0; return; }
 if(BotCharacter->GetGadgets()->IsBlinded()){Weapon->StopFire();CombatTarget.Reset();TargetVisibleSince=GetWorld()->GetTimeSeconds();return;}
 // A departing human can leave this pawn holding a knife. Bots use rifle combat.
 if(auto* Equipment=BotCharacter->GetEquipment(); Equipment && Equipment->GetSelectedSlot()==EVanthexWeaponSlot::Knife) {
  if(Equipment->HasSlot(EVanthexWeaponSlot::Rifle)) Equipment->SelectRifle(); else Equipment->SelectPistol();
 }
 const float Now=GetWorld()->GetTimeSeconds();
 AVanthexCharacter* Enemy=nullptr; float Best=2200;
 for(TActorIterator<AVanthexCharacter> It(GetWorld());It;++It) {
  if(*It==BotCharacter || It->GetHealthComponent()->IsDead() || !Mode->CanDamage(BotCharacter,*It)) continue;
  float Distance=FVector::Dist(BotCharacter->GetActorLocation(),It->GetActorLocation()); if(Distance>=Best) continue;
  // A retained visible target can cross the periphery, but new targets must be
  // in the forward 120-degree field. No omniscient acquisition behind the bot.
  const FVector ToEnemy=(It->GetActorLocation()-BotCharacter->GetActorLocation()).GetSafeNormal2D();
  if(*It!=CombatTarget.Get()&&FVector::DotProduct(BotCharacter->GetActorForwardVector(),ToEnemy)<.5f)continue;
  if(AVanthexGadgetActor::SmokeBlocks(GetWorld(),BotCharacter->GetPawnViewLocation(),It->GetActorLocation()))continue;
  FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(BotSight),false,BotCharacter); Params.AddIgnoredActor(Weapon);
  if(!GetWorld()->LineTraceSingleByChannel(Hit,BotCharacter->GetPawnViewLocation(),It->GetActorLocation(),ECC_Visibility,Params) || Hit.GetActor()==*It) { Enemy=*It; Best=Distance; }
 }
 if(Enemy!=CombatTarget.Get()) { CombatTarget=Enemy; TargetVisibleSince=Now; NextCombatShot=Now+1.f; BurstEnds=0; Weapon->StopFire(); }
 if(Enemy) {
  const float Phase=float(GetUniqueID()%17)*.7f;
  const FVector Chest=Enemy->GetActorLocation()+FVector(0,0,16);
  const FVector Right=FVector::CrossProduct(FVector::UpVector,(Chest-BotCharacter->GetPawnViewLocation()).GetSafeNormal()).GetSafeNormal();
  const float Error=FMath::Clamp(Best*.035f,18.f,65.f);
  TurnToward(Chest+Right*FMath::Sin(Now*1.7f+Phase)*Error+FVector(0,0,FMath::Sin(Now*1.1f+Phase)*Error*.3f),DeltaSeconds);
  const bool Facing=FVector::DotProduct(BotCharacter->GetActorForwardVector(),(Chest-BotCharacter->GetActorLocation()).GetSafeNormal2D())>.985f;
  if(Facing&&Now-TargetVisibleSince>2.f&&Now>=NextGadgetUse&&Best>500.f&&Best<1100.f){
   const auto* State=BotCharacter->GetPlayerState<AVanthexPlayerState>();
   if(State&&State->GetEconomy().Gadgets.Num()){
    const FName Gadget=State->GetEconomy().Gadgets[0];
    if(Gadget!=TEXT("Smoke")||BotCharacter->GetHealthComponent()->GetHealth()<65.f||BotCharacter->IsCarryingCore()){
     BotCharacter->GetGadgets()->UseSlot(0);NextGadgetUse=Now+14.f;NextCombatShot=Now+1.f;BurstEnds=0;
    }
   }
  }
  if (Weapon->GetAmmoInMagazine()==0 && Weapon->GetReserveAmmo()>0) Weapon->StartReload();
  if(!Facing||Weapon->IsReloading()) { Weapon->StopFire(); BurstEnds=0; }
  else if(BurstEnds>0&&Now>=BurstEnds) { Weapon->StopFire(); BurstEnds=0; NextCombatShot=Now+.85f; }
  else if(Now>=NextCombatShot&&BurstEnds==0) {
   Weapon->StopFire(); Weapon->StartFire();
   if(Weapon->IsAutomatic())BurstEnds=Now+Weapon->GetFireInterval()*2.2f;
   else NextCombatShot=Now+.65f;
  }
  // Fight from a useful distance instead of running blindly through the target
  // on the way to the Core. Slower lateral movement leaves readable exchanges.
  const FVector Toward=(Enemy->GetActorLocation()-BotCharacter->GetActorLocation()).GetSafeNormal2D();
  const float Side=FMath::Sin(Now*.65f+Phase)>.0f?1.f:-1.f;
  const FVector Travel=Best>1050?Toward:Best<430?-Toward:FVector::CrossProduct(FVector::UpVector,Toward)*Side;
  MoveSafely(Travel,.45f);
  return;
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
  TurnToward(BotCharacter->GetPawnViewLocation()+Direction*500,DeltaSeconds);
  MoveSafely(Direction,.75f);
 }
}


