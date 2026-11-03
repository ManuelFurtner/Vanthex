#include "VanthexGameMode.h"
#if !UE_BUILD_SHIPPING
#include "VanthexCharacter.h"
#include "VanthexPlayerState.h"
#include "VanthexGameState.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexEconomyTypes.h"
#include "VanthexEconomyRewards.h"
#include "VanthexCore.h"
#include "VanthexNavigation.h"
#include "VanthexExtractionZone.h"
#include "VanthexDamage.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"
#endif

void VanthexRunEconomyIntegrationTest(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if (!Mode || !Mode->HasAuthority() || !Mode->Core || Mode->Combatants.Num()!=AVanthexGameMode::MaxPlayersPerTeam*2) return;
 // Legacy readiness requires MOVE_Walking, whereas this fixture must run during
 // the genuine buy phase with every pawn deliberately frozen in MOVE_None.
 for (AVanthexCharacter* Pawn : Mode->Combatants) {
  if (!Pawn || !Mode->GetVanthexPlayerState(Pawn) || !Pawn->GetWeapon() || !Pawn->GetEquipment() ||
   !Pawn->GetHealthComponent() || Pawn->GetHealthComponent()->IsDead()) return;
 }
 static bool Done = false;
 if (Done) return;
 Done = true;
 Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
 Mode->GetWorldTimerManager().ClearTimer(Mode->SelfTestTimer);
 TArray<FString> Lines;
 int32 Failures = 0;
 auto Check = [&](bool Value, const TCHAR* Name) { Lines.Add(FString::Printf(TEXT("%s %s"),Value?TEXT("PASS"):TEXT("FAIL"),Name)); if(!Value) ++Failures; };
 auto* Human = Mode->FindCombatant(EVanthexTeam::Alpha,0);
 auto* Ally = Mode->FindCombatant(EVanthexTeam::Alpha,1);
 auto* Ally2 = Mode->FindCombatant(EVanthexTeam::Alpha,2);
 auto* Enemy = Mode->FindCombatant(EVanthexTeam::Bravo,0);
 if (!Human || !Ally || !Ally2 || !Enemy) { Check(false,TEXT("six roster pawns ready")); }
 else {
 auto* H = Mode->GetVanthexPlayerState(Human);
 auto* A = Mode->GetVanthexPlayerState(Ally);
 auto* B = Mode->GetVanthexPlayerState(Enemy);
 uint32 Purchase = 0;
 Check(Mode->IsBuyPhase() && Mode->TimeRemaining > 22.f && Mode->TimeRemaining <= 25.f,TEXT("initial 25-second buy phase"));
 Check(Human->GetCharacterMovement()->MovementMode == MOVE_None,TEXT("buy phase freezes movement at spawn"));
 Check(H->GetEconomy().Credits==800 && H->GetEconomy().PrimaryWeapon.IsNone() && H->GetEconomy().Sidearm==TEXT("Pistol") && H->GetArmorPoints()==0,TEXT("match starts with 800 CR pistol knife no armor"));
 const int32 Ammo = Human->GetWeapon()->GetAmmoInMagazine();
 Human->GetWeapon()->FireShot(); Human->GetEquipment()->Attack();
 Check(Human->GetWeapon()->GetAmmoInMagazine()==Ammo && !Human->GetEquipment()->IsSwinging(),TEXT("buy phase rejects firearms and melee"));
 Check(!Mode->Core->TryPickup(Human),TEXT("buy phase rejects Core interaction"));
 Check(H->TryPurchase(TEXT("P250"),++Purchase) && H->TryPurchase(TEXT("ArmorLight"),++Purchase) && H->GetEconomy().Credits==50,TEXT("round one P250 and armor cost 750"));
 Check(!H->TryPurchase(TEXT("ArmorLight"),Purchase) && H->GetEconomy().Credits==50,TEXT("duplicate purchase cannot charge twice"));
 Check(!H->TryPurchase(TEXT("AUG"),++Purchase) && !H->TryPurchase(TEXT("invalid"),++Purchase) && H->GetEconomy().Credits==50,TEXT("insufficient credits and invalid IDs leave account unchanged"));
 H->AwardCredits(100000); Check(H->GetEconomy().Credits==7000,TEXT("credit cap 7000"));
 H->ResetEconomyForMatch(); Purchase=0;
 Check(H->TryPurchase(TEXT("ArmorHeavy"),++Purchase),TEXT("heavy armor purchase"));
 Mode->ActivateRound(); Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
 Check(Mode->IsRoundActive() && Mode->TimeRemaining==180.f && Human->GetCharacterMovement()->MovementMode==MOVE_Walking,TEXT("active round unlocks movement with 180 seconds"));
 Check(!H->TryPurchase(TEXT("P250"),++Purchase),TEXT("active round rejects purchases"));
 FVanthexDamageRequest Hit; Hit.InstigatorActor=Enemy; Hit.InstigatorController=Enemy->GetController(); Hit.DamageKind=EVanthexDamageKind::Bullet; Hit.Damage=40;
 Human->GetHealthComponent()->ApplyDamage(Hit);
 Check(FMath::IsNearlyEqual(Human->GetHealthComponent()->GetHealth(),76.f) && FMath::IsNearlyEqual(H->GetArmorPoints(),64.f),TEXT("real body hit heavy armor absorbs16 HP loss24"));
 Hit.bHeadshot=true; Hit.Damage=20; Human->GetHealthComponent()->ApplyDamage(Hit);
 Check(FMath::IsNearlyEqual(Human->GetHealthComponent()->GetHealth(),56.f) && H->GetArmorPoints()==64.f,TEXT("headshot bypasses armor"));
 Hit.bHeadshot=false; Hit.DamageKind=EVanthexDamageKind::Melee; Hit.Damage=50; Human->GetHealthComponent()->ApplyDamage(Hit);
 Check(FMath::IsNearlyEqual(Human->GetHealthComponent()->GetHealth(),6.f) && H->GetArmorPoints()==64.f,TEXT("knife bypasses armor"));
 Mode->ResetCombatants();
 Check(Human->GetHealthComponent()->GetHealth()==100.f && H->GetArmorPoints()==64.f,TEXT("survivor keeps damaged armor while HP restore"));
 Hit.Damage=100; Human->GetHealthComponent()->ApplyDamage(Hit);
 Check(H->GetEconomy().PrimaryWeapon.IsNone() && H->GetEconomy().Sidearm==TEXT("Pistol") && H->GetArmorPoints()==0 && H->GetEconomy().Credits==50,TEXT("death removes equipment but preserves credits"));
 Mode->ResetCombatants();
 Check(Human->GetWeapon()->GetWeaponId()==TEXT("Pistol") && Human->GetHealthComponent()->GetHealth()==100,TEXT("next round restores free pistol and full health"));

 // Kill and assist accounting uses the real damage entry point and capped HP.
 auto ResetCombat = [&]() { Mode->Phase=EVanthexRoundPhase::RoundActive; for(AVanthexCharacter* C:Mode->Combatants) { auto* S=Mode->GetVanthexPlayerState(C); S->ResetEconomyForMatch(); S->SetAlive(true); C->GetHealthComponent()->ResetHealth(); } };
 auto Damage = [&](AVanthexCharacter* Source, float Amount) { FVanthexDamageRequest R; R.InstigatorActor=Source; R.InstigatorController=Source->GetController(); R.DamageKind=EVanthexDamageKind::Bullet; R.Damage=Amount; Enemy->GetHealthComponent()->ApplyDamage(R); };
 ResetCombat(); Damage(Ally,29); Damage(Human,1000);
 Check(A->GetEconomy().Credits==800 && H->GetEconomy().Credits==1000 && Enemy->GetHealthComponent()->GetLastAppliedHPDamage()==71,TEXT("29 HP no assist and overkill excluded"));
 Damage(Human,1000); Check(H->GetEconomy().Credits==1000,TEXT("dead target cannot pay another kill"));
 ResetCombat(); Damage(Ally,30); Damage(Human,70);
 Check(A->GetEconomy().Credits==900 && H->GetEconomy().Credits==1000,TEXT("30 HP earns single assist and killer gets only kill"));
 ResetCombat(); Damage(Ally,30);
 for(auto& Sample:Mode->DamageHistory.FindChecked(Enemy)) Sample.Time=Mode->GetWorld()->GetTimeSeconds()-8.01;
 Damage(Human,70); Check(A->GetEconomy().Credits==800,TEXT("damage older than eight seconds excluded"));
 ResetCombat(); Damage(Ally,30);
 for(auto& Sample:Mode->DamageHistory.FindChecked(Enemy)) Sample.Time=Mode->GetWorld()->GetTimeSeconds()-8.;
 Damage(Human,70); Check(A->GetEconomy().Credits==900,TEXT("damage exactly eight seconds old still earns assist"));
 ResetCombat(); Damage(Ally,30); Damage(Ally2,30); Damage(Human,40);
 Check(A->GetEconomy().Credits==900 && Mode->GetVanthexPlayerState(Ally2)->GetEconomy().Credits==800,TEXT("equal-time equal-damage assist chooses lower stable slot"));
 ResetCombat(); Damage(Ally,40); FVanthexDamageRequest AllyDeath; AllyDeath.Damage=100; Ally->GetHealthComponent()->ApplyDamage(AllyDeath); Damage(Human,60);
 Check(A->GetEconomy().Credits==900,TEXT("dead assisting teammate remains eligible"));
 ResetCombat(); Damage(Ally,40); FVanthexDamageRequest Environment; Environment.Damage=100; Enemy->GetHealthComponent()->ApplyDamage(Environment);
 Check(A->GetEconomy().Credits==800,TEXT("environmental death gives no assist"));
 ResetCombat(); Damage(Ally,40); Enemy->GetHealthComponent()->ResetHealth(); Damage(Human,100);
 Check(A->GetEconomy().Credits==800,TEXT("healing clears damage ledger"));

 ResetCombat(); Mode->AlphaLossStreak=Mode->BravoLossStreak=0; Mode->RoundIndex=10; Mode->LastSettledRound=-1;
 Mode->SettleRoundEconomy(EVanthexTeam::Alpha,true);
 Check(H->GetEconomy().Credits==4100 && B->GetEconomy().Credits==2700,TEXT("extraction pays win3000 plus300 and first loss1900"));
 Mode->SettleRoundEconomy(EVanthexTeam::Alpha,true); Check(H->GetEconomy().Credits==4100,TEXT("round settlement is idempotent"));
 ++Mode->RoundIndex; B->ResetEconomyForMatch(); Mode->SettleRoundEconomy(EVanthexTeam::Alpha,false); Check(B->GetEconomy().Credits==3200,TEXT("second team loss pays2400"));
 ++Mode->RoundIndex; B->ResetEconomyForMatch(); Mode->SettleRoundEconomy(EVanthexTeam::Alpha,false); Check(B->GetEconomy().Credits==3700,TEXT("third team loss pays2900"));
 ++Mode->RoundIndex; B->ResetEconomyForMatch(); Mode->SettleRoundEconomy(EVanthexTeam::None,false); Check(B->GetEconomy().Credits==2700 && Mode->BravoLossStreak==3,TEXT("draw pays1900 without changing loss streak"));
 ++Mode->RoundIndex; Mode->SettleRoundEconomy(EVanthexTeam::Bravo,false); Check(Mode->BravoLossStreak==0 && Mode->AlphaLossStreak==1,TEXT("win resets team loss streak"));
 Mode->Phase=EVanthexRoundPhase::MatchEnded; Check(!H->TryPurchase(TEXT("P250"),++Purchase),TEXT("match end rejects all purchases"));

 Check(!VanthexEconomyRewards::QualifiesPass(399,1200,700) && VanthexEconomyRewards::QualifiesPass(400,1200,800) && !VanthexEconomyRewards::QualifiesPass(500,1200,801),TEXT("pass exact 399/400 travel and route-progress boundaries"));
 Check(!VanthexEconomyRewards::QualifiesPass(500,TNumericLimits<float>::Max(),500),TEXT("unreachable route never pays pass"));
 ResetCombat(); Mode->ProcessedThrows.Reset(); Mode->PaidIntercepts.Reset();
 Mode->NotifyCoreCaught(1,Human,Enemy,FVector::ZeroVector,FVector(800,0,0));
 Mode->NotifyCoreCaught(1,Human,Enemy,FVector::ZeroVector,FVector(800,0,0));
 Mode->NotifyCoreCaught(2,Human,Enemy,FVector::ZeroVector,FVector(800,0,0));
 Check(B->GetEconomy().Credits==900 && H->GetEconomy().Credits==800,TEXT("intercept once per player round no passer reward or duplicate"));
 // Find a real same-floor improvement on the current navigation graph.
 FVector PassStart = Mode->Core->GetHome()+FVector(0,0,30), PassEnd;
 bool HasRoute = false;
 for(TActorIterator<AVanthexExtractionZone> It(Mode->GetWorld()); It && !HasRoute; ++It) {
  TArray<FVector> Route;
  if(!VanthexNavigation::FindRoute(Mode->GetWorld(),PassStart,It->GetActorLocation()+FVector(0,0,96),Route)) continue;
  for(const FVector& Point:Route) if(VanthexEconomyRewards::QualifiesPass(FVector::Dist(PassStart,Point),Mode->ShortestExtractionRoute(PassStart),Mode->ShortestExtractionRoute(Point))) { PassEnd=Point; HasRoute=true; break; }
 }
 Check(HasRoute,TEXT("Core pass fixture uses reachable extraction progress on collision graph"));
 if(HasRoute) {
  Mode->PaidPasses.Reset(); Mode->PaidPassPairs.Reset(); Mode->AlphaPasses=0;
  Mode->NotifyCoreCaught(10,Human,Ally,PassStart,PassEnd);
  Mode->NotifyCoreCaught(10,Human,Ally,PassStart,PassEnd);
  Mode->NotifyCoreCaught(11,Human,Ally,PassStart,PassEnd);
  Check(H->GetEconomy().Credits==850,TEXT("paid pass once per throw and directed pair"));
  Mode->NotifyCoreCaught(12,Human,Ally2,PassStart,PassEnd);
  Mode->NotifyCoreCaught(13,Human,Ally2,PassStart,PassEnd);
  Check(H->GetEconomy().Credits==900 && Mode->PaidPasses.FindRef(0)==2,TEXT("passer maximum two paid passes"));
  Mode->NotifyCoreCaught(14,Ally,Human,PassStart,PassEnd);
  Mode->NotifyCoreCaught(15,Ally,Ally2,PassStart,PassEnd);
  const int32 BeforeTeamCap=Mode->GetVanthexPlayerState(Ally2)->GetEconomy().Credits;
  Mode->NotifyCoreCaught(16,Ally2,Human,PassStart,PassEnd);
  Check(Mode->AlphaPasses==4 && Mode->GetVanthexPlayerState(Ally2)->GetEconomy().Credits==BeforeTeamCap,TEXT("team maximum four paid passes"));
 }
 H->ResetEconomyForMatch();
 Mode->Phase=EVanthexRoundPhase::BuyPhase; Mode->NotifyCoreCaught(3,Human,Ally,FVector::ZeroVector,FVector(800,0,0)); Check(H->GetEconomy().Credits==800,TEXT("inactive throw cannot award credits"));
 H->AwardCredits(123); auto* Transfer=Mode->GetVanthexPlayerState(Ally2); Transfer->CopyEconomyFrom(H);
 Check(Transfer->GetEconomy().Credits==923 && Transfer->GetEconomy().Sidearm==H->GetEconomy().Sidearm,TEXT("slot handover copies balance and inventory without reset"));
 const auto OriginalTeam=H->GetTeam(); H->SetTeam(EVanthexTeam::Bravo,2); Check(H->GetTeam()==OriginalTeam,TEXT("mid-match team switch rejected"));
 Mode->RestartMatch(); Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
 Check(Mode->IsBuyPhase() && H->GetEconomy().Credits==800 && Mode->AlphaLossStreak==0 && Mode->BravoLossStreak==0,TEXT("new match resets accounts series and enters buy phase"));
 }
 Lines.Insert(FString::Printf(TEXT("OVERALL %s\nCHECKS %d\nFAILURES %d"),Failures?TEXT("FAIL"):TEXT("PASS"),Lines.Num(),Failures),0);
 FFileHelper::SaveStringToFile(FString::Join(Lines,TEXT("\n"))+TEXT("\n"),*(FPaths::ProjectSavedDir()/TEXT("EconomyIntegration.txt")));
 FPlatformMisc::RequestExitWithStatus(false, Failures?1:0);
#endif
}
