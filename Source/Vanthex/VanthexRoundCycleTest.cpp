#include "VanthexGameMode.h"
#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexBotController.h"
#include "VanthexCharacter.h"
#include "VanthexCore.h"
#include "VanthexExtractionZone.h"
#include "VanthexPlayerState.h"
#include "VanthexTestArena.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"
#endif

// Dedicated-server integration test driven by real timers and two unmodified
// packaged Windows clients. Only enabled by an explicit server command line.
void VanthexRunRoundCycleTest(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if (!Mode || !Mode->HasAuthority()) return;
 struct FProbe {
  TWeakObjectPtr<UWorld> World;
  TArray<TWeakObjectPtr<APlayerController>> Players;
  int32 Step = 0, Checks = 0;
  double Started = 0, EndedAt = 0;
  bool SawBuy = false, CheckedDuplicate = false;
 };
 static FProbe Probe;
 auto* World = Mode->GetWorld();
 const double Now = FPlatformTime::Seconds();
 if (Probe.World.Get() != World) { Probe = FProbe(); Probe.World = World; Probe.Started = Now; }
 if (Probe.Step == 100) return;
 auto Fail = [&](const TCHAR* Reason) {
  UE_LOG(LogVanthex, Error, TEXT("ROUND_CYCLE FAIL: %s"), Reason);
  Probe.Step = 100; Mode->GetWorldTimerManager().ClearTimer(Mode->SelfTestTimer);
 };
 auto Check = [&](bool OK, const TCHAR* Reason) {
  if (!OK) { Fail(Reason); return false; }
  ++Probe.Checks; UE_LOG(LogVanthex, Display, TEXT("ROUND_CYCLE CHECK: %s"), Reason); return true;
 };
 if (Now - Probe.Started > 145) { Fail(TEXT("timed out")); return; }
 int32 HumanCount = 0;
 for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
  if (It->IsValid() && It->Get()->GetNetConnection()) ++HumanCount;
 if (Probe.Step == 0 && (HumanCount != 2 || Mode->Combatants.Num() != 6 || !Mode->Core)) return;
 if (Probe.Step > 0 && HumanCount != 2) { Fail(TEXT("both original clients remain connected")); return; }
 for (AVanthexCharacter* Combatant : Mode->Combatants) {
  if (!Combatant) continue;
  if (auto* Bot = Cast<AVanthexBotController>(Combatant->GetController())) Bot->SetActorTickEnabled(false);
  if (Combatant->GetWeapon()) Combatant->GetWeapon()->StopFire();
  Combatant->GetCharacterMovement()->StopMovementImmediately();
  Combatant->GetCharacterMovement()->DisableMovement();
 }
 if (Mode->IsBuyPhase()) Probe.SawBuy = true;
 auto RoundReady = [&](int32 Index) {
  if (Mode->RoundIndex != Index || !Mode->IsRoundActive()) return false;
  return Check(Probe.SawBuy && FMath::IsNearlyEqual(Mode->BuyDuration, 12.f)
   && Mode->CountAlive(EVanthexTeam::Alpha) == 3 && Mode->CountAlive(EVanthexTeam::Bravo) == 3,
   TEXT("automatic next round preserves buy phase and revives all six players"));
 };
 switch (Probe.Step) {
 case 0:
  if (!Mode->IsRoundActive()) return;
  for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
   if (It->IsValid() && It->Get()->GetNetConnection()) Probe.Players.Add(It->Get());
  Mode->KillTeam(EVanthexTeam::Bravo);
  if (!Check(Mode->RoundWinner == EVanthexTeam::Alpha && Mode->AlphaScore == 1, TEXT("elimination ends round once"))) return;
  Probe.SawBuy = false; Probe.Step = 1; return;
 case 1:
  if (!RoundReady(2)) return;
  Mode->TimeRemaining = .1f; Probe.SawBuy = false; Probe.Step = 2; return;
 case 2:
  if (!RoundReady(3)) return;
  if (!Check(Mode->AlphaScore == 1 && Mode->BravoScore == 0, TEXT("timeout draw preserves score"))) return;
  {
   auto* Alpha = Mode->FindCombatant(EVanthexTeam::Alpha, 0);
   auto* Zone = Mode->Arena->GetExtractionZone(EVanthexExtractionId::A);
   if (!Alpha || !Zone) { Fail(TEXT("extraction fixture")); return; }
   Mode->Core->ResetToSpawn();
   Alpha->SetActorLocation(Mode->Core->GetActorLocation() + FVector(0,0,60), false, nullptr, ETeleportType::TeleportPhysics);
   if (!Check(Mode->Core->TryPickup(Alpha), TEXT("core pickup before real extraction"))) return;
   Alpha->GetController()->SetControlRotation(FRotator::ZeroRotator);
   Alpha->SetActorRotation(FRotator::ZeroRotator);
   Alpha->SetActorLocation(Zone->GetActorLocation() + FVector(-60,0,96), false, nullptr, ETeleportType::TeleportPhysics);
  }
  Probe.SawBuy = false; Probe.Step = 3; return;
 case 3:
  if (!RoundReady(4)) return;
  if (!Check(Mode->AlphaScore == 2 && Mode->Core->IsAtHome(), TEXT("extraction awards score and resets core next round"))) return;
  Mode->AlphaScore = AVanthexGameMode::RoundsToWin - 1;
  Mode->KillTeam(EVanthexTeam::Bravo);
  if (!Check(Mode->Phase == EVanthexRoundPhase::MatchEnded && Mode->AlphaScore == 7
    && Mode->MatchWinner == EVanthexTeam::Alpha, TEXT("seventh win enters match results"))) return;
  Probe.EndedAt = Now; Probe.Step = 4; return;
 case 4:
  if (Mode->Phase == EVanthexRoundPhase::MatchEnded) {
   if (!Probe.CheckedDuplicate && Now - Probe.EndedAt > 2) {
    const float Before = Mode->TimeRemaining;
    Mode->EnterMatchEnded(); Mode->EndRound(EVanthexTeam::Alpha);
    if (!Check(Before < 9.f && FMath::IsNearlyEqual(Before, Mode->TimeRemaining) && Mode->AlphaScore == 7,
      TEXT("countdown advances and duplicate end notifications cannot reset it"))) return;
    Probe.CheckedDuplicate = true;
   }
   return;
  }
  if (!Check(Probe.CheckedDuplicate && Now - Probe.EndedAt >= 9.5 && Now - Probe.EndedAt < 13
    && Mode->IsBuyPhase() && Mode->RoundIndex == 1 && Mode->AlphaScore == 0 && Mode->BravoScore == 0
    && Mode->MatchWinner == EVanthexTeam::None && Mode->RoundWinner == EVanthexTeam::None
    && Mode->Core->IsAtHome(), TEXT("match restarts automatically after ten seconds with cleared score and core"))) return;
  for (const auto& Player : Probe.Players) {
   auto* State = Player.IsValid() ? Player->GetPlayerState<AVanthexPlayerState>() : nullptr;
   auto* Pawn = Player.IsValid() ? Cast<AVanthexCharacter>(Player->GetPawn()) : nullptr;
   if (!Check(State && Pawn && Player->GetNetConnection() && !Pawn->GetHealthComponent()->IsDead()
     && State->GetKills() == 0 && State->GetDeaths() == 0 && State->GetAssists() == 0
     && State->GetEconomy().Credits == FVanthexEconomyState().Credits,
     TEXT("original human connection retained with reset health, credits and statistics"))) return;
  }
  Probe.SawBuy = true; Probe.Step = 5; return;
 case 5:
  if (!RoundReady(1)) return;
  UE_LOG(LogVanthex, Display, TEXT("ROUND_CYCLE PASS checks=%d clients=2 elimination=1 timeout=1 extraction=1 automatic_rematch=1"), Probe.Checks);
  Probe.Step = 100; Mode->GetWorldTimerManager().ClearTimer(Mode->SelfTestTimer); return;
 default: return;
 }
#endif
}
