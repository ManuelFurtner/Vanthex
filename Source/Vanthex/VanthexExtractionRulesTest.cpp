#include "VanthexGameMode.h"
#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexCore.h"
#include "VanthexTestArena.h"
#include "VanthexExtractionZone.h"
#include "VanthexPlayerState.h"
#include "VanthexBotController.h"
#include "VanthexEquipmentComponent.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"
#endif

void VanthexRunExtractionRulesTest(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode||!Mode->HasAuthority()||!Mode->Core||!Mode->Arena||Mode->Combatants.Num()!=6)return;
 static TWeakObjectPtr<UWorld> Fixture; static int32 Step=0,Case=0,Failures=0; static double Start=0,Next=0,StageAt=0;
 static TArray<FString> Lines;
 auto* World=Mode->GetWorld();const double Now=FPlatformTime::Seconds();
 if(Fixture.Get()!=World){Fixture=World;Step=Case=Failures=0;Start=Now;Next=0;Lines.Reset();}
 if(Step==100)return;
 auto Check=[&](bool OK,const FString& Name){Lines.Add(FString(OK?TEXT("PASS "):TEXT("FAIL "))+Name);if(!OK)++Failures;};
 auto Finish=[&](){Step=100;const FString Report=FString::Printf(TEXT("OVERALL %s CHECKS %d FAILURES %d\n"),Failures?TEXT("FAIL"):TEXT("PASS"),Lines.Num(),Failures)+FString::Join(Lines,TEXT("\n"));FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("ExtractionRules.txt")));UE_LOG(LogVanthex,Display,TEXT("EXTRACTION_RULES %s"),*Report);FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);};
 if(Now-Start>110){Check(false,TEXT("bounded test completes"));Finish();return;}if(Now<Next)return;
 auto* Core=Mode->Core.Get();auto* Alpha=Mode->FindCombatant(EVanthexTeam::Alpha,0);auto* Bravo=Mode->FindCombatant(EVanthexTeam::Bravo,0);
 if(!Alpha||!Bravo||!Alpha->GetController()||!Bravo->GetController())return;
 auto Place=[](AVanthexCharacter* C,FVector P){C->SetHoldForward(false);C->GetCharacterMovement()->StopMovementImmediately();C->GetCharacterMovement()->DisableMovement();C->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);C->GetController()->SetControlRotation(FRotator::ZeroRotator);C->SetActorRotation(FRotator::ZeroRotator);};
 auto Park=[&](){for(int32 I=0;I<Mode->Combatants.Num();++I){auto* C=Mode->Combatants[I].Get();if(auto* Bot=Cast<AVanthexBotController>(C->GetController()))Bot->SetActorTickEnabled(false);C->GetWeapon()->StopFire();Place(C,FVector(50000,I*500,5000));}};
 auto Round=[&](){Mode->AlphaScore=Mode->BravoScore=0;Mode->MatchWinner=EVanthexTeam::None;Mode->BeginRound();Mode->ActivateRound();Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);Park();};
 auto Acquire=[&](AVanthexCharacter* C){Core->ResetToSpawn();Place(C,Core->GetActorLocation()+FVector(0,0,60));return Core->TryPickup(C);};
 auto Kill=[&](EVanthexTeam Team){for(auto C:Mode->Combatants)if(Team==EVanthexTeam::None||C->GetPlayerState<AVanthexPlayerState>()->GetTeam()==Team){FVanthexDamageRequest Fatal;Fatal.Damage=1000;C->GetHealthComponent()->ApplyDamage(Fatal);}};
 auto Advance=[&](int32 Value,double Delay=.05){Step=Value;Next=Now+Delay;};
 const auto Team=Case%2==0?EVanthexTeam::Alpha:EVanthexTeam::Bravo;
 const auto Other=Team==EVanthexTeam::Alpha?EVanthexTeam::Bravo:EVanthexTeam::Alpha;
 auto* Pawn=Team==EVanthexTeam::Alpha?Alpha:Bravo;
 auto* Zone=Mode->Arena->GetExtractionZone((Case/2)%2==0?EVanthexExtractionId::A:EVanthexExtractionId::B);
 const FString Label=FString::Printf(TEXT("case %d: "),Case);
 switch(Step){
 case 0:
  Round();Check(Acquire(Pawn),Label+TEXT("claim"));Place(Pawn,Zone->GetActorLocation()+FVector(-60,0,96));Mode->UpdateExtraction(3);Core->Drop();Park();Advance(1,.9);return;
 case 1:
  Check(Core->GetCoreState()==EVanthexCoreState::AtRest&&Mode->HasDepositedCore(),Label+TEXT("physical drop settles in pad"));
  Kill(Case<4?Team:Case<8?Other:EVanthexTeam::None);
  Check(Mode->IsRoundActive()&&Mode->bEliminationDeferred,Label+TEXT("eliminated depositing/opposing/both teams cannot end deposit"));
  Mode->TimeRemaining=0;Mode->ResolveTimeout();
  Check(Mode->IsRoundActive(),Label+TEXT("zero round time cannot end deposit"));
  for(int32 I=0;I<119;++I)Mode->TickRoundClock();
  Check(Mode->IsRoundActive()&&FMath::IsNearlyEqual(Mode->ExtractionElapsed,14.9f,.01f),Label+TEXT("countdown advances without early victory"));Mode->TickRoundClock();
  Check(!Mode->IsRoundActive()&&Mode->RoundWinner==Team&&Core->GetCoreState()==EVanthexCoreState::Extracted,Label+TEXT("full fifteen seconds awards depositing team"));
  ++Case;Advance(Case<12?0:2);return;
 case 2:
  Round();Check(Acquire(Alpha),TEXT("recovery setup"));Place(Alpha,Zone->GetActorLocation()+FVector(-60,0,96));Mode->UpdateExtraction(3);Core->Drop();Park();Advance(3,.9);return;
 case 3:
  Kill(EVanthexTeam::Alpha);Check(Mode->IsRoundActive(),TEXT("dead depositing team still has pending objective"));Place(Bravo,Core->GetActorLocation()+FVector(0,0,76));
  Check(Core->TryPickup(Bravo),TEXT("opponent recovers deposited Core"));Mode->TickRoundClock();
  Check(!Mode->IsRoundActive()&&Mode->RoundWinner==EVanthexTeam::Bravo&&Core->GetCoreState()!=EVanthexCoreState::Extracted,TEXT("recovery ends deferred elimination for survivors"));
  Round();Kill(EVanthexTeam::Bravo);Check(!Mode->IsRoundActive()&&Mode->RoundWinner==EVanthexTeam::Alpha,TEXT("normal elimination without deposit unchanged"));
  Round();Check(Acquire(Alpha),TEXT("exit setup"));Place(Alpha,Zone->GetActorLocation()+FVector(-60,0,96));Core->Drop();Kill(EVanthexTeam::Alpha);
  Core->ResetToSpawn();Mode->TickRoundClock();Check(!Mode->IsRoundActive()&&Mode->RoundWinner==EVanthexTeam::Bravo,TEXT("Core leaving/resetting resolves deferred elimination"));
  Round();Alpha->GetEquipment()->SelectKnife();Advance(4,.8);return;
 case 4:
  Check(FMath::IsNearlyEqual(Alpha->GetCharacterMovement()->MaxWalkSpeed,690.f),TEXT("drawn knife gives 15 percent walk bonus"));
  Alpha->SetSprinting(true);Check(FMath::IsNearlyEqual(Alpha->GetCharacterMovement()->MaxWalkSpeed,1035.f),TEXT("knife sprint receives 15 percent bonus"));
  Check(FMath::IsNearlyEqual(Alpha->GetCharacterMovement()->MaxWalkSpeedCrouched,280.f),TEXT("crouch remains unchanged"));
  Alpha->GetEquipment()->SelectPistol();Alpha->SetSprinting(false);
  Check(FMath::IsNearlyEqual(Alpha->GetCharacterMovement()->MaxWalkSpeed,600.f),TEXT("holstering knife immediately removes bonus"));Advance(5,.8);return;
 case 5:
  Check(FMath::IsNearlyEqual(Alpha->GetCharacterMovement()->MaxWalkSpeed,600.f),TEXT("pistol returns to normal speed"));Alpha->SetSprinting(true);
  Check(FMath::IsNearlyEqual(Alpha->GetCharacterMovement()->MaxWalkSpeed,900.f),TEXT("pistol sprint unchanged"));Alpha->SetSprinting(false);
  Alpha->GetEquipment()->SelectKnife();Alpha->SetSprinting(false);
  Check(FMath::IsNearlyEqual(Alpha->GetCharacterMovement()->MaxWalkSpeed,600.f),TEXT("drawing knife does not grant early bonus"));Case=0;Advance(6,.8);return;
 case 6:
  Round();Pawn=Mode->FindCombatant(Team,1);Check(Acquire(Pawn),Label+TEXT("bot claims Core"));Place(Pawn,Zone->GetActorLocation()+FVector(-380,0,96));
  Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);CastChecked<AVanthexBotController>(Pawn->GetController())->SetActorTickEnabled(true);StageAt=Now;Advance(7,.05);return;
 case 7:
  Pawn=Mode->FindCombatant(Team,1);
  if(Core->GetCarrier()&&Now-StageAt<5)return;
  Check(!Core->GetCarrier()&&Core->GetCoreState()==EVanthexCoreState::Thrown,Label+TEXT("real bot tick turns and throws"));Advance(8,1.2);return;
 case 8:
  Pawn=Mode->FindCombatant(Team,1);
  Check(!Core->GetCarrier()&&Core->GetCoreState()==EVanthexCoreState::AtRest&&Mode->HasDepositedCore()&&Core->GetExtractionTeam()==Team,Label+TEXT("bot throw settles safely in zone and is not repicked"));
  Mode->UpdateExtraction(2);Check(Mode->ExtractionTeam==Team&&Mode->ExtractionElapsed>=2,Label+TEXT("bot delivery starts countdown"));
  // Put a defender within pickup distance: its actual AI must recover, even while facing an enemy.
  {auto* Defender=Mode->FindCombatant(Other,1);Place(Defender,Core->GetActorLocation()+FVector(0,0,76));CastChecked<AVanthexBotController>(Defender->GetController())->SetActorTickEnabled(true);}
  Advance(9,.1);return;
 case 9:
  Check(Core->GetCarrier()==Mode->FindCombatant(Other,1),Label+TEXT("enemy bot prioritizes recovery"));Mode->UpdateExtraction(0);
  Check(Mode->ExtractionTeam==Other&&Mode->ExtractionElapsed==0,Label+TEXT("recovery cancels old team countdown"));
  ++Case;Advance(Case<4?6:10);return;
 case 10:
  Case=0;Advance(11);return;
 case 11:
  Round();Check(Acquire(Pawn),Label+TEXT("friendly recovery fixture"));Place(Pawn,Zone->GetActorLocation()+FVector(-60,0,96));Mode->UpdateExtraction(3);Core->Drop();Park();Advance(12,.9);return;
 case 12:
  Mode->UpdateExtraction(0);Kill(Other);Mode->TimeRemaining=0;
  Check(Mode->IsRoundActive()&&Mode->bEliminationDeferred,Label+TEXT("enemy eliminated with loose deposit and expired round clock"));
  Place(Pawn,Core->GetActorLocation()+FVector(-50,0,76));Check(Core->TryPickup(Pawn),Label+TEXT("friendly pickup after elimination"));
  Mode->TickRoundClock();
  Check(Mode->IsRoundActive()&&Mode->HasPendingDepositedExtraction()&&FMath::IsNearlyEqual(Mode->ExtractionElapsed,3.1f,.01f),Label+TEXT("friendly held Core preserves countdown and timeout protection"));
  for(int32 I=0;I<119;++I)Mode->TickRoundClock();
  Check(!Mode->IsRoundActive()&&Mode->RoundWinner==Team&&Core->GetCoreState()==EVanthexCoreState::Extracted,Label+TEXT("friendly recovery finishes by extraction, not early elimination"));
  ++Case;Advance(Case<4?11:13);return;
 case 13:
  Round();Check(Acquire(Alpha),TEXT("same tick pickup fixture"));Place(Alpha,Zone->GetActorLocation()+FVector(-60,0,96));Core->Drop();Park();Advance(14,.9);return;
 case 14:
  // No UpdateExtraction since release: TryPickup itself must remember the deposit.
  Place(Alpha,Core->GetActorLocation()+FVector(-50,0,76));Check(Core->TryPickup(Alpha),TEXT("friendly pickup between extraction clock ticks"));Kill(EVanthexTeam::Bravo);Mode->TickRoundClock();
  Check(Mode->IsRoundActive()&&Mode->HasPendingDepositedExtraction(),TEXT("between-ticks deposit is retained"));
  Place(Alpha,FVector(50000,0,5000));Mode->TickRoundClock();Check(!Mode->IsRoundActive(),TEXT("carried Core leaving pad ends protection"));
  Round();Check(Acquire(Alpha),TEXT("ordinary held fixture"));Place(Alpha,Zone->GetActorLocation()+FVector(-60,0,96));Mode->UpdateExtraction(1);Kill(EVanthexTeam::Bravo);
  Check(!Mode->IsRoundActive(),TEXT("held Core without prior deposit does not bypass ordinary elimination"));Finish();return;
 }
#endif
}
