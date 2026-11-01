#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "VanthexTypes.h"
#include "VanthexGameState.generated.h"

UCLASS()
class AVanthexGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
 void PublishRound(
		EVanthexRoundPhase NewPhase,
		int32 NewAlphaScore,
		int32 NewBravoScore,
		int32 NewRoundIndex,
		float NewTimeRemaining,
		EVanthexTeam NewRoundWinner,
		EVanthexTeam NewMatchWinner);

	void PublishCore(EVanthexCoreReadout NewReadout, float NewExtractionRemaining);

	EVanthexRoundPhase GetPhase() const { return Phase; }
	int32 GetAlphaScore() const { return AlphaScore; }
	int32 GetBravoScore() const { return BravoScore; }
	int32 GetRoundIndex() const { return RoundIndex; }
	float GetTimeRemaining() const { return TimeRemaining; }
	EVanthexTeam GetRoundWinner() const { return RoundWinner; }
	EVanthexTeam GetMatchWinner() const { return MatchWinner; }
	EVanthexCoreReadout GetCoreReadout() const { return CoreReadout; }
	float GetExtractionRemaining() const { return ExtractionRemaining; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	EVanthexRoundPhase Phase = EVanthexRoundPhase::Waiting;

	UPROPERTY(Replicated)
	int32 AlphaScore = 0;

	UPROPERTY(Replicated)
	int32 BravoScore = 0;

	UPROPERTY(Replicated)
	int32 RoundIndex = 0;

	UPROPERTY(Replicated)
	float TimeRemaining = 0.f;

	UPROPERTY(Replicated)
	EVanthexTeam RoundWinner = EVanthexTeam::None;

	UPROPERTY(Replicated)
	EVanthexTeam MatchWinner = EVanthexTeam::None;

	UPROPERTY(Replicated)
	EVanthexCoreReadout CoreReadout = EVanthexCoreReadout::Center;

	UPROPERTY(Replicated)
	float ExtractionRemaining = 0.f;
};
