#pragma once

#include "Commandlets/Commandlet.h"
#include "VanthexBuildBulletAudioCommandlet.generated.h"

/** Imports the original AUDIO M1 prototype WAVs and creates editable audio assets. */
UCLASS()
class UVanthexBuildBulletAudioCommandlet : public UCommandlet
{

	GENERATED_BODY()

public:
	UVanthexBuildBulletAudioCommandlet();
	virtual int32 Main(const FString& Params) override;
};
