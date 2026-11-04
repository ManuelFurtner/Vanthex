#include "VanthexGameMode.h"
#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexCore.h"
#include "VanthexTestArena.h"
#include "VanthexExtractionZone.h"
#include "VanthexGameState.h"
#include "VanthexPlayerState.h"
#include "VanthexBotController.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"
#endif

void VanthexRunLooseCoreExtractionTest(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode||!Mode->HasAuthority()||!Mode->Core||!Mode->Arena||Mode->Combatants.Num()!=6)return;
 static TWeakObjectPtr<UWorld> Fixture; static int32 Step=0,Case=0,Failures=0; static double Start=0,Next=0,ClockStart=0;
 static TArray<FString> Lines;
 auto* World=Mode->GetWorld(); const double Now=FPlatformTime::Seconds();
 if(Fixture.Get()!=World) {Fixture=World;Step=Case=Failures=0;Start=Now;Next=0;Lines.Reset();}
 if(Step==100)return;
 auto Check=[&](bool OK,const FString& Name){Lines.Add(FString(OK?TEXT("PASS "):TEXT("FAIL "))+Name);if(!OK)++Failures;};
 auto Finish=[&](){Step=100;const FString Report=FString::Printf(TEXT("OVERALL %s CHECKS %d FAILURES %d\n"),Failures?TEXT("FAIL"):TEXT("PASS"),Lines.Num(),Failures)+FString::Join(Lines,TEXT("\n"));FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("LooseCoreExtraction.txt")));UE_LOG(LogVanthex,Display,TEXT("LOOSE_CORE_TEST %s"),*Report);FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);};
 if(Now-Start>65){Check(false,TEXT("bounded test completes"));Finish();return;}
 if(Now<Next)return;
 auto* Core=Mode->Core.Get();
 auto* Alpha=Mode->FindCombatant(EVanthexTeam::Alpha,0); auto* Bravo=Mode->FindCombatant(EVanthexTeam::Bravo,0);
 if(!Alpha||!Bravo||!Alpha->GetController()||!Bravo->GetController())return;
 const auto Team=Case%2==0?EVanthexTeam::Alpha:EVanthexTeam::Bravo;
 const auto Id=Case<2?EVanthexExtractionId::A:EVanthexExtractionId::B;
 auto* Pawn=Team==EVanthexTeam::Alpha?Alpha:Bravo;
 auto* Zone=Mode->Arena->GetExtractionZone(Id); auto* ZoneA=Mode->Arena->GetExtractionZone(EVanthexExtractionId::A);
 auto Place=[](AVanthexCharacter* C,FVector P,FVector Direction=FVector(1,0,0)){
  C->SetHoldForward(false);C->GetCharacterMovement()->StopMovementImmediately();C->GetCharacterMovement()->DisableMovement();
  C->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);C->GetController()->SetControlRotation(Direction.Rotation());
 };
 auto Park=[&](){for(int32 I=0;I<Mode->Combatants.Num();++I){auto* C=Mode->Combatants[I].Get();if(auto* Bot=Cast<AVanthexBotController>(C->GetController()))Bot->SetActorTickEnabled(false);if(C->GetWeapon())C->GetWeapon()->StopFire();Place(C,FVector(50000,I*500,5000));}};
 auto Round=[&](){Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);Mode->BeginRound();Mode->ActivateRound();Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);Park();};
 auto Acquire=[&](AVanthexCharacter* C){Core->ResetToSpawn();Place(C,Core->GetActorLocation()+FVector(0,0,60));return Core->TryPickup(C);};
 auto NextStep=[&](int32 Value,double Delay=.05){Step=Value;Next=Now+Delay;};
 const FString Label=FString::Printf(TEXT("case %d (%s / %s): "),Case,Team==EVanthexTeam::Alpha?TEXT("Alpha"):TEXT("Bravo"),Id==EVanthexExtractionId::A?TEXT("A"):TEXT("B"));
 switch(Step){
 case 0:
  Round();Check(Acquire(Pawn),Label+TEXT("pickup"));Place(Pawn,Zone->GetActorLocation()+FVector(-60,0,96));
  Mode->UpdateExtraction(3);Check(Mode->ActiveExtraction==Id&&FMath::IsNearlyEqual(Mode->ExtractionElapsed,3.f),Label+TEXT("held extraction starts"));
  Core->Drop();Mode->UpdateExtraction(0);
  Check(!Core->GetCarrier()&&Core->GetCoreState()==EVanthexCoreState::Thrown&&FMath::IsNearlyEqual(Mode->ExtractionElapsed,3.f),Label+TEXT("release keeps progress while physics remains active"));NextStep(1,.9);return;
 case 1:
  Check(Core->GetCoreState()==EVanthexCoreState::AtRest&&Zone->Contains(Core->GetActorLocation()),Label+TEXT("actual dropped Core settles inside pad"));Mode->UpdateExtraction(2);
  Check(FMath::IsNearlyEqual(Mode->ExtractionElapsed,5.f)&&FMath::IsNearlyEqual(Mode->GetGameState<AVanthexGameState>()->GetExtractionRemaining(),10.f),Label+TEXT("loose Core advances countdown and HUD"));
  Place(Pawn,Core->GetActorLocation()+FVector(0,0,60));Check(Core->TryPickup(Pawn),Label+TEXT("extracting loose Core remains pickable"));Mode->UpdateExtraction(0);
  Check(FMath::IsNearlyEqual(Mode->ExtractionElapsed,5.f),Label+TEXT("same-team pickup preserves progress"));
  {auto* Other=Mode->Arena->GetExtractionZone(Id==EVanthexExtractionId::A?EVanthexExtractionId::B:EVanthexExtractionId::A);Place(Pawn,Other->GetActorLocation()+FVector(-60,0,96));Mode->UpdateExtraction(0);
   Check(Mode->ActiveExtraction==Other->GetId()&&Mode->ExtractionElapsed==0,Label+TEXT("changing pads resets progress"));}
  // Start outside the actual pad, then use production throw physics to enter it.
  {float Edge=0;while(Edge<2000&&Zone->Contains(Zone->GetActorLocation()+FVector(-Edge,0,0)))Edge+=10;
   Place(Pawn,Zone->GetActorLocation()+FVector(-Edge-150,0,96),FVector(1,0,-.75));}
  Mode->UpdateExtraction(0);Check(Mode->ActiveExtraction==EVanthexExtractionId::None&&Mode->ExtractionElapsed==0,Label+TEXT("leaving pad cancels countdown"));
  Core->Throw();Place(Pawn,FVector(50000,0,5000));NextStep(2,1.2);return;
 case 2:
  Check(!Core->GetCarrier()&&Core->GetCoreState()==EVanthexCoreState::AtRest&&Zone->Contains(Core->GetActorLocation()),Label+TEXT("actual throw from outside lands inside pad: ")+Core->GetActorLocation().ToString());
  Mode->UpdateExtraction(.1f);Check(Mode->ActiveExtraction==Id&&Mode->ExtractionTeam==Team,Label+TEXT("throw starts extraction for last carrier team"));
  Mode->UpdateExtraction(14.7f);Check(Mode->IsRoundActive()&&FMath::IsNearlyEqual(Mode->ExtractionElapsed,14.8f,.01f),Label+TEXT("cannot win before fifteen seconds"));
  Mode->UpdateExtraction(.2f);Check(!Mode->IsRoundActive()&&Mode->RoundWinner==Team&&Core->GetCoreState()==EVanthexCoreState::Extracted&&!Core->GetCarrier(),Label+TEXT("loose Core completes and credits correct team"));
  Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);++Case;NextStep(Case<4?0:3);return;
 case 3:
  Round();Check(Acquire(Alpha),TEXT("takeover fixture pickup"));Place(Alpha,ZoneA->GetActorLocation()+FVector(-60,0,96));Mode->UpdateExtraction(4);Core->Drop();NextStep(4,.9);return;
 case 4:
  Place(Bravo,Core->GetActorLocation()+FVector(0,0,60));Check(Core->TryPickup(Bravo),TEXT("opponent can recover an extracting loose Core"));Mode->UpdateExtraction(0);
  Check(Mode->ExtractionTeam==EVanthexTeam::Bravo&&Mode->ExtractionElapsed==0,TEXT("opponent possession starts a fresh timer"));
  Core->ResetToSpawn();Core->SetActorLocation(ZoneA->GetActorLocation()+FVector(0,0,30));Mode->UpdateExtraction(5);
  Check(Core->GetExtractionTeam()==EVanthexTeam::None&&Mode->ActiveExtraction==EVanthexExtractionId::None&&Mode->ExtractionElapsed==0,TEXT("reset unclaimed Core cannot extract for stale team"));
  Park();Check(Acquire(Alpha),TEXT("death fixture pickup"));Place(Alpha,ZoneA->GetActorLocation()+FVector(-60,0,96));Mode->UpdateExtraction(3);
  {FVanthexDamageRequest Fatal;Fatal.Damage=1000;Alpha->GetHealthComponent()->ApplyDamage(Fatal);}
  Mode->UpdateExtraction(0);Check(Mode->ExtractionTeam==EVanthexTeam::Alpha&&FMath::IsNearlyEqual(Mode->ExtractionElapsed,3.f)&&!Core->GetCarrier(),TEXT("carrier death preserves dropped Core team and progress"));
  ClockStart=Now;Mode->GetWorldTimerManager().SetTimer(Mode->FlowTimer,Mode,&AVanthexGameMode::TickRoundClock,.1f,true);NextStep(5);return;
 case 5:
  if(Mode->IsRoundActive())return;
  Check(Mode->RoundWinner==EVanthexTeam::Alpha&&Core->GetCoreState()==EVanthexCoreState::Extracted&&!Core->GetCarrier()&&Now-ClockStart>=11.8&&Now-ClockStart<13.5,TEXT("normal round timer finishes remaining twelve seconds after death without a carrier"));
  Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);Finish();return;
 }
#endif
}
