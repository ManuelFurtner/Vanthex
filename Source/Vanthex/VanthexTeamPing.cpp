#include "VanthexPlayerController.h"

#include "HealthComponent.h"
#include "VanthexCharacter.h"
#include "VanthexGameState.h"
#include "VanthexPlayerState.h"
#include "Engine/World.h"

namespace
{
constexpr double TeamPingRange = 3500.0;
constexpr double TeamPingCooldown = 1.0;
constexpr double TeamPingDuration = 3.5;

const AVanthexPlayerState* GetLivingTeamState(const AVanthexPlayerController* Controller)
{
 const auto* Pawn = Cast<AVanthexCharacter>(Controller->GetPawn());
 const auto* Health = Pawn ? Pawn->GetHealthComponent() : nullptr;
 const auto* State = Controller->GetPlayerState<AVanthexPlayerState>();
 return State && State->GetTeam() != EVanthexTeam::None && State->IsAlive()
  && Health && !Health->IsDead() ? State : nullptr;
}
}

void AVanthexPlayerController::RequestTeamPing()
{
 if(CanPredictDamageFeedback() && !IsScoreboardOpen()) ServerPlaceTeamPing();
}

void AVanthexPlayerController::ServerPlaceTeamPing_Implementation()
{
 UWorld* World = GetWorld();
 const auto* Match = World ? World->GetGameState<AVanthexGameState>() : nullptr;
 const auto* Sender = GetLivingTeamState(this);
 if(!Match || Match->GetPhase() != EVanthexRoundPhase::RoundActive || !Sender) return;

 const double Now = World->GetTimeSeconds();
 if(Now < NextTeamPingAt) return;
 NextTeamPingAt = Now + TeamPingCooldown;

 const FVector Start = GetPawn()->GetPawnViewLocation();
 const FVector Direction = GetControlRotation().Vector();
 if(Start.ContainsNaN() || Direction.ContainsNaN() || Direction.IsNearlyZero()) return;

 FHitResult Hit;
 const FCollisionQueryParams Query(SCENE_QUERY_STAT(VanthexTeamPing), false, GetPawn());
 FCollisionObjectQueryParams Surfaces(ECC_WorldStatic);
 Surfaces.AddObjectTypesToQuery(ECC_WorldDynamic);
 // Pawns are outside this trace. A ping marks a surface rather than tracking an actor.
 if(!World->LineTraceSingleByObjectType(Hit, Start, Start + Direction * TeamPingRange, Surfaces, Query)) return;

 for(FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
 {
  auto* Recipient = Cast<AVanthexPlayerController>(It->Get());
  const auto* RecipientState = Recipient ? GetLivingTeamState(Recipient) : nullptr;
  if(RecipientState && RecipientState->GetTeam() == Sender->GetTeam())
   Recipient->ClientPlaceTeamPing(Hit.ImpactPoint, Match->GetRoundIndex());
 }
}

void AVanthexPlayerController::ClientPlaceTeamPing_Implementation(FVector_NetQuantize Location, int32 RoundIndex)
{
 if(!IsLocalController() || !GetWorld() || Location.ContainsNaN()) return;
 TeamPingLocation = Location;
 TeamPingRound = RoundIndex;
 TeamPingUntil = GetWorld()->GetTimeSeconds() + TeamPingDuration;
}

const FVector* AVanthexPlayerController::GetTeamPingLocation() const
{
 if(!CanPredictDamageFeedback() || GetWorld()->GetTimeSeconds() >= TeamPingUntil) return nullptr;
 const auto* Match = GetWorld()->GetGameState<AVanthexGameState>();
 return Match && Match->GetRoundIndex() == TeamPingRound ? &TeamPingLocation : nullptr;
}
