#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VanthexTypes.h"
#include "VanthexExtractionZone.generated.h"

class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class AVanthexExtractionZone : public AActor
{
	GENERATED_BODY()

public:
	AVanthexExtractionZone();

	void Setup(EVanthexExtractionId InId, const FVector& InHalfExtent);
	void SetCharge(float Alpha);
	void HoldCharge(float Seconds);
	bool Contains(const FVector& WorldPoint) const;
	EVanthexExtractionId GetId() const { return Id; }

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Extraction")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Extraction")
	TObjectPtr<UStaticMeshComponent> Pad;

	UPROPERTY(VisibleAnywhere, Category = "Extraction")
	EVanthexExtractionId Id = EVanthexExtractionId::None;

	UPROPERTY(VisibleAnywhere, Category = "Extraction")
	FVector HalfExtent = FVector(250.f, 250.f, 200.f);

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ChargeRing;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ChargeMaterials;

	float Charge = 0.f;
	float HoldChargeUntil = 0.f;
};
