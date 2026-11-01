#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "VanthexTestTarget.generated.h"

class UHealthComponent;
class UStaticMeshComponent;

UCLASS()
class AVanthexTestTarget : public AActor
{
	GENERATED_BODY()

public:
	AVanthexTestTarget();

	UHealthComponent* GetHealthComponent() const { return Health; }
	FVector GetBodyCenter() const;
	FVector GetHeadCenter() const;
	void ResetForRound();
	bool IsCombatCollisionEnabled() const;

protected:
	virtual void BeginPlay() override;

	void HandleDeath(AActor* DeadActor);
	void PaintMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color);

	UPROPERTY(VisibleAnywhere, Category = "Target")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Target")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Target")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, Category = "Target")
	TObjectPtr<UHealthComponent> Health;
};
