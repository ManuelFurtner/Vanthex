#pragma once

#include "VanthexTypes.generated.h"

UENUM(BlueprintType)
enum class EVanthexTeam : uint8
{
	None,
	Alpha,
	Bravo
};

UENUM(BlueprintType)
enum class EVanthexRoundPhase : uint8
{
	Waiting,
	RoundStarting,
	RoundActive,
	RoundEnding,
	Intermission,
	MatchEnded,
	BuyPhase
};

UENUM(BlueprintType)
enum class EVanthexCoreState : uint8
{
	AtRest,
	Carried,
	Thrown,
	Extracting,
	Extracted
};

UENUM(BlueprintType)
enum class EVanthexExtractionId : uint8
{
	None,
	A,
	B
};

UENUM(BlueprintType)
enum class EVanthexCoreReadout : uint8
{
	Center,
	Alpha,
	Bravo,
	Dropped,
	Thrown,
	ExtractingA,
	ExtractingB,
	Extracted
};
