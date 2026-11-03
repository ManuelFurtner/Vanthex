#pragma once
#include "CoreMinimal.h"

/** Distance-driven foot/gear cadence. Deliberately accepts no inventory or armor information. */
struct FVanthexEquipmentStepClock
{
 bool Advance(FVector Position,FVector Velocity,float DeltaSeconds,bool bGrounded,bool bAlive,bool bRoundActive,bool bTeleported,bool bCrouched);
private:
 FVector Previous=FVector::ZeroVector;
 float Distance=0.f, SinceStep=0.f;
 bool bSampled=false;
};
