#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "VanthexBulletAudioProfile.generated.h"

class USoundBase;
class USoundAttenuation;
class USoundConcurrency;

USTRUCT(BlueprintType)
struct FVanthexBulletSoundSet
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<USoundBase>> Variants;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="1")) float Volume = .4f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.9", ClampMax="1.1")) float PitchMin = .97f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.9", ClampMax="1.1")) float PitchMax = 1.03f;
};

/** Cosmetic tuning only; distances in Unreal centimetres. Hard sound references are cooked with this asset. */
UCLASS(BlueprintType)
class UVanthexBulletAudioProfile : public UPrimaryDataAsset
{
 GENERATED_BODY()
public:
 UVanthexBulletAudioProfile();
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Distances", meta=(ClampMin="1")) float CloseDistance = 50.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Distances", meta=(ClampMin="1")) float MediumDistance = 150.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Distances", meta=(ClampMin="1")) float MaxFlybyDistance = 300.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Distances", meta=(ClampMin="1")) float MaxImpactDistance = 600.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Listener") FVector ListenerOffset = FVector::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limits", meta=(ClampMin="0.02", ClampMax="1")) float MinEventInterval = .045f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limits", meta=(ClampMin="1", ClampMax="60")) int32 MaxPacketsPerSecond = 16;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds") TMap<FGameplayTag, FVanthexBulletSoundSet> Events;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds") TMap<FName, FVanthexBulletSoundSet> Impacts;
 /** Ordered first-match rules: specific grate before generic Metal. Component and actor tags are supported. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surfaces") TArray<FName> SurfaceTagPriority;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surfaces") TMap<FName, FName> SurfaceTags;
 /** Optional PhysicalSurface number -> impact key, preferred over tags. Zero remains the default fallback. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surfaces") TMap<int32, FName> PhysicalSurfaces;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surfaces") FName DefaultSurface = TEXT("Concrete");
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spatial") TObjectPtr<USoundAttenuation> Attenuation;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spatial") TObjectPtr<USoundConcurrency> Concurrency;
};
