#include "VanthexGameMode.h"
#include "VanthexGridV4Layout.h"
#include "VanthexTestArena.h"
#include "VanthexCharacter.h"
#include "VanthexCore.h"
#include "VanthexExtractionZone.h"
#include "VanthexPlayerState.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Exercises the saved level using the normal Core tick, round clock and character movement.
// Teleports are fixture setup only; throws, settling, extraction and ramp traversal run live.
void AVanthexGameMode::AdvanceGridV4GameTest()
{
 auto* Human=GetTestCharacter();
 auto* Mate=FindCombatant(EVanthexTeam::Alpha,1);
 auto* Enemy=FindCombatant(EVanthexTeam::Bravo,0);
 auto Finish=[&]() {
  if(Human) Human->SetHoldForward(false);
  GetWorldTimerManager().ClearTimer(SelfTestTimer);
  SelfTestLines.Insert(bSelfTestFailed?TEXT("OVERALL FAIL"):TEXT("OVERALL PASS"),0);
  FFileHelper::SaveStringArrayToFile(SelfTestLines,*(FPaths::ProjectSavedDir()/TEXT("GridV4Gameplay.txt")));
  FPlatformMisc::RequestExitWithStatus(false,bSelfTestFailed?1:0);
 };
 auto Next=[&](int32 Stage) { CoreTestMark=Stage; SelfTestTicks=0; MoveStartTime=GetWorld()->GetTimeSeconds(); };
 auto Expect=[&](bool OK,const TCHAR* Name,const FString& Detail) {
  SelfTestExpect(OK,Name,Detail);
  if(!OK) CoreTestMark=99;
  return OK;
 };
 auto Place=[](AVanthexCharacter* C,const FVector& Position,const FVector& Look=FVector(1,0,0)) {
  if(!C) return;
  C->SetHoldForward(false);
  C->GetCharacterMovement()->StopMovementImmediately(); C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  C->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
  if(C->GetController()) C->GetController()->SetControlRotation(Look.Rotation());
 };
 auto Park=[&]() {
  for(AVanthexCharacter* C:Combatants) if(C) {
   auto* PS=GetVanthexPlayerState(C);
   if(auto* Start=PS?Arena->GetTeamSpawn(PS->GetTeam(),PS->GetSpawnSlot()):nullptr) Place(C,Start->GetActorLocation());
  }
 };
 auto Acquire=[&](AVanthexCharacter* C) {
  Core->ResetToSpawn(); Place(C,Core->GetActorLocation()+FVector(0,0,60));
  return Core->TryPickup(C);
 };
 const float Now=GetWorld()->GetTimeSeconds();
 if(CoreTestMark==99) { Finish(); return; }
 if(!Human || !Mate || !Enemy || !Core || !Arena || (!IsRoundActive() && CoreTestMark!=9)) {
  if(++SelfTestTicks>240) { Expect(false,TEXT("WORLD_READY"),TEXT("roster or active round missing")); Finish(); }
  return;
 }
 ++SelfTestTicks;
 switch(CoreTestMark) {
 case 0:
  if(FParse::Param(FCommandLine::Get(),TEXT("GridV4BoundaryOnly"))){VisualShotIndex=0;Next(16);return;}
  Park();
  if(!Expect(Combatants.Num()==6 && Core->IsAtHome() && FMath::IsNearlyEqual(ExtractionDuration,15.f),TEXT("ROSTER_CORE_RULES"),TEXT("six players, neutral Core, fifteen seconds"))) return;
  if(!Expect(Acquire(Human),TEXT("PICKUP_ALPHA"),Core->GetActorLocation().ToString())) return;
  Human->DropCarriedCore(); Park(); Next(1); return;
 case 1:
  if(Core->GetCoreState()==EVanthexCoreState::AtRest) {
   if(!Expect(!Core->GetCarrier(),TEXT("DROP_SETTLES_MAIN"),Core->GetActorLocation().ToString())) return;
   if(!Expect(Acquire(Enemy),TEXT("PICKUP_BRAVO"),TEXT("shared neutral objective"))) return;
   FVanthexDamageRequest D; D.Damage=100; Enemy->GetHealthComponent()->ApplyDamage(D);
   if(!Expect(!Core->GetCarrier() && Core->GetCoreState()==EVanthexCoreState::Thrown,TEXT("DEATH_DROP"),TEXT("carrier ownership cleared"))) return;
   Enemy->PrepareForRound(Arena->GetTeamSpawn(EVanthexTeam::Bravo,0)->GetActorTransform()); Park();
   if(!Expect(Acquire(Human),TEXT("THROW_SETUP"),TEXT("Core picked up"))) return;
   const FVector P(-1400,-400,100);
   Place(Human,P,FVector(1,0,0)); Place(Mate,P+FVector(700,0,0)); Core->Throw(); Next(2);
  } else if(SelfTestTicks>100) Expect(false,TEXT("DROP_SETTLES_MAIN"),Core->GetActorLocation().ToString());
  return;
 case 2:
  if(Core->GetCarrier()==Mate) {
   Expect(true,TEXT("TEAMMATE_CATCH_THROUGH_DOOR"),TEXT("server-to-containment pass caught by Alpha"));
   Park(); const FVector P(-1400,-400,100);
   Place(Mate,P,FVector(1,0,0)); Place(Enemy,P+FVector(700,0,0)); Core->Throw(); Next(3);
  } else if(SelfTestTicks>70) Expect(false,TEXT("TEAMMATE_CATCH"),Core->GetActorLocation().ToString());
  return;
 case 3:
  if(Core->GetCarrier()==Enemy) {
   Expect(true,TEXT("ENEMY_INTERCEPT"),TEXT("Bravo intercepted live throw"));
   Park(); if(!Expect(Acquire(Human),TEXT("UPPER_PASS_SETUP"),TEXT("carry to observation"))) return;
   Place(Human,FVector(-100,750,700),FVector(1,0,-1)); Place(Mate,FVector(600,750,100));
   Next(12);
  } else if(SelfTestTicks>70) Expect(false,TEXT("ENEMY_INTERCEPT"),Core->GetActorLocation().ToString());
  return;
 case 12:
  if(Now-MoveStartTime>=0.35f) {
   UE_LOG(LogTemp,Display,TEXT("GRIDV4 UPPER PASS player=%s velocity=%s"),*Human->GetActorLocation().ToString(),*Human->GetCharacterMovement()->Velocity.ToString());
   Human->StopJumping(); Core->Throw(); Next(13);
  }
  return;
 case 13:
  if(Core->GetCoreState()==EVanthexCoreState::AtRest) {
   if(!Expect(!Human->CanJump()&&Core->GetActorLocation().Z>=600.f,TEXT("UPPER_RAIL_BLOCKS_STANDING_PASS"),TEXT("downward standing throw stays behind observation rail; jumping is disabled")))return;
   Park(); if(!Expect(Acquire(Human),TEXT("APERTURE_PASS_SETUP"),TEXT("carry to containment opening"))) return;
   Place(Human,FVector(700,280,100),FVector(0,1,-1)); Place(Enemy,FVector(700,750,-500)); Next(15);
  } else if(SelfTestTicks>70) Expect(false,TEXT("UPPER_STANDING_PASS_SETTLES"),Core->GetActorLocation().ToString());
  return;
 case 15:
  if(Now-MoveStartTime>=0.25f) { Human->StopJumping(); Core->Throw(); Next(14); }
  return;
 case 14:
  if(Core->GetCoreState()==EVanthexCoreState::AtRest) {
   if(!Expect(!Human->CanJump()&&Core->GetActorLocation().Z>=0.f,TEXT("APERTURE_RAIL_BLOCKS_STANDING_PASS"),TEXT("downward standing throw is stopped by aperture guard; use the lower routes")))return;
   Park(); if(!Expect(Acquire(Human),TEXT("LOWER_DROP_SETUP"),TEXT("carry to lower floor"))) return;
   FVector Lower=FVector::ZeroVector;
   for(const auto& Room:GridV4::Rooms()) if(Room.Level==-1) { Lower=Room.Center; break; }
   MoveStart=Lower+FVector(0,0,98); Place(Human,MoveStart); Core->Drop(); Park(); Next(4);
  } else if(SelfTestTicks>70) Expect(false,TEXT("APERTURE_STANDING_PASS_SETTLES"),Core->GetActorLocation().ToString());
  return;
 case 4:
  if(Now-MoveStartTime>2.f && Core->GetCoreState()==EVanthexCoreState::AtRest) {
   if(!Expect(Core->GetActorLocation().Z<-450 && !Core->IsAtHome(),TEXT("LOWER_DROP_SETTLES"),Core->GetActorLocation().ToString())) return;
   Place(Human,Core->GetActorLocation()+FVector(0,0,80));
   if(!Expect(Core->TryPickup(Human),TEXT("LOWER_PICKUP"),TEXT("Core remains recoverable in maintenance"))) return;
   Core->ResetToSpawn(); Core->SetActorLocation(FVector(0,0,-2200)); Next(5);
  } else if(SelfTestTicks>120) Expect(false,TEXT("LOWER_DROP_SETTLES"),Core->GetActorLocation().ToString());
  return;
 case 5:
  if(Core->IsAtHome()) {
   Expect(true,TEXT("VOID_RECOVERY"),TEXT("Core recovers below the facility"));
   Park(); if(!Expect(Acquire(Human),TEXT("EXTRACTION_CANCEL_SETUP"),TEXT("Alpha carries"))) return;
   Place(Human,Arena->GetExtractionZone(EVanthexExtractionId::A)->GetActorLocation()+FVector(0,0,80)); Next(6);
  } else if(SelfTestTicks>60) Expect(false,TEXT("VOID_RECOVERY"),Core->GetActorLocation().ToString());
  return;
 case 6:
  if(Core->GetCoreState()==EVanthexCoreState::Extracting) { Park(); Next(7); }
  else if(SelfTestTicks>60) Expect(false,TEXT("EXTRACTION_STARTS"),TEXT("A never started"));
  return;
 case 7:
  if(Core->GetCoreState()==EVanthexCoreState::Carried && ActiveExtraction==EVanthexExtractionId::None) {
   Expect(true,TEXT("EXTRACTION_CANCEL_LEAVE"),TEXT("leaving A resets progress")); VisualShotIndex=0; Next(8);
  } else if(SelfTestTicks>60) Expect(false,TEXT("EXTRACTION_CANCEL_LEAVE"),TEXT("progress persisted"));
  return;
 case 8: {
  Park();
  const bool bAlpha=VisualShotIndex<2; const auto Id=(VisualShotIndex%2)==0?EVanthexExtractionId::A:EVanthexExtractionId::B;
  auto* Carrier=bAlpha?Human:Enemy; Carrier->GetHealthComponent()->ResetHealth();
  if(!Expect(Acquire(Carrier),TEXT("EXTRACTION_PICKUP"),FString::Printf(TEXT("case %d"),VisualShotIndex))) return;
  CoreScoreBefore=bAlpha?AlphaScore:BravoScore;
  Place(Carrier,Arena->GetExtractionZone(Id)->GetActorLocation()+FVector(0,0,80)); Next(9); return;
 }
 case 9: {
  const bool bAlpha=VisualShotIndex<2;
  // EndRound changes phase immediately; detect score before the next entry's ready check.
  const int32 Score=bAlpha?AlphaScore:BravoScore;
  if(Score>CoreScoreBefore) {
   const float Seconds=Now-MoveStartTime;
   if(!Expect(Seconds>=14.8f && Seconds<=15.5f,TEXT("FIFTEEN_SECOND_EXTRACTION"),FString::Printf(TEXT("case %d team %s zone %s measured %.2fs"),VisualShotIndex,bAlpha?TEXT("Alpha"):TEXT("Bravo"),VisualShotIndex%2?TEXT("B"):TEXT("A"),Seconds))) return;
   ++VisualShotIndex;
   if(VisualShotIndex<4) Next(8); else { VisualShotIndex=0; Next(10); }
  } else if(SelfTestTicks>370) Expect(false,TEXT("FIFTEEN_SECOND_EXTRACTION"),FString::Printf(TEXT("case %d failed"),VisualShotIndex));
  return;
 }
 case 10: {
  Park(); Core->ResetToSpawn(); const auto Links=GridV4::VerticalLinks();
  if(VisualShotIndex>=Links.Num()*2) { VisualShotIndex=0;Next(16);return; }
  const auto& Link=Links[VisualShotIndex/2];
  const bool bUp=(VisualShotIndex%2)==0;
  const FVector Direction=(Link.Top-Link.Bottom).GetSafeNormal2D();
  const FVector Start=(bUp?Link.Bottom-Direction*90:Link.Top+Direction*90)+FVector(0,0,100);
  MoveStart=(bUp?Link.Top+Direction*90:Link.Bottom-Direction*90)+FVector(0,0,100);
  Place(Human,Start,bUp?Direction:-Direction); Human->SetHoldForward(true); Next(11); return;
 }
 case 11: {
  const auto Links=GridV4::VerticalLinks(); const auto& Link=Links[VisualShotIndex/2];
  const FVector Position=Human->GetActorLocation();
  if(FVector::Dist2D(Position,MoveStart)<90 && FMath::Abs(Position.Z-MoveStart.Z)<70) {
   Human->SetHoldForward(false); Human->GetCharacterMovement()->StopMovementImmediately();
   Expect(true,TEXT("WALK_VERTICAL"),FString::Printf(TEXT("link %d %s %.2fs %s"),Link.Id,VisualShotIndex%2?TEXT("down"):TEXT("up"),Now-MoveStartTime,*Position.ToString()));
   ++VisualShotIndex; Next(10); return;
  }
  Human->GetController()->SetControlRotation((MoveStart-Position).GetSafeNormal2D().Rotation());
  if(Now-MoveStartTime>25.f) Expect(false,TEXT("WALK_VERTICAL"),FString::Printf(TEXT("link %d direction%d stuck %s goal %s"),Link.Id,VisualShotIndex%2,*Position.ToString(),*MoveStart.ToString()));
  return;
 }
 case 16: {
  if(VisualShotIndex>=3){Finish();return;}
  Park();Core->ResetToSpawn();
  const FVector Starts[]={FVector(-700,-2650,-500),FVector(1050,-1150,100),FVector(-5400,-1450,100)};
  const FVector Directions[]={FVector(0,1,0),FVector(1,0,0),FVector(-1,0,0)};
  MoveStart=Starts[VisualShotIndex];Place(Human,MoveStart,Directions[VisualShotIndex]);
  Human->SetHoldForward(true);Next(17);return;
 }
 case 17: {
  const float Age=Now-MoveStartTime;
  if(Age>.4f&&Age<.65f)Human->Jump();
  if(Age>=.65f)Human->StopJumping();
  if(Age<2.f)return;
  Human->SetHoldForward(false);Human->GetCharacterMovement()->StopMovementImmediately();
  const FVector Position=Human->GetActorLocation();
  const float Moved=FVector::Dist2D(Position,MoveStart);
  if(!Expect(!Human->CanJump()&&Moved>10.f&&Moved<250.f&&FMath::Abs(Position.Z-MoveStart.Z)<45.f,TEXT("SEALED_EXIT_WALK_AND_BLOCKED_JUMP"),FString::Printf(TEXT("former leak %d moved %.1f cm; position %s"),VisualShotIndex,Moved,*Position.ToString())))return;
  ++VisualShotIndex;Next(16);return;
 }
 }
}
