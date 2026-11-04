#pragma once
#include "Commandlets/Commandlet.h"
#include "VanthexBuildWeaponModelsCommandlet.generated.h"

UCLASS()
class UVanthexBuildWeaponModelsCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVanthexBuildWeaponModelsCommandlet();
 virtual int32 Main(const FString& Params) override;
};
