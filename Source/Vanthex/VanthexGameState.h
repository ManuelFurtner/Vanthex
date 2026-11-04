#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "VanthexTypes.h"
#include "VanthexGameState.generated.h"

class AVanthexPlayerState;

USTRUCT()
struct FVanthexElimination
{
 GENERATED_BODY()
 UPROPERTY() FString Killer;
 UPROPERTY() FString Victim;
 UPROPERTY() int32 KillerId = INDEX_NONE;
 UPROPERTY() int32 VictimId = INDEX_NONE;
 UPROPERTY() float ServerTime = 0.f;
};

UCLASS()
class AVanthexGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
 void SetCTFOvertime(bool Value){if(HasAuthority())bCTFOvertime=Value;}
 bool IsCTFOvertime()const{return bCTFOvertime;}
 void PublishElimination(const AVanthexPlayerState* Killer, const AVanthexPlayerState* Victim);
 const TArray<FVanthexElimination>& GetEliminations() const { return Eliminations; }
 void PublishRound(
		EVanthexRoundPhase NewPhase,
		int32 NewAlphaScore,
		int32 NewBravoScore,
		int32 NewRoundIndex,
		float NewTimeRemaining,
		EVanthexTeam NewRoundWinner,
		EVanthexTeam NewMatchWinner);

	void PublishCore(EVanthexCoreReadout NewReadout, float NewExtractionRemaining, float NewExtractionDuration = 15.f);

	EVanthexRoundPhase GetPhase() const { return Phase; }
	int32 GetAlphaScore() const { return AlphaScore; }
	int32 GetBravoScore() const { return BravoScore; }
	int32 GetRoundIndex() const { return RoundIndex; }
	float GetTimeRemaining() const { return TimeRemaining; }
	EVanthexTeam GetRoundWinner() const { return RoundWinner; }
	EVanthexTeam GetMatchWinner() const { return MatchWinner; }
	EVanthexCoreReadout GetCoreReadout() const { return CoreReadout; }
	float GetExtractionRemaining() const { return ExtractionRemaining; }
 float GetExtractionDuration() const { return ExtractionDuration; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
 UPROPERTY(Replicated) bool bCTFOvertime=false;
 UPROPERTY(Replicated) TArray<FVanthexElimination> Eliminations;
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
 UPROPERTY(Replicated) float ExtractionDuration = 15.f;
};
