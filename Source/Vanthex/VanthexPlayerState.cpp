#include "VanthexPlayerState.h"

#include "Net/UnrealNetwork.h"

void AVanthexPlayerState::SetTeam(EVanthexTeam NewTeam, int32 NewSlot)
{
	if (!HasAuthority())
	{
		return;
	}

	Team = NewTeam;
	SpawnSlot = NewSlot;
}

void AVanthexPlayerState::SetAlive(bool bNewAlive)
{
	if (!HasAuthority())
	{
		return;
	}

	bAlive = bNewAlive;
}

void AVanthexPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AVanthexPlayerState, Team);
	DOREPLIFETIME(AVanthexPlayerState, SpawnSlot);
	DOREPLIFETIME(AVanthexPlayerState, bAlive);
}
