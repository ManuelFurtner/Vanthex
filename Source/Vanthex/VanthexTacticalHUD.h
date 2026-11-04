#pragma once
#include "CoreMinimal.h"
#include "VanthexTypes.h"

namespace VanthexTacticalHUD
{
// Deliberately no sensor, visibility, or enemy-spotting exception.
inline bool ShowTeammate(EVanthexTeam Viewer,EVanthexTeam Candidate,bool Alive,bool Self)
{
 return !Self&&Alive&&Viewer!=EVanthexTeam::None&&Viewer==Candidate;
}
inline int32 Floor(float Z) { return FMath::Clamp(FMath::RoundToInt((Z-96.f)/600.f),-1,1); }
}
