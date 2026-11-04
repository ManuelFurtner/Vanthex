#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "VanthexGridV3Layout.h"
#include "VanthexGridV4Layout.h"

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
inline bool IsGridV4(const UWorld* World=nullptr){
 return (World&&World->GetMapName().EndsWith(TEXT("GridV4")))||FParse::Param(FCommandLine::Get(),TEXT("BakeGridV4"))||FParse::Param(FCommandLine::Get(),TEXT("GridV4Test"))||FParse::Param(FCommandLine::Get(),TEXT("GridV4GameTest"))||FParse::Param(FCommandLine::Get(),TEXT("GridV4Shots"));
}
inline bool IsFacility(const UWorld* World=nullptr){return IsGridV3(World)||IsGridV4(World);}
inline const TArray<GridV3::FRoom>& Rooms(const UWorld* World){return IsGridV4(World)?GridV4::Rooms():GridV3::Rooms();}
inline const TArray<GridV3::FConnection>& Connections(const UWorld* World){return IsGridV4(World)?GridV4::Connections():GridV3::Connections();}
inline const TArray<GridV3::FVerticalLink>& VerticalLinks(const UWorld* World){return IsGridV4(World)?GridV4::VerticalLinks():GridV3::VerticalLinks();}
inline const TCHAR* CurrentMap(const UWorld* World){return IsGridV4(World)?TEXT("/Game/Vanthex/Maps/GridV4"):IsGridV3(World)?TEXT("/Game/Vanthex/Maps/GridV3"):TEXT("/Game/Vanthex/Maps/Foundry");}
inline bool IsLegacyTest()
{
 return FParse::Param(FCommandLine::Get(), TEXT("VanthexSelfTest")) || FParse::Param(FCommandLine::Get(), TEXT("VanthexVisualShots"));
}
inline FVector2D Extent(const UWorld* World = nullptr)
{
 if(IsLegacyTest()) return FVector2D(2000,2000);
 if(IsFacility(World)) { const auto B=IsGridV4(World)?GridV4::Bounds():GridV3::Bounds(); return FVector2D(FMath::Max(FMath::Abs(B.Min.X),FMath::Abs(B.Max.X)),FMath::Max(FMath::Abs(B.Min.Y),FMath::Abs(B.Max.Y))); }
 return FVector2D(10000,12000);
}
inline FVector ExtractionA() { return FVector(-3500,2300,20); }
inline FVector ExtractionB() { return FVector(3500,-2300,20); }
}
