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

namespace
{
// Short downward delivery, using the Core's sphere, gravity and sliding loss.
// Reject wall contacts and edge landings; if no safe throw exists the bot walks closer.
bool SafeCoreDelivery(AVanthexCharacter* Pawn, AVanthexCore* Core, AVanthexExtractionZone* Zone, const FVector& Direction)
{
 auto* World=Pawn->GetWorld();
 FVector Position=Pawn->GetActorLocation()+FVector(0,0,60)+Direction*90;
 FVector Velocity=Direction*Core->GetThrowSpeed();
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BotCoreDelivery),false,Core);
 Query.AddIgnoredActor(Pawn); Query.AddIgnoredActor(Pawn->GetWeapon());
 FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
 FHitResult Hit;
 if(World->SweepSingleByObjectType(Hit,Pawn->GetActorLocation(),Position,FQuat::Identity,Objects,FCollisionShape::MakeSphere(20),Query))return false;
 for(int32 Step=0;Step<100;++Step) {
  Velocity.Z-=980.f*.02f;
  const FVector Next=Position+Velocity*.02f;
  if(World->SweepSingleByObjectType(Hit,Position,Next,FQuat::Identity,Objects,FCollisionShape::MakeSphere(20),Query)) {
   if(Hit.bStartPenetrating||Hit.Normal.Z<.65f)return false;
   Position=Hit.Location+Hit.Normal*.5f;
   Velocity=FVector::VectorPlaneProject(Velocity,Hit.Normal)*.5f;
   if(Velocity.Size()<60.f) {
    for(const FVector Margin:{FVector(45,0,0),FVector(-45,0,0),FVector(0,45,0),FVector(0,-45,0)})
     if(!Zone->Contains(Position+Margin))return false;
    return true;
   }
  } else Position=Next;
 }
 return false;
}
}

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
 if(FParse::Param(FCommandLine::Get(),TEXT("GridV4Test")) || FParse::Param(FCommandLine::Get(),TEXT("GridV4GameTest")) || FParse::Param(FCommandLine::Get(),TEXT("GridV4Shots")) || FParse::Param(FCommandLine::Get(),TEXT("GridV3Test")) || FParse::Param(FCommandLine::Get(),TEXT("GridV3GameTest")) || FParse::Param(FCommandLine::Get(),TEXT("GridV3Shots")) || FParse::Param(FCommandLine::Get(),TEXT("BulletAudioNetworkTest")) || FParse::Param(FCommandLine::Get(),TEXT("BulletAudioPlaybackTest"))) return;
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
 auto* Core=Mode->GetCore(); if(!Core)return;
 const auto* State=BotCharacter->GetPlayerState<AVanthexPlayerState>();
 const bool Deposited=Mode->HasDepositedCore();
 const bool FriendlyDeposit=Deposited&&State&&Core->GetExtractionTeam()==State->GetTeam();
 AVanthexExtractionZone* DeliveryZone=nullptr; float DeliveryDistance=TNumericLimits<float>::Max();
 if(Core->GetCarrier()==BotCharacter)for(TActorIterator<AVanthexExtractionZone> It(GetWorld());It;++It) {
  const float Distance=FVector::DistSquared(BotCharacter->GetActorLocation(),It->GetActorLocation());
  if(Distance<DeliveryDistance){DeliveryDistance=Distance;DeliveryZone=*It;}
 }
 // Recover an enemy deposit before fighting; finish a nearby delivery under pressure.
 const bool UrgentObjective=(Deposited&&!FriendlyDeposit)||(DeliveryZone&&DeliveryDistance<FMath::Square(600.f));
 AVanthexCharacter* Enemy=nullptr; float Best=2200;
 for(TActorIterator<AVanthexCharacter> It(GetWorld());It;++It) {
  if(UrgentObjective)break;
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
 FVector Goal=Core->GetActorLocation();
 if(DeliveryZone) {
  Goal=DeliveryZone->GetActorLocation()+FVector(0,0,96);
  if(FVector::Dist2D(BotCharacter->GetActorLocation(),Goal)<550.f&&FMath::Abs(BotCharacter->GetActorLocation().Z-Goal.Z)<100.f) {
   FVector Toward=(Goal-BotCharacter->GetActorLocation()).GetSafeNormal2D();
   if(Toward.IsNearlyZero())Toward=BotCharacter->GetActorForwardVector();
   for(float Slope:{.75f,1.5f,3.f,8.f}) {
    const FVector Direction=(Toward-FVector(0,0,Slope)).GetSafeNormal();
    if(!SafeCoreDelivery(BotCharacter,Core,DeliveryZone,Direction))continue;
    BotCharacter->GetCharacterMovement()->StopMovementImmediately();
    TurnToward(BotCharacter->GetPawnViewLocation()+Direction*500,DeltaSeconds);
    if(FVector::DotProduct(GetControlRotation().Vector(),Direction)>.9998f) {Core->Throw();Route.Reset();NextRouteTime=0;}
    return;
   }
  }
 } else if(FriendlyDeposit) {
  // Do not undo a teammate's delivery or catch it again: step back and watch approaches.
  const FVector Away=(BotCharacter->GetActorLocation()-Goal).GetSafeNormal2D();
  if(FVector::Dist2D(BotCharacter->GetActorLocation(),Goal)<300.f)MoveSafely(Away,.65f);
  TurnToward(BotCharacter->GetPawnViewLocation()+BotCharacter->GetActorForwardVector().RotateAngleAxis(35.f*DeltaSeconds,FVector::UpVector)*500,DeltaSeconds);
  return;
 } else if(!Core->GetCarrier()) {
  BotCharacter->TryPickupCore();
  // A resting Core sits at floor height; route to standing height for the final approach.
  Goal.Z+=76.f;
 }
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


