#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "VanthexGridV3Layout.h"

// Unreal units are centimetres. GRID occupies 200 x 240 metres.
namespace VanthexMap
{
inline bool IsGridV3(const UWorld* World = nullptr)
{
 return (World && World->GetMapName().EndsWith(TEXT("GridV3")))
  || FParse::Param(FCommandLine::Get(),TEXT("BuildGridV3"))
  || FParse::Param(FCommandLine::Get(),TEXT("BakeGridV3"))
  || FParse::Param(FCommandLine::Get(),TEXT("GridV3Test"))
  || FParse::Param(FCommandLine::Get(),TEXT("GridV3GameTest"))
  || FParse::Param(FCommandLine::Get(),TEXT("GridV3Shots"));
}
inline bool IsLegacyTest()
{
 return FParse::Param(FCommandLine::Get(), TEXT("VanthexSelfTest")) || FParse::Param(FCommandLine::Get(), TEXT("VanthexVisualShots"));
}
inline FVector2D Extent(const UWorld* World = nullptr)
{
 if(IsLegacyTest()) return FVector2D(2000,2000);
 if(IsGridV3(World)) { const auto B=GridV3::Bounds(); return FVector2D(FMath::Max(FMath::Abs(B.Min.X),FMath::Abs(B.Max.X)),FMath::Max(FMath::Abs(B.Min.Y),FMath::Abs(B.Max.Y))); }
 return FVector2D(10000,12000);
}
inline FVector ExtractionA() { return FVector(-3500,2300,20); }
inline FVector ExtractionB() { return FVector(3500,-2300,20); }
}
