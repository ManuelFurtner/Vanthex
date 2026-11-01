#include "VanthexGameState.h"

#include "Net/UnrealNetwork.h"

void AVanthexGameState::PublishRound(
	EVanthexRoundPhase NewPhase,
	int32 NewAlphaScore,
	int32 NewBravoScore,
	int32 NewRoundIndex,
	float NewTimeRemaining,
	EVanthexTeam NewRoundWinner,
	EVanthexTeam NewMatchWinner)
{
	if (!HasAuthority())
	{
		return;
	}

	Phase = NewPhase;
	AlphaScore = NewAlphaScore;
	BravoScore = NewBravoScore;
	RoundIndex = NewRoundIndex;
	TimeRemaining = NewTimeRemaining;
	RoundWinner = NewRoundWinner;
	MatchWinner = NewMatchWinner;
}

void AVanthexGameState::PublishCore(EVanthexCoreReadout NewReadout, float NewExtractionRemaining)
{
	if (!HasAuthority())
	{
		return;
	}

	CoreReadout = NewReadout;
	ExtractionRemaining = NewExtractionRemaining;
}

void AVanthexGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AVanthexGameState, Phase);
	DOREPLIFETIME(AVanthexGameState, AlphaScore);
	DOREPLIFETIME(AVanthexGameState, BravoScore);
	DOREPLIFETIME(AVanthexGameState, RoundIndex);
	DOREPLIFETIME(AVanthexGameState, TimeRemaining);
	DOREPLIFETIME(AVanthexGameState, RoundWinner);
	DOREPLIFETIME(AVanthexGameState, MatchWinner);
	DOREPLIFETIME(AVanthexGameState, CoreReadout);
	DOREPLIFETIME(AVanthexGameState, ExtractionRemaining);
}


#include "VanthexTestArena.h"
#include "Engine/World.h"
void AVanthexGameState::BeginPlay() { Super::BeginPlay(); if (!HasAuthority()) { FActorSpawnParameters Params; Params.Name=TEXT("VanthexArena"); if (auto* Arena = GetWorld()->SpawnActor<AVanthexTestArena>(AVanthexTestArena::StaticClass(),FTransform::Identity,Params)) Arena->Build(); } }
