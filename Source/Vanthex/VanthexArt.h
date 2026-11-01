#pragma once

#include "CoreMinimal.h"

class AActor;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UObject;
class UInstancedStaticMeshComponent;
class UDecalComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;

enum class EVanthexSurface : uint8
{
	Concrete,
	PaintedSteel,
	BareSteel,
	DarkMetal,
	Floor,
	Rubber
};

namespace VanthexArt
{
	UMaterialInterface* GetSurfaceMaterial();
	UMaterialInterface* GetEnergyMaterial();
	UMaterialInterface* GetSignMaterial();
	UMaterialInterface* GetDecalMaterial();
	UMaterialInterface* GetGlassMaterial();

	UMaterialInstanceDynamic* MakeSurface(
		UObject* Outer,
		const FLinearColor& Color,
		float Roughness,
		float Metallic,
		const FLinearColor& EmissiveColor = FLinearColor::Black,
		float EmissiveStrength = 0.f,
		EVanthexSurface Style = EVanthexSurface::Concrete);

	UMaterialInstanceDynamic* MakeEnergy(UObject* Outer, const FLinearColor& Color, float Strength);

	UStaticMesh* LoadCube();
	UStaticMesh* LoadCylinder();
	UStaticMesh* LoadSphere();
	UStaticMesh* LoadPlane();
	UStaticMesh* LoadKit(const TCHAR* Name);

	UStaticMeshComponent* AddMesh(
		AActor* Owner,
		USceneComponent* Parent,
		UStaticMesh* Mesh,
		const FVector& Location,
		const FRotator& Rotation,
		const FVector& Scale,
		UMaterialInterface* Material,
		bool bCastShadow);

	UInstancedStaticMeshComponent* MakeInstances(
		AActor* Owner,
		USceneComponent* Parent,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		UMaterialInterface* Secondary,
		bool bCastShadow);

	UDecalComponent* AddDecal(
		AActor* Owner,
		USceneComponent* Parent,
		const FVector& Location,
		const FRotator& Rotation,
		const FVector& Size,
		const FLinearColor& Color,
		float Opacity,
		float Stripe);

	void SpawnSign(
		AActor* Owner,
		USceneComponent* Parent,
		const FString& Text,
		const FVector& Location,
		const FRotator& Rotation,
		float WidthCm,
		float HeightCm,
		const FLinearColor& Color);
}
