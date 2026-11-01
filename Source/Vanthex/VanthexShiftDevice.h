#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VanthexShiftDevice.generated.h"
class AVanthexCharacter;
class UStaticMeshComponent;
class UPointLightComponent;
UCLASS()
class AVanthexShiftDevice : public AActor
{
 GENERATED_BODY()
public:
 AVanthexShiftDevice();
 void Configure(uint8 InKind);
 bool Activate(AVanthexCharacter* User, float Range);
 void ResetDevice();
 FVector InteractionPoint(const FVector& UserLocation) const;
 void SetCircuitPower(bool bOn);
 UPROPERTY(Replicated) int32 Circuit = -1;
 UPROPERTY(Replicated) bool bPowered = true;
 UPROPERTY(Replicated) float HackProgress = 0.f;
 UPROPERTY(Replicated) FVector Home = FVector::ZeroVector;
 virtual void Tick(float DeltaSeconds) override;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 UPROPERTY(ReplicatedUsing=OnRep_Kind) uint8 Kind = 0;
 UFUNCTION() void OnRep_Kind();
 UPROPERTY(Replicated) bool bActive = false;
 UPROPERTY(ReplicatedUsing=OnRep_Kind) float DoorWidth = 300.f;
 UPROPERTY(Replicated) float LiftTravel = 220.f;
protected:
 virtual void BeginPlay() override;
 virtual void PostInitializeComponents() override;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
 TWeakObjectPtr<AVanthexCharacter> Hacker;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> ScreenFront;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> ScreenBack;
 float HackRange = 250.f;
 float HackHealth = 0.f;
 float LastUse = -10.f;
};

