#pragma once
#include "Commandlets/Commandlet.h"
#include "VanthexImportEnergyCommandlet.generated.h"
UCLASS()
class UVanthexImportEnergyCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVanthexImportEnergyCommandlet();
 virtual int32 Main(const FString& Params) override;
};
