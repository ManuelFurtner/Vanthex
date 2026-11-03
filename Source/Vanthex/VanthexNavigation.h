#pragma once
#include "CoreMinimal.h"
class UWorld;
namespace VanthexNavigation
{
 bool FindRoute(UWorld* World,const FVector& Start,const FVector& Goal,TArray<FVector>& Route);
 // Standing-height route length, independent of the Core's hand/flight height.
 bool FindRouteLength(UWorld* World,const FVector& Start,const FVector& Goal,float& Length);
 // Geometry-only validation of a continuous floor segment. Endpoints are floor
 // positions, unlike FindRoute's character/objective positions.
 bool CheckWalkableSegment(UWorld* World,const FVector& FloorStart,const FVector& FloorEnd,FString* Failure=nullptr);
 void GridV3GraphStats(UWorld* World,int32& NodeCount,int32& EdgeCount,int32& ComponentCount);
}
