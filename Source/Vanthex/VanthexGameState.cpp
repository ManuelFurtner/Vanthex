#include "VanthexGameState.h"

#include "Net/UnrealNetwork.h"
#include "VanthexPlayerState.h"
#include "VanthexCommunicationComponent.h"

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

	if (RoundIndex != NewRoundIndex || (NewPhase == EVanthexRoundPhase::BuyPhase && Phase != NewPhase)) Eliminations.Reset();
	Phase = NewPhase;
	AlphaScore = NewAlphaScore;
	BravoScore = NewBravoScore;
	RoundIndex = NewRoundIndex;
	TimeRemaining = NewTimeRemaining;
	RoundWinner = NewRoundWinner;
	MatchWinner = NewMatchWinner;
}

void AVanthexGameState::PublishCore(EVanthexCoreReadout NewReadout, float NewExtractionRemaining, float NewExtractionDuration)
{
	if (!HasAuthority())
	{
		return;
	}

	CoreReadout = NewReadout;
	ExtractionRemaining = NewExtractionRemaining;
 ExtractionDuration = FMath::Max(.01f,NewExtractionDuration);
}

void AVanthexGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AVanthexGameState, bCTFOvertime);
	DOREPLIFETIME(AVanthexGameState, Eliminations);
	DOREPLIFETIME(AVanthexGameState, Phase);
	DOREPLIFETIME(AVanthexGameState, AlphaScore);
	DOREPLIFETIME(AVanthexGameState, BravoScore);
	DOREPLIFETIME(AVanthexGameState, RoundIndex);
	DOREPLIFETIME(AVanthexGameState, TimeRemaining);
	DOREPLIFETIME(AVanthexGameState, RoundWinner);
	DOREPLIFETIME(AVanthexGameState, MatchWinner);
	DOREPLIFETIME(AVanthexGameState, CoreReadout);
	DOREPLIFETIME(AVanthexGameState, ExtractionRemaining);
 DOREPLIFETIME(AVanthexGameState, ExtractionDuration);
}


#include "VanthexTestArena.h"
#include "Engine/World.h"
void AVanthexGameState::BeginPlay() { Super::BeginPlay(); if (!HasAuthority()) { FActorSpawnParameters Params; Params.Name=TEXT("VanthexArena"); if (auto* Arena = GetWorld()->SpawnActor<AVanthexTestArena>(AVanthexTestArena::StaticClass(),FTransform::Identity,Params)) Arena->Build(); } }

void AVanthexGameState::PublishElimination(const AVanthexPlayerState* Killer, const AVanthexPlayerState* Victim)
{
 if (!HasAuthority() || !Victim) return;
 FVanthexElimination Entry;
 Entry.VictimId=Victim->GetPlayerId();
 Entry.KillerId=Killer?Killer->GetPlayerId():INDEX_NONE;
 Entry.Victim=UVanthexCommunicationComponent::CleanText(Victim->GetPlayerName(),24);
 Entry.Killer=Killer?UVanthexCommunicationComponent::CleanText(Killer->GetPlayerName(),24):TEXT("Umgebung");
 if(Entry.Victim.IsEmpty()) Entry.Victim=TEXT("Spieler");
 if(Entry.Killer.IsEmpty()) Entry.Killer=TEXT("Spieler");
 Entry.ServerTime=GetServerWorldTimeSeconds();
 Eliminations.Add(Entry);
 if(Eliminations.Num()>12) Eliminations.RemoveAt(0,Eliminations.Num()-12);
 ForceNetUpdate();
}
