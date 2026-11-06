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
inline FVector2D Heading(FVector Velocity,float Yaw)
{
 Velocity.Z=0;
 if(Velocity.SizeSquared()<25.f)Velocity=FRotator(0,Yaw,0).Vector();
 Velocity.Normalize();return FVector2D(Velocity.X,-Velocity.Y);
}
inline int32 Floor(float Z) { return FMath::Clamp(FMath::RoundToInt((Z-96.f)/600.f),-1,1); }
}
