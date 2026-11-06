#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VanthexWeaponPickup.generated.h"

class AVanthexCharacter;
class USphereComponent;
class USceneComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

USTRUCT()
struct FVanthexDroppedWeapon
{
 GENERATED_BODY()
 UPROPERTY() FName Id;
 UPROPERTY() int32 Magazine = 0;
 UPROPERTY() int32 Reserve = 0;
};

/** Server-owned inventory transfer; clients only render the replicated drop. */
UCLASS()
class AVanthexWeaponPickup : public AActor
{
 GENERATED_BODY()
public:
 AVanthexWeaponPickup();
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
 static AVanthexWeaponPickup* SpawnDrop(AVanthexCharacter* Source, const FVanthexDroppedWeapon& Data, bool Throw);
 static AVanthexWeaponPickup* FindFor(const AVanthexCharacter* Character);
 bool CanReach(const AVanthexCharacter* Character) const;
 bool TryPickup(AVanthexCharacter* Character);
 const FVanthexDroppedWeapon& GetWeaponData() const { return WeaponData; }
 static void ClearForRound(UWorld* World);
protected:
 virtual void BeginPlay() override;
 UFUNCTION() void RefreshVisual();
 UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> VisualRoot;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
 UPROPERTY(ReplicatedUsing=RefreshVisual) FVanthexDroppedWeapon WeaponData;
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Parts;
 bool bClaimed = false;
};
