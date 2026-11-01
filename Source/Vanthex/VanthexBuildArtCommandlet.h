#pragma once

#include "Commandlets/Commandlet.h"
#include "VanthexBuildArtCommandlet.generated.h"

UCLASS()
class UVanthexBuildArtCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVanthexBuildArtCommandlet();

	virtual int32 Main(const FString& Params) override;
};
