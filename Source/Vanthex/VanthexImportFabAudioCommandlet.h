#pragma once
#include "Commandlets/Commandlet.h"
#include "VanthexImportFabAudioCommandlet.generated.h"
UCLASS()
class UVanthexImportFabAudioCommandlet:public UCommandlet
{
 GENERATED_BODY()
public:
 UVanthexImportFabAudioCommandlet();
 virtual int32 Main(const FString& Params)override;
};
