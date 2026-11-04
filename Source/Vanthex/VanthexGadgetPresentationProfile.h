#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VanthexGadgetPresentationProfile.generated.h"

class UMaterialInterface;
class UStaticMesh;
class USoundBase;
class USoundAttenuation;
class USoundConcurrency;

UCLASS()
class UVanthexGadgetPresentationProfile : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere) TMap<FName,TObjectPtr<USoundBase>> Sounds;
 UPROPERTY(EditAnywhere) TObjectPtr<USoundAttenuation> NearAttenuation;
 UPROPERTY(EditAnywhere) TObjectPtr<USoundAttenuation> FarAttenuation;
 UPROPERTY(EditAnywhere) TObjectPtr<USoundConcurrency> Concurrency;
 UPROPERTY(EditAnywhere) TObjectPtr<UMaterialInterface> SmokeMaterial;
 UPROPERTY(EditAnywhere) TObjectPtr<UMaterialInterface> EnergyMaterial;
 UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> Sphere;
 UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> Cylinder;
 UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> Cube;
};
