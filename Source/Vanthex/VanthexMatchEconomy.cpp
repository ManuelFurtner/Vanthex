#include "VanthexGameMode.h"
#include "VanthexCharacter.h"
#include "VanthexPlayerState.h"
#include "VanthexBotController.h"
#include "VanthexCore.h"
#include "VanthexExtractionZone.h"
#include "VanthexNavigation.h"
#include "VanthexDamage.h"
#include "VanthexEconomyRewards.h"
#include "HealthComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool AVanthexGameMode::UsesLegacyCombatFixture() const
{
#if !UE_BUILD_SHIPPING
 // Existing diagnostics explicitly retain the original 40-damage rifle fixture.
 // Real matches and the economy integration/network tests use the purchase rules.
 for (const TCHAR* Flag : {TEXT("VanthexSelfTest"),TEXT("GridV4Test"),TEXT("GridV4GameTest"),TEXT("GridV4Shots"),TEXT("GridV3Test"),TEXT("GridV3GameTest"),TEXT("OperatorTest"),TEXT("OperatorShots"),TEXT("GridV3Shots"),TEXT("VanthexMapShots"),TEXT("VanthexVisualShots"),TEXT("VanthexNetworkProbe"),TEXT("EquipmentNetworkTest"),TEXT("BulletAudioNetworkTest"),TEXT("BulletAudioPlaybackTest"),TEXT("VanthexEnergyTest"),TEXT("VanthexMapTest")})
  if (FParse::Param(FCommandLine::Get(), Flag)) return true;
#endif
 return false;
}

void AVanthexGameMode::TickBuyPhase()
{
 if (!IsBuyPhase()) return;
 TimeRemaining = FMath::Max(0.f, TimeRemaining - .1f);
 PublishMatch();
 if (TimeRemaining <= .001f) ActivateRound();
}

void AVanthexGameMode::BuyForBots()
{
 if (!IsBuyPhase() || IsCTF()) return;
#if !UE_BUILD_SHIPPING
 if(FParse::Param(FCommandLine::Get(),TEXT("AimTest"))||FParse::Param(FCommandLine::Get(),TEXT("AimShots"))||FParse::Param(FCommandLine::Get(),TEXT("AimNetworkTest")))return;
 if(FParse::Param(FCommandLine::Get(),TEXT("GadgetTest"))||FParse::Param(FCommandLine::Get(),TEXT("GadgetNetworkTest"))||FParse::Param(FCommandLine::Get(),TEXT("GadgetShots"))||FParse::Param(FCommandLine::Get(),TEXT("WeaponModelShots")))return;
 if(FParse::Param(FCommandLine::Get(),TEXT("EconomyTest")) || FParse::Param(FCommandLine::Get(),TEXT("EconomyNetworkTest")) || FParse::Param(FCommandLine::Get(),TEXT("CoreEconomyTest")) || FParse::Param(FCommandLine::Get(),TEXT("EconomyShots")) || FParse::Param(FCommandLine::Get(),TEXT("WeaponProfileTest"))) return;
#endif
 for (AVanthexCharacter* Character : Combatants) {
  if (!Character || !Cast<AVanthexBotController>(Character->GetController())) continue;
  auto* State = GetVanthexPlayerState(Character);
  if (!State) continue;
  // Bots pay the identical catalog prices and retain surviving equipment.
  uint32 Request = uint32(RoundIndex) * 16;
  if (State->GetEconomy().PrimaryWeapon.IsNone()) {
   if (!State->TryPurchase(TEXT("AUG"), ++Request))
    if (!State->TryPurchase(TEXT("MP7"), ++Request)) State->TryPurchase(TEXT("MP5"), ++Request);
  }
  if (!State->TryPurchase(TEXT("ArmorHeavy"), ++Request)) State->TryPurchase(TEXT("ArmorLight"), ++Request);
  if (State->GetEconomy().PrimaryWeapon.IsNone() && State->GetEconomy().Sidearm == TEXT("Pistol")) State->TryPurchase(TEXT("P250"), ++Request);
  if(State->GetEconomy().Gadgets.Num()<2)State->TryPurchase(State->GetSpawnSlot()==0?TEXT("Smoke"):State->GetSpawnSlot()==1?TEXT("Frag"):TEXT("Sensor"),++Request);
  Character->SyncEconomyLoadout(false);
 }
}

int32 AVanthexGameMode::EconomySlot(const AVanthexPlayerState* State) const
{
 if (!State || State->GetTeam() == EVanthexTeam::None || State->GetSpawnSlot() < 0 || State->GetSpawnSlot() >= TeamSize()) return -1;
 return (State->GetTeam() == EVanthexTeam::Bravo ? TeamSize() : 0) + State->GetSpawnSlot();
}

AVanthexPlayerState* AVanthexGameMode::EconomyStateForSlot(int32 Key) const
{
 for (const AVanthexCharacter* Character : Combatants) {
  auto* State = GetVanthexPlayerState(Character);
  if (Key >= 0 && EconomySlot(State) == Key) return State;
 }
 return nullptr;
}

void AVanthexGameMode::SettleRoundEconomy(EVanthexTeam Winner, bool bExtraction)
{
 if (!HasAuthority() || LastSettledRound == RoundIndex) return;
 LastSettledRound = RoundIndex;
 if (Winner == EVanthexTeam::Alpha) { AlphaLossStreak = 0; ++BravoLossStreak; }
 else if (Winner == EVanthexTeam::Bravo) { BravoLossStreak = 0; ++AlphaLossStreak; }
 for (AVanthexCharacter* Character : Combatants) {
  auto* State = GetVanthexPlayerState(Character);
  if (!State || State->GetTeam() == EVanthexTeam::None) continue;
  if (Winner == EVanthexTeam::None) State->AwardCredits(1900);
  else if (State->GetTeam() == Winner) {
   State->AwardCredits(3000);
   if (bExtraction) State->AwardCredits(300);
  } else {
   const int32 Streak = State->GetTeam() == EVanthexTeam::Alpha ? AlphaLossStreak : BravoLossStreak;
   State->AwardCredits(Streak <= 1 ? 1900 : Streak == 2 ? 2400 : 2900);
  }
 }
}

void AVanthexGameMode::ClearDamageHistory(AVanthexCharacter* Victim)
{
 if (!Victim) return;
 DamageHistory.Remove(Victim);
 RewardedDeaths.Remove(Victim);
}

void AVanthexGameMode::RecordDamage(AVanthexCharacter* Victim, const FVanthexDamageRequest& Request, float AppliedHP, bool bKilled)
{
 if (!HasAuthority() || !IsRoundActive() || !Victim || !FMath::IsFinite(AppliedHP) || AppliedHP <= 0.f || RewardedDeaths.Contains(Victim)) return;
 auto* VictimState = GetVanthexPlayerState(Victim);
 auto* AttackerState = GetVanthexPlayerState(Request.InstigatorController ? static_cast<AActor*>(Request.InstigatorController) : Request.InstigatorActor);
 const int32 Attacker = EconomySlot(AttackerState);
 const bool Enemy = Request.DamageKind != EVanthexDamageKind::Environment && VictimState && Attacker >= 0 && AttackerState != VictimState && AttackerState->GetTeam() != VictimState->GetTeam();
 const double Now = GetWorld()->GetTimeSeconds();
 auto& History = DamageHistory.FindOrAdd(Victim);
 History.RemoveAll([Now](const FDamageCredit& Hit) { return Now - Hit.Time > 8.; });
 if (Enemy) History.Add({Attacker, AppliedHP, Now});
 if (!bKilled) return;
 RewardedDeaths.Add(Victim);
 if(VictimState) VictimState->RecordDeath();
 // Environmental, friendly and self deaths never pay a killer or an assister.
 if (Enemy) {
  AttackerState->AwardCredits(200);
  AttackerState->RecordKill();
  TMap<int32, float> Totals;
  TMap<int32, double> Recent;
  for (const auto& Hit : History) {
   auto* Candidate = EconomyStateForSlot(Hit.Attacker);
   if (Hit.Attacker == Attacker || !Candidate || Candidate->GetTeam() != AttackerState->GetTeam()) continue;
   Totals.FindOrAdd(Hit.Attacker) += Hit.HP;
   Recent.FindOrAdd(Hit.Attacker) = FMath::Max(Recent.FindRef(Hit.Attacker), Hit.Time);
  }
  int32 Best = -1;
  for (const auto& Entry : Totals) {
   if (Entry.Value < 30.f) continue;
   if (Best < 0 || Entry.Value > Totals.FindRef(Best)
    || (Entry.Value == Totals.FindRef(Best) && (Recent.FindRef(Entry.Key) > Recent.FindRef(Best)
     || (Recent.FindRef(Entry.Key) == Recent.FindRef(Best) && Entry.Key < Best)))) Best = Entry.Key;
  }
  if (auto* Assister = EconomyStateForSlot(Best)) { Assister->AwardCredits(100); Assister->RecordAssist(); }
 }
 DamageHistory.Remove(Victim);
}

float AVanthexGameMode::ShortestExtractionRoute(const FVector& Position) const
{
 float Best = TNumericLimits<float>::Max();
 for (TActorIterator<AVanthexExtractionZone> It(GetWorld()); It; ++It) {
  float Distance = 0.f;
  if (!VanthexNavigation::FindRouteLength(GetWorld(), Position, It->GetActorLocation() + FVector(0,0,96), Distance)) continue;
  Best = FMath::Min(Best, Distance);
 }
 return Best;
}

void AVanthexGameMode::NotifyCoreCaught(uint32 ThrowId, AVanthexCharacter* Thrower, AVanthexCharacter* Catcher, const FVector& Origin, const FVector& CatchPosition)
{
 if (!HasAuthority() || !IsRoundActive() || !ThrowId || ProcessedThrows.Contains(ThrowId)) return;
 ProcessedThrows.Add(ThrowId);
 if (!Thrower || !Catcher || Thrower == Catcher || Thrower->GetHealthComponent()->IsDead() || Catcher->GetHealthComponent()->IsDead()) return;
 auto* From = GetVanthexPlayerState(Thrower);
 auto* To = GetVanthexPlayerState(Catcher);
 const int32 Sender = EconomySlot(From), Receiver = EconomySlot(To);
 if (Sender < 0 || Receiver < 0) return;
 if (From->GetTeam() != To->GetTeam()) {
  if (!PaidIntercepts.Contains(Receiver)) { PaidIntercepts.Add(Receiver); To->AwardCredits(100); }
  return;
 }
 int32& TeamPasses = From->GetTeam() == EVanthexTeam::Alpha ? AlphaPasses : BravoPasses;
 const int32 Pair = Sender * (TeamSize()*2) + Receiver;
 if (PaidPasses.FindRef(Sender) >= 2 || TeamPasses >= 4 || PaidPassPairs.Contains(Pair) || FVector::Dist(Origin, CatchPosition) < 400.f) return;
 const float Before = ShortestExtractionRoute(Origin), After = ShortestExtractionRoute(CatchPosition);
 if (!VanthexEconomyRewards::QualifiesPass(FVector::Dist(Origin, CatchPosition), Before, After)) return;
 ++PaidPasses.FindOrAdd(Sender); ++TeamPasses; PaidPassPairs.Add(Pair);
 From->AwardCredits(50);
}
