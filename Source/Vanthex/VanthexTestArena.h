#pragma once

#include "CoreMinimal.h"
#include "VanthexArt.h"
#include "GameFramework/Actor.h"
#include "VanthexTypes.h"
#include "VanthexTestArena.generated.h"

class AVanthexTestTarget;
class AVanthexCore;
class AVanthexExtractionZone;
class APlayerStart;
class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class ASkyLight;

UCLASS()
class AVanthexTestArena : public AActor
{
	GENERATED_BODY()

public:
	AVanthexTestArena();

	void Build();
	void ResetRoundState();
	AVanthexTestTarget* GetTarget() const { return Target; }
	APlayerStart* GetPlayerStart() const;
	APlayerStart* GetTeamSpawn(EVanthexTeam Team, int32 Slot) const;
	AVanthexCore* GetCore() const { return Core; }
	AVanthexExtractionZone* GetExtractionZone(EVanthexExtractionId Id) const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UStaticMeshComponent* SpawnBlock(const FVector& Location, const FVector& Scale, const FLinearColor& Color, float Roughness, float Metallic, EVanthexSurface Style);
	void DressFacility();
	void DressPass02();
	void BuildAtmosphere();
 void BuildGridMap();
 void BuildFoundryMap();
 void BuildFoundryGameplay();
 void BuildCTF();
 void BuildCTFGameplay();
 void BuildGridV4();
 void BuildGridV4Gameplay();
 void ValidateGridV4();
 void BuildGridV3();
 void BuildGridV3Gameplay();
 void ValidateGridV3();
 void ValidateGridMap();
 void ValidateEnergySystems();

	UPROPERTY(VisibleAnywhere, Category = "Arena")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<AVanthexTestTarget> Target;

	UPROPERTY()
	TArray<TObjectPtr<APlayerStart>> AlphaSpawns;

	UPROPERTY()
	TArray<TObjectPtr<APlayerStart>> BravoSpawns;

	UPROPERTY()
	TObjectPtr<AVanthexCore> Core;

	UPROPERTY()
	TObjectPtr<AVanthexExtractionZone> ExtractionA;

	UPROPERTY()
	TObjectPtr<AVanthexExtractionZone> ExtractionB;

	UPROPERTY()
	TObjectPtr<ASkyLight> SkyLight;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> SpinParts;

	float SpinAngle = 0.f;
 int32 BlockIndex = 0;
};
