#pragma once
#include "Commandlets/Commandlet.h"
#include "VanthexBuildGadgetPresentationCommandlet.generated.h"

UCLASS()
class UVanthexBuildGadgetPresentationCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVanthexBuildGadgetPresentationCommandlet();
 virtual int32 Main(const FString& Params) override;
};
