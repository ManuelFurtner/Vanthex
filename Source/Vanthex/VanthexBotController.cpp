#include "VanthexBotController.h"
#include "VanthexCharacter.h"
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
 if(FParse::Param(FCommandLine::Get(),TEXT("GridV3Test")) || FParse::Param(FCommandLine::Get(),TEXT("GridV3GameTest")) || FParse::Param(FCommandLine::Get(),TEXT("GridV3Shots")) || FParse::Param(FCommandLine::Get(),TEXT("BulletAudioNetworkTest")) || FParse::Param(FCommandLine::Get(),TEXT("BulletAudioPlaybackTest"))) return;
 if(FParse::Param(FCommandLine::Get(),TEXT("VanthexEnergyTest"))) return;
 if(FParse::Param(FCommandLine::Get(),TEXT("VanthexNetworkProbe")) || FParse::Param(FCommandLine::Get(),TEXT("VanthexMapTest"))) return;
 if(FParse::Param(FCommandLine::Get(),TEXT("VanthexMapShots"))) return;
 if(!HasAuthority() || FParse::Param(FCommandLine::Get(),TEXT("VanthexSelfTest")) || FParse::Param(FCommandLine::Get(),TEXT("VanthexVisualShots"))) return;
 auto* BotCharacter=Cast<AVanthexCharacter>(GetPawn()); auto* Mode=GetWorld()->GetAuthGameMode<AVanthexGameMode>();
 if(!BotCharacter || !Mode || !BotCharacter->GetWeapon()) return;
 auto* Weapon=BotCharacter->GetWeapon();
 if(BotCharacter->GetHealthComponent()->IsDead() || !Mode->IsRoundActive()) { Weapon->StopFire(); Route.Reset(); return; }
 const float Now=GetWorld()->GetTimeSeconds();
 AVanthexCharacter* Enemy=nullptr; float Best=1800;
 for(TActorIterator<AVanthexCharacter> It(GetWorld());It;++It) {
  if(*It==BotCharacter || It->GetHealthComponent()->IsDead() || !Mode->CanDamage(BotCharacter,*It)) continue;
  float Distance=FVector::Dist(BotCharacter->GetActorLocation(),It->GetActorLocation()); if(Distance>=Best) continue;
  FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(BotSight),false,BotCharacter); Params.AddIgnoredActor(Weapon);
  if(!GetWorld()->LineTraceSingleByChannel(Hit,BotCharacter->GetPawnViewLocation(),It->GetActorLocation(),ECC_Visibility,Params) || Hit.GetActor()==*It) { Enemy=*It; Best=Distance; }
 }
 if(Enemy!=CombatTarget.Get()) { CombatTarget=Enemy; TargetVisibleSince=Now; Weapon->StopFire(); }
 if(Enemy) {
  SetControlRotation((Enemy->GetActorLocation()-BotCharacter->GetPawnViewLocation()).Rotation());
  if(Now-TargetVisibleSince>0.5f && !Weapon->IsTriggerHeld()) Weapon->StartFire();
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


