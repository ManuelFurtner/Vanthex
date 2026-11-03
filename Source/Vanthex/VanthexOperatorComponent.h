#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VanthexTypes.h"
#include "VanthexOperatorComponent.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class AVanthexCharacter;

/** Shared ARC presentation for human players and server-controlled bots. */
UCLASS(ClassGroup=(Vanthex), meta=(BlueprintSpawnableComponent))
class UVanthexOperatorComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVanthexOperatorComponent();
 virtual void BeginPlay() override;
 virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
 void SetTeamAppearance(EVanthexTeam Team);
 void PresentShot();
 FVector GetMuzzleLocation() const;
 FName GetOperatorId() const { return TEXT("ARC"); }
 bool IsPresentationReady() const;
 UStaticMeshComponent* GetWorldRifle() const { return WorldRifle; }
 UMaterialInstanceDynamic* GetArmorMaterial() const { return ArmorMaterial; }

private:
 UPROPERTY(Transient) TObjectPtr<AVanthexCharacter> Character;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ArmorMaterial;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> AccentMaterial;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WorldRifle;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WorldMuzzle;
 float MuzzleSeconds=0.f;
 EVanthexTeam CurrentTeam=EVanthexTeam::None;
};
