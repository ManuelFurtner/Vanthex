#pragma once
#include "CoreMinimal.h"
class UWorld;
class AActor;

struct FVanthexWeaponAcousticRoute
{
 bool bDirect=true, bPortal=false;
 float Distance=0.f, Transmission=1.f, LowPassHz=20000.f, ReflectionDelay=0.f;
 FVector ApparentPosition=FVector::ZeroVector;
 int32 SourceRoom=-1, ListenerRoom=-1, Hops=0;
};
namespace VanthexWeaponAcoustics
{
 FVanthexWeaponAcousticRoute Route(UWorld* World, FVector Source, FVector Listener, AActor* SourceActor=nullptr);
 void Invalidate(UWorld* World);
}
