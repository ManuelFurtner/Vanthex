#include "VanthexTestTarget.h"

#include "VanthexArt.h"
#include "HealthComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AVanthexTestTarget::AVanthexTestTarget()
{
	PrimaryActorTick.bCanEverTick = false;
 bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	BodyMesh->SetupAttachment(SceneRoot);
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	BodyMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.6f));
	BodyMesh->SetCollisionProfileName(TEXT("BlockAll"));
	BodyMesh->ComponentTags.Add(TEXT("Body"));

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	HeadMesh->SetupAttachment(SceneRoot);
	HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 180.f));
	HeadMesh->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.4f));
	HeadMesh->SetCollisionProfileName(TEXT("BlockAll"));
	HeadMesh->ComponentTags.Add(TEXT("Head"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMesh.Object);
		HeadMesh->SetStaticMesh(CubeMesh.Object);
	}

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
}

void AVanthexTestTarget::BeginPlay()
{
	Super::BeginPlay();

	if (Health)
	{
		Health->OnDeath.AddUObject(this, &AVanthexTestTarget::HandleDeath);
	}

	PaintMesh(BodyMesh, FLinearColor(0.75f, 0.12f, 0.08f));
	PaintMesh(HeadMesh, FLinearColor(0.95f, 0.8f, 0.15f));
}

FVector AVanthexTestTarget::GetBodyCenter() const
{
	return BodyMesh ? BodyMesh->GetComponentLocation() : GetActorLocation();
}

FVector AVanthexTestTarget::GetHeadCenter() const
{
	return HeadMesh ? HeadMesh->GetComponentLocation() : GetActorLocation() + FVector(0.f, 0.f, 180.f);
}

void AVanthexTestTarget::PaintMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color)
{
	if (!Mesh)
	{
		return;
	}

	if (UMaterialInstanceDynamic* Instance = VanthexArt::MakeSurface(this, Color, 0.62f, 0.05f))
	{
		Mesh->SetMaterial(0, Instance);
	}
}

void AVanthexTestTarget::ResetForRound()
{
	if (Health)
	{
		Health->ResetHealth();
	}

	auto RestoreMesh = [](UStaticMeshComponent* Mesh)
	{
		if (!Mesh)
		{
			return;
		}
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	};
	RestoreMesh(BodyMesh);
	RestoreMesh(HeadMesh);
	PaintMesh(BodyMesh, FLinearColor(0.75f, 0.12f, 0.08f));
	PaintMesh(HeadMesh, FLinearColor(0.95f, 0.8f, 0.15f));
}

bool AVanthexTestTarget::IsCombatCollisionEnabled() const
{
	const bool bBody = BodyMesh && BodyMesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
	const bool bHead = HeadMesh && HeadMesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
	return bBody && bHead;
}

void AVanthexTestTarget::HandleDeath(AActor* /*DeadActor*/)
{
	if (BodyMesh)
	{
		BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (HeadMesh)
	{
		HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	PaintMesh(BodyMesh, FLinearColor(0.05f, 0.05f, 0.05f));
	PaintMesh(HeadMesh, FLinearColor(0.02f, 0.02f, 0.02f));
}
