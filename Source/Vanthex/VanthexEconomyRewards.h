#pragma once
#include "CoreMinimal.h"

namespace VanthexEconomyRewards
{
inline bool QualifiesPass(float Travel, float Before, float After)
{
 return FMath::IsFinite(Travel) && FMath::IsFinite(Before) && FMath::IsFinite(After)
  && Before >= 0.f && After >= 0.f && Before < TNumericLimits<float>::Max()
  && After < TNumericLimits<float>::Max() && Travel >= 400.f && Before - After >= 400.f;
}
}
