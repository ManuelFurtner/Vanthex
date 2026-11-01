#include "VanthexExtractionZone.h"

#include "VanthexArt.h"
#include "VanthexMapLayout.h"
#include "Materials/MaterialInterface.h"
#include "Components/SceneComponent.h"
#include "Math/RotationMatrix.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AVanthexExtractionZone::AVanthexExtractionZone()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	Pad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad"));
	Pad->SetupAttachment(SceneRoot);
	Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Pad->SetStaticMesh(CubeMesh.Object);
	}
}

void AVanthexExtractionZone::Setup(EVanthexExtractionId InId, const FVector& InHalfExtent)
{
	Id = InId;
	HalfExtent = InHalfExtent;
	if (Pad)
	{
		Pad->SetWorldScale3D(FVector(HalfExtent.X / 50.f, HalfExtent.Y / 50.f, 0.08f));
		const bool bA = Id != EVanthexExtractionId::B;
		const FLinearColor Energy = bA ? FLinearColor(0.25f, 0.9f, 1.f) : FLinearColor(1.f, 0.55f, 0.15f);
		const FLinearColor Plate = bA ? FLinearColor(0.08f, 0.12f, 0.14f) : FLinearColor(0.14f, 0.1f, 0.07f);
		if (UMaterialInstanceDynamic* PlateMaterial = VanthexArt::MakeSurface(this, Plate, 0.42f, 0.65f, Energy, 0.08f, EVanthexSurface::DarkMetal))
		{
			Pad->SetMaterial(0, PlateMaterial);
		}
		Pad->SetVisibility(false);

		UStaticMesh* Cylinder = VanthexArt::LoadCylinder();
		UStaticMesh* BaseMesh = VanthexArt::LoadKit(TEXT("SM_TransferBase"));
		UStaticMesh* PylonMesh = VanthexArt::LoadKit(TEXT("SM_Pylon"));
		UStaticMesh* GateMesh = VanthexArt::LoadKit(TEXT("SM_Gate"));
		UMaterialInterface* Metal = VanthexArt::MakeSurface(this, FLinearColor(0.1f, 0.11f, 0.12f), 0.4f, 0.72f, FLinearColor::Black, 0.f, EVanthexSurface::PaintedSteel);
  // The legacy objective meshes have procedural UVs. World-space coated steel
  // keeps their details readable without stretching an unrelated kit UV atlas.
  if(VanthexMap::IsGridV3(GetWorld())) Metal=VanthexArt::MakeSurface(this,FLinearColor(.35f,.39f,.43f),.75f,.15f,FLinearColor::Black,0.f,EVanthexSurface::PaintedSteel);
		UMaterialInstanceDynamic* Glow = VanthexArt::MakeEnergy(this, Energy, 0.55f);
		UMaterialInstanceDynamic* Ring = VanthexArt::MakeEnergy(this, Energy, 0.7f);
		ChargeMaterials.Reset();
		if (Glow)
		{
			ChargeMaterials.Add(Glow);
		}
		if (Ring)
		{
			ChargeMaterials.Add(Ring);
		}
		const FVector Origin = GetActorLocation();
		const float Reach = HalfExtent.X * 0.62f;
		if (BaseMesh)
		{
			if (UStaticMeshComponent* Base = VanthexArt::AddMesh(this, SceneRoot, BaseMesh, Origin, FRotator::ZeroRotator, FVector(1.15f, 1.15f, 1.f), Metal, true))
			{
				Base->SetMaterial(1, Glow);
			}
		}
		else
		{
			Pad->SetVisibility(true);
		}

		const FVector Corners[4] = {
			FVector(-Reach, -Reach, 0.f),
			FVector(-Reach, Reach, 0.f),
			FVector(Reach, -Reach, 0.f),
			FVector(Reach, Reach, 0.f)
		};
		for (const FVector& Corner : Corners)
		{
			if (PylonMesh)
			{
				if (UStaticMeshComponent* Pylon = VanthexArt::AddMesh(this, SceneRoot, PylonMesh, Origin + Corner, FRotator::ZeroRotator, FVector(1.f), Metal, true))
				{
					Pylon->SetMaterial(1, Glow);
				}
			}
			if (Cylinder)
			{
				const FVector Head = Origin + Corner + FVector(0.f, 0.f, 150.f);
				const FVector Hub = Origin + FVector(0.f, 0.f, 80.f);
				const FVector Delta = Hub - Head;
				const float Length = Delta.Size();
				const FRotator Aim = FRotationMatrix::MakeFromZ(Delta.GetSafeNormal()).Rotator();
				VanthexArt::AddMesh(this, SceneRoot, Cylinder, (Head + Hub) * 0.5f, Aim, FVector(0.035f, 0.035f, Length / 100.f), Metal, false);
			}
		}
		ChargeRing = VanthexArt::AddMesh(this, SceneRoot, Cylinder, Origin + FVector(0.f, 0.f, 18.f), FRotator::ZeroRotator, FVector(1.1f, 1.1f, 0.02f), Ring, false);
		if (GateMesh)
		{
			const FVector ToCenter = (-Origin).GetSafeNormal2D();
			const FRotator FaceIn(0.f, ToCenter.Rotation().Yaw, 0.f);
			if (UStaticMeshComponent* Gate = VanthexArt::AddMesh(this, SceneRoot, GateMesh, Origin + FVector(0.f, 0.f, 0.f), FaceIn, FVector(1.35f), Metal, true))
			{
				Gate->SetMaterial(1, Glow);
				ChargeMaterials.Add(Glow);
			}
		}

		const FString Label = bA ? TEXT("TRANSFER A") : TEXT("TRANSFER B");
		const FVector SignOffset = bA ? FVector(-HalfExtent.X - 40.f, 0.f, 180.f) : FVector(HalfExtent.X + 40.f, 0.f, 180.f);
		const FRotator SignRotation(0.f, bA ? 0.f : 180.f, 0.f);
		VanthexArt::SpawnSign(this, SceneRoot, Label, Origin + SignOffset, SignRotation, 220.f, 28.f, Energy);
	}
}

bool AVanthexExtractionZone::Contains(const FVector& WorldPoint) const
{
	const FVector Local = GetActorQuat().UnrotateVector(WorldPoint - GetActorLocation());
	return FMath::Abs(Local.X) <= HalfExtent.X
		&& FMath::Abs(Local.Y) <= HalfExtent.Y
		&& FMath::Abs(Local.Z) <= HalfExtent.Z;
}

void AVanthexExtractionZone::SetCharge(float Alpha)
{
	if (Alpha <= 0.01f && GetWorld() && GetWorld()->GetTimeSeconds() < HoldChargeUntil)
	{
		return;
	}

	Charge = FMath::Clamp(Alpha, 0.f, 1.f);
	const float Strength = FMath::Lerp(0.45f, 5.2f, Charge);
	for (UMaterialInstanceDynamic* Material : ChargeMaterials)
	{
		if (Material)
		{
			Material->SetScalarParameterValue(TEXT("Strength"), Strength);
		}
	}
}

void AVanthexExtractionZone::HoldCharge(float Seconds)
{
	if (UWorld* World = GetWorld())
	{
		HoldChargeUntil = World->GetTimeSeconds() + Seconds;
	}
}

void AVanthexExtractionZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ChargeRing)
	{
		ChargeRing->AddLocalRotation(FRotator(0.f, DeltaSeconds * FMath::Lerp(12.f, 160.f, Charge), 0.f));
	}
}
