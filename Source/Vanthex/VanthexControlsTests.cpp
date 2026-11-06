#include "VanthexGameMode.h"
#include "VanthexTacticalHUD.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexMinimapPrivacyTest,"Vanthex.HUD.MinimapNoEnemies",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVanthexMinimapPrivacyTest::RunTest(const FString&)
{
 for(auto Team:{EVanthexTeam::None,EVanthexTeam::Alpha,EVanthexTeam::Bravo})
  for(auto Other:{EVanthexTeam::None,EVanthexTeam::Alpha,EVanthexTeam::Bravo})
   for(bool Alive:{false,true}) for(bool Self:{false,true}) {
    const bool Visible=VanthexTacticalHUD::ShowTeammate(Team,Other,Alive,Self);
    TestEqual(TEXT("only living non-self teammates appear; no enemy or unassigned player leaks"),Visible,Team!=EVanthexTeam::None&&Team==Other&&Alive&&!Self);
   }
 TestEqual(TEXT("lower floor"),VanthexTacticalHUD::Floor(-502.f),-1);
 TestEqual(TEXT("main floor"),VanthexTacticalHUD::Floor(98.f),0);
 TestEqual(TEXT("upper floor"),VanthexTacticalHUD::Floor(698.f),1);
 return true;
}
#endif
#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexPlayerController.h"
#include "VanthexPlayerState.h"
#include "VanthexGameState.h"
#include "VanthexBotController.h"
#include "VanthexBuyMenuComponent.h"
#include "VanthexEquipmentComponent.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "Framework/Application/SlateApplication.h"
#include "InputKeyEventArgs.h"
#endif

void VanthexRunControlsTest(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode||!Mode->HasAuthority()) return;
 static TWeakObjectPtr<UWorld> Fixture; static int32 Step=0,Failures=0; static double Start=0,Next=0;
 static float BuyRemaining=0,StartZ=0,SecondJumpVelocity=0; static TArray<FString> Lines;
 const double Now=FPlatformTime::Seconds();
 if(Fixture.Get()!=Mode->GetWorld()) { Fixture=Mode->GetWorld(); Step=Failures=0; Start=Now; Next=0; Lines.Reset(); }
 if(Step==100) return;
 auto Check=[&](bool Pass,const TCHAR* What){Lines.Add(FString(Pass?TEXT("PASS "):TEXT("FAIL "))+What);if(!Pass)++Failures;};
 auto Finish=[&](){Step=100;const FString Report=FString::Printf(TEXT("OVERALL %s CHECKS %d FAILURES %d\n"),Failures?TEXT("FAIL"):TEXT("PASS"),Lines.Num(),Failures)+FString::Join(Lines,TEXT("\n"));FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("ControlsTest.txt")));UE_LOG(LogVanthex,Display,TEXT("CONTROLS_TEST %s"),*Report);FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);};
 if(Now-Start>40) {Check(false,TEXT("bounded test completes"));Finish();return;}
 if(Now<Next)return;
 auto* Pawn=Mode->FindCombatant(EVanthexTeam::Alpha,0); auto* PC=Pawn?Cast<AVanthexPlayerController>(Pawn->GetController()):nullptr;
 if(!Pawn||!PC||!Pawn->GetWeapon()||!Pawn->GetEquipment())return;
 auto* E=Pawn->GetEquipment(); auto* W=Pawn->GetWeapon();
 auto Advance=[&](){++Step;Next=Now+.7;};
 switch(Step) {
 case 0:
  Check(Mode->IsBuyPhase(),TEXT("fixture starts in real buy phase"));
  BuyRemaining=Mode->GetWorld()->GetGameState<AVanthexGameState>()->GetTimeRemaining();
  Check(BuyRemaining>9&&BuyRemaining<=12,TEXT("initial buy duration is twelve seconds")); Start=Now;
  PC->GetBuyMenu()->Close();
  for(TActorIterator<AVanthexCharacter> It(Mode->GetWorld());It;++It)if(auto* Bot=Cast<AVanthexBotController>(It->GetController()))Bot->SetActorTickEnabled(false);
  E->SyncLoadout(NAME_None,TEXT("Pistol"),true);E->SelectRoundWeapon(); Pawn->NextWeapon();Advance();return;
 case 1:
  Check(FMath::IsNearlyEqual(Pawn->GetCharacterMovement()->MaxWalkSpeed,600.f)&&FMath::IsNearlyEqual(Pawn->GetCharacterMovement()->MaxWalkSpeedCrouched,280.f),TEXT("drawn knife uses standard walk and crouch speed"));
  Check(E->GetSelectedSlot()==EVanthexWeaponSlot::Knife&&W->GetWeaponId()==TEXT("Knife"),TEXT("wheel down selects knife during buy phase"));
  E->Attack();Check(!E->IsSwinging(),TEXT("buy phase still forbids knife attacks"));Pawn->NextWeapon();Advance();return;
 case 2:
  Check(FMath::IsNearlyEqual(Pawn->GetCharacterMovement()->MaxWalkSpeed,510.f)&&FMath::IsNearlyEqual(Pawn->GetCharacterMovement()->MaxWalkSpeedCrouched,238.f),TEXT("pistol reduces walk and crouch speed by fifteen percent"));
  Check(E->GetSelectedSlot()==EVanthexWeaponSlot::Pistol&&W->GetWeaponId()==TEXT("Pistol"),TEXT("wheel wraps and skips absent primary"));
  {const int32 Ammo=W->GetAmmoInMagazine(); W->FireShot();Check(W->GetAmmoInMagazine()==Ammo,TEXT("buy phase still forbids firearm shots"));}
  {auto* PS=Pawn->GetPlayerState<AVanthexPlayerState>();PS->AwardCredits(4000);
   Check(PS->TryPurchase(TEXT("M4"),901)&&PS->TryPurchase(TEXT("P250"),902),TEXT("preparation buys actual primary and sidearm"));Pawn->SyncEconomyLoadout(false);}
  Pawn->PreviousWeapon();Advance();return;
 case 3:
  Check(FMath::IsNearlyEqual(Pawn->GetCharacterMovement()->MaxWalkSpeed,510.f),TEXT("rifle uses reduced movement speed"));
  Check(E->GetSelectedSlot()==EVanthexWeaponSlot::Rifle&&W->GetWeaponId()==TEXT("M4"),TEXT("wheel up selects owned primary"));Pawn->PreviousWeapon();Advance();return;
 case 4:
  Check(E->GetSelectedSlot()==EVanthexWeaponSlot::Knife,TEXT("wheel up wraps primary to knife"));E->SelectPistol();Advance();return;
 case 5:
  Check(E->GetSelectedSlot()==EVanthexWeaponSlot::Pistol&&W->GetWeaponId()==TEXT("P250"),TEXT("number-slot selection also works in buy phase"));
  PC->GetBuyMenu()->Toggle();
  Check(PC->GetBuyMenu()->IsOpen(),TEXT("purchase menu opens before keyboard focus test"));Advance();return;
 case 6:
  FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Tab,FModifierKeysState(),0,false,0,0));
  Check(!PC->GetBuyMenu()->IsOpen()&&PC->IsScoreboardOpen(),TEXT("real Tab event replaces focused purchase menu with scoreboard"));
  Advance();return;
 case 7:
  FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Tab,FModifierKeysState(),0,false,0,0));
  Check(!Pawn->CanJump(),TEXT("jump is disabled during preparation"));Advance();return;
 case 8:
  if(!Mode->IsRoundActive())return;
  Check(!PC->IsScoreboardOpen(),TEXT("real Tab release closes scoreboard after leaving purchase UI"));
  Check(FMath::Abs((Now-Start)-BuyRemaining)<1.5,TEXT("real countdown enters combat after remaining twelve-second buy window"));
  Check(E->GetSelectedSlot()==EVanthexWeaponSlot::Pistol&&W->GetWeaponId()==TEXT("P250"),TEXT("buy-phase weapon selection survives round activation"));
  Check(Pawn->CanJump(),TEXT("living grounded character can jump during active play"));
  StartZ=Pawn->GetActorLocation().Z;
  PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar,IE_Pressed,1.f));
  ++Step;Next=Now+.15;return;
 case 9:
  Check(Pawn->GetCharacterMovement()->IsFalling()&&Pawn->GetActorLocation().Z>StartZ+15.f,TEXT("Space input launches character above the floor"));
  PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar,IE_Released,0.f));
  ++Step;Next=Now+.05;return;
 case 10:
  Check(!Pawn->CanJump(),TEXT("airborne character cannot double jump"));
  SecondJumpVelocity=Pawn->GetCharacterMovement()->Velocity.Z;
  PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar,IE_Pressed,1.f));
  ++Step;Next=Now+.1;return;
 case 11:
  Check(Pawn->GetCharacterMovement()->Velocity.Z<=SecondJumpVelocity+5.f,TEXT("second Space press cannot add upward velocity in midair"));
  PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar,IE_Released,0.f));
  ++Step;Next=Now+.8;return;
 case 12:
  Check(!Pawn->GetCharacterMovement()->IsFalling()&&FMath::Abs(Pawn->GetActorLocation().Z-StartZ)<5.f&&Pawn->CanJump(),TEXT("character lands and can jump again"));
  Pawn->SetCrouching(true);++Step;Next=Now+.1;return;
 case 13:
  Check(Pawn->bIsCrouched&&!Pawn->CanJump(),TEXT("crouched character cannot jump"));
  Pawn->SetCrouching(false);++Step;Next=Now+.1;return;
 case 14:
  {auto* Ally=Mode->FindCombatant(EVanthexTeam::Alpha,1);auto* Enemy=Mode->FindCombatant(EVanthexTeam::Bravo,0);
   auto* H=Pawn->GetPlayerState<AVanthexPlayerState>();auto* A=Ally->GetPlayerState<AVanthexPlayerState>();auto* B=Enemy->GetPlayerState<AVanthexPlayerState>();
   const int32 K=H->GetKills(),Assists=A->GetAssists(),D=B->GetDeaths();
   Enemy->GetHealthComponent()->ResetHealth();FVanthexDamageRequest Hit;Hit.DamageKind=EVanthexDamageKind::Melee;Hit.Damage=35;Hit.InstigatorActor=Ally;Hit.InstigatorController=Ally->GetController();Enemy->GetHealthComponent()->ApplyDamage(Hit);
   Hit.Damage=100;Hit.InstigatorActor=Pawn;Hit.InstigatorController=PC;Enemy->GetHealthComponent()->ApplyDamage(Hit);
   Check(H->GetKills()==K+1&&A->GetAssists()==Assists+1&&B->GetDeaths()==D+1,TEXT("real kill credits public K/A/D exactly once"));
   const auto* Match=Mode->GetWorld()->GetGameState<AVanthexGameState>();
   const int32 FeedCount=Match->GetEliminations().Num();
   Check(FeedCount>0&&Match->GetEliminations().Last().KillerId==H->GetPlayerId()&&Match->GetEliminations().Last().VictimId==B->GetPlayerId(),TEXT("lethal damage publishes the actual killer and victim"));
   Enemy->GetHealthComponent()->ApplyDamage(Hit);
   Check(Match->GetEliminations().Num()==FeedCount,TEXT("corpse hits cannot duplicate elimination messages"));
   Check(H->GetKills()==K+1&&B->GetDeaths()==D+1,TEXT("corpse hits cannot inflate scoreboard"));
   Check(!Enemy->CanJump(),TEXT("dead character cannot jump"));
   Enemy->PrepareForRound(Enemy->GetActorTransform());Check(B->GetDeaths()==D+1,TEXT("round respawn preserves match statistics"));}
  Finish();return;
 }
#endif
}
