#pragma once
#include "CoreMinimal.h"

struct FGadgetProfile
{
 FName Id;
 float Fuse, Duration, Radius, MaxDamage;
};
namespace VanthexGadgetRules
{
 const FGadgetProfile* Find(FName Id);
 float FlashStrength(float Distance,float FacingDot,float Radius);
 float BlastDamage(float Distance,float Radius,float Maximum);
 bool SegmentIntersectsSmoke(const FVector& A,const FVector& B,const FVector& Center,float Radius);
}
