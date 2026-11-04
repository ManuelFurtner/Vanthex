#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VanthexImportedWeaponSet.generated.h"

class UStaticMesh;

/** Locally derived, licensed presentation data. Gameplay profiles remain independent. */
USTRUCT()
struct FVanthexImportedWeaponModel
{
 GENERATED_BODY()
 UPROPERTY() FName WeaponId;
 UPROPERTY() TObjectPtr<UStaticMesh> Body;
 UPROPERTY() TObjectPtr<UStaticMesh> Magazine;
 UPROPERTY() TObjectPtr<UStaticMesh> Bolt;
 UPROPERTY() FVector Muzzle=FVector::ZeroVector;
 UPROPERTY() FVector SightPoint=FVector::ZeroVector;
 UPROPERTY() float SightPitch=0.f;
 UPROPERTY() bool bReflexSight=false;
};

UCLASS()
class UVanthexImportedWeaponSet : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY() TArray<FVanthexImportedWeaponModel> Models;
 /** Returns null when the optional licensed package is absent or incomplete. */
 static const FVanthexImportedWeaponModel* Find(FName WeaponId);
};
