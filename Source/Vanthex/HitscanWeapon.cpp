#include "HitscanWeapon.h"
#include "VanthexCharacter.h"

#include "Vanthex.h"
#include "VanthexArt.h"
#include "VanthexDamage.h"
#include "VanthexBulletAudioComponent.h"
#include "VanthexGameMode.h"
#include "HealthComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Math/RotationMatrix.h"
#include "Engine/Engine.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

AHitscanWeapon::AHitscanWeapon()
{
	ViewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ViewMesh"));
	PrimaryActorTick.bCanEverTick = true;
	ViewMesh->SetupAttachment(WeaponRoot);
	ViewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ViewMesh->SetRelativeScale3D(FVector(0.25f, 0.06f, 0.08f));
	ViewMesh->SetRelativeLocation(FVector(25.f, 0.f, 0.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ViewMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AHitscanWeapon::BeginPlay()
{
	Super::BeginPlay();
	RiflePivot = NewObject<USceneComponent>(this, TEXT("RiflePivot"));
	RiflePivot->SetupAttachment(WeaponRoot);
	RiflePivot->SetRelativeLocation(FVector(28.f, 8.f, -6.f));
	RiflePivot->RegisterComponent();

	UStaticMesh* Cube = VanthexArt::LoadCube();
	UStaticMesh* RifleMesh = VanthexArt::LoadKit(TEXT("SM_Rifle"));
	UMaterialInstanceDynamic* Metal = VanthexArt::MakeSurface(this, FLinearColor(0.045f, 0.046f, 0.048f), 0.34f, 0.88f, FLinearColor::Black, 0.f, EVanthexSurface::BareSteel);
	UMaterialInstanceDynamic* Dark = VanthexArt::MakeSurface(this, FLinearColor(0.02f, 0.02f, 0.022f), 0.62f, 0.08f, FLinearColor::Black, 0.f, EVanthexSurface::Rubber);
	UMaterialInstanceDynamic* Optic = VanthexArt::MakeEnergy(this, FLinearColor(1.f, 0.22f, 0.12f), 1.4f);

	if (ViewMesh)
	{
		ViewMesh->SetVisibility(false);
	}

	UStaticMesh* BodyMesh = RifleMesh ? RifleMesh : Cube;
	const FVector BodyScale = RifleMesh ? FVector(1.f) : FVector(0.22f, 0.045f, 0.055f);
	if (UStaticMeshComponent* Rifle = VanthexArt::AddMesh(this, RiflePivot, BodyMesh, RiflePivot->GetComponentLocation(), RiflePivot->GetComponentRotation(), BodyScale, Metal, false))
	{
		Rifle->SetMaterial(1, Optic);
		if (!RifleMesh)
		{
			Rifle->SetMaterial(0, Dark);
		}
	}

	MuzzleFlash = VanthexArt::AddMesh(this, RiflePivot, VanthexArt::LoadSphere(), FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.07f, 0.04f, 0.04f), VanthexArt::MakeEnergy(this, FLinearColor(1.f, 0.42f, 0.08f), 3.2f), false);
	if (MuzzleFlash)
	{
		MuzzleFlash->SetRelativeLocation(FVector(24.f, 0.f, 2.2f));
		MuzzleFlash->SetVisibility(false);
	}
}

void AHitscanWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Recoil = FMath::FInterpTo(Recoil, 0.f, DeltaSeconds, 14.f);
	if (RiflePivot)
	{
		float Sprint = 0.f;
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			Sprint = OwnerPawn->GetVelocity().Size2D() > 750.f ? 1.f : 0.f;
		}
		const float Kick = Sprint * 3.5f;
		RiflePivot->SetRelativeLocation(FVector(28.f - Recoil * 1.4f + Kick, 8.f, -6.f - Kick));
		RiflePivot->SetRelativeRotation(FRotator(Recoil * 0.35f, 0.f, Sprint * -2.f));
	}
	if (MuzzleFlash)
	{
		MuzzleTime = FMath::Max(0.f, MuzzleTime - DeltaSeconds);
		MuzzleFlash->SetVisibility(MuzzleTime > 0.f);
	}
}

void AHitscanWeapon::PresentImpact(const FHitResult& Hit)
{
	UWorld* World = GetWorld();
	if (!World || !Hit.bBlockingHit)
	{
		return;
	}

	const bool bMetal = Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(TEXT("Metal"));
	const FLinearColor DecalColor = bMetal ? FLinearColor(0.12f, 0.11f, 0.1f) : FLinearColor(0.32f, 0.3f, 0.26f);
	const FLinearColor SparkColor = bMetal ? FLinearColor(1.f, 0.55f, 0.16f) : FLinearColor(0.42f, 0.39f, 0.34f);
	if (Hit.GetActor() && Hit.GetComponent() && !Hit.GetActor()->FindComponentByClass<UHealthComponent>())
	{
		UDecalComponent* Decal = NewObject<UDecalComponent>(Hit.GetActor());
		Decal->SetupAttachment(Hit.GetComponent());
		Decal->SetWorldLocationAndRotation(Hit.ImpactPoint + Hit.ImpactNormal, FRotationMatrix::MakeFromX(-Hit.ImpactNormal).Rotator());
		Decal->DecalSize = FVector(6.f, 7.f, 7.f);
		Decal->RegisterComponent();
		if (UMaterialInterface* DecalMaterial = VanthexArt::GetDecalMaterial())
		{
			if (UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(DecalMaterial, Hit.GetActor()))
			{
				Instance->SetVectorParameterValue(TEXT("Color"), DecalColor);
				Instance->SetScalarParameterValue(TEXT("Opacity"), bMetal ? 0.8f : 0.55f);
				Instance->SetScalarParameterValue(TEXT("Stripe"), 0.f);
				Decal->SetDecalMaterial(Instance);
			}
		}
		Decal->SetFadeOut(1.2f, 2.f, false);
	}

	AActor* Spark = World->SpawnActor<AActor>(Hit.ImpactPoint, FRotator::ZeroRotator);
	if (!Spark)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Spark);
	Spark->SetRootComponent(Root);
	Root->RegisterComponent();
	UStaticMesh* Sphere = VanthexArt::LoadSphere();
	const int32 SpeckCount = bMetal ? 3 : 2;
	for (int32 Index = 0; Index < SpeckCount; ++Index)
	{
		const FVector Offset = Hit.ImpactNormal * (1.5f + Index * 2.f) + FVector(Index == 1 ? 2.f : -1.f, Index == 2 ? 2.f : 0.f, 1.f);
		UStaticMeshComponent* Speck = VanthexArt::AddMesh(Spark, Root, Sphere, Hit.ImpactPoint + Offset, FRotator::ZeroRotator, FVector(bMetal ? 0.025f : 0.055f), VanthexArt::MakeEnergy(Spark, SparkColor, bMetal ? 3.2f : 0.25f), false);
		if (Speck)
		{
			Speck->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	Spark->SetLifeSpan(bMetal ? 0.08f : 0.16f);
}

void AHitscanWeapon::PresentShotFeedback(const FRotator& ViewRotation)
{
	UWorld* World = GetWorld();
	if (!World || !MuzzleFlash)
	{
		return;
	}

	const FVector Muzzle = MuzzleFlash->GetComponentLocation();
	MuzzleFlash->SetVisibility(true);
	const FVector Right = ViewRotation.RotateVector(FVector(0.f, 1.f, 0.f));
	const FVector Up = FVector::UpVector;

	if (AActor* Smoke = World->SpawnActor<AActor>(Muzzle, ViewRotation))
	{
		USceneComponent* Root = NewObject<USceneComponent>(Smoke);
		Smoke->SetRootComponent(Root);
		Root->RegisterComponent();
		if (UStaticMeshComponent* Puff = VanthexArt::AddMesh(Smoke, Root, VanthexArt::LoadSphere(), Muzzle + ViewRotation.Vector() * 14.f, ViewRotation, FVector(0.04f, 0.035f, 0.035f), VanthexArt::MakeEnergy(Smoke, FLinearColor(0.22f, 0.21f, 0.19f), 0.18f), false))
		{
			Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Smoke->SetLifeSpan(0.18f);
	}

	if (AActor* Shell = World->SpawnActor<AActor>(Muzzle, ViewRotation))
	{
		UStaticMeshComponent* Casing = NewObject<UStaticMeshComponent>(Shell);
		Casing->SetStaticMesh(VanthexArt::LoadCylinder());
		Casing->SetRelativeScale3D(FVector(0.012f, 0.012f, 0.035f));
		Casing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Casing->SetCastShadow(false);
		Shell->SetRootComponent(Casing);
		Casing->RegisterComponent();
		if (UMaterialInstanceDynamic* Brass = VanthexArt::MakeSurface(Shell, FLinearColor(0.55f, 0.4f, 0.16f), 0.28f, 0.9f, FLinearColor::Black, 0.f, EVanthexSurface::BareSteel))
		{
			Casing->SetMaterial(0, Brass);
		}
		UProjectileMovementComponent* Move = NewObject<UProjectileMovementComponent>(Shell);
		Move->UpdatedComponent = Casing;
		Move->bRotationFollowsVelocity = true;
		Move->ProjectileGravityScale = 1.4f;
		Move->MaxSpeed = 900.f;
		Move->Velocity = Right * 260.f + Up * 140.f - ViewRotation.Vector() * 30.f;
		Move->RegisterComponent();
		Shell->SetLifeSpan(0.55f);
	}
}

void AHitscanWeapon::ResetForRound()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimer);
	}
	NextFireTime = 0.f;
	Super::ResetForRound();
}

void AHitscanWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AHitscanWeapon::StartFire()
{
	if (const auto* Character = Cast<AVanthexCharacter>(GetOwner())) { if (!Character->CanUseRifle()) return; }
	Super::StartFire();
	if (bReloading)
	{
		return;
	}

	FireShot();
	if (bReloading)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FireTimer, this, &AHitscanWeapon::FireShot, FireInterval, true, FireInterval);
	}
}

void AHitscanWeapon::StartReload()
{
	if (const auto* Character = Cast<AVanthexCharacter>(GetOwner())) { if (!Character->CanUseRifle()) return; }
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimer);
	}
	Super::StartReload();
}

void AHitscanWeapon::StopFire()
{
	Super::StopFire();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimer);
	}
}

void AHitscanWeapon::FinishReload()
{
	Super::FinishReload();
	if (bTriggerHeld)
	{
		StartFire();
	}
}

bool AHitscanWeapon::IsHeadHit(const FHitResult& Hit) const
{
	if (const UPrimitiveComponent* HitComponent = Hit.GetComponent())
	{
		if (HitComponent->ComponentHasTag(TEXT("Head")))
		{
			return true;
		}
	}

	const FString Bone = Hit.BoneName.ToString();
	return Bone.Contains(TEXT("head"), ESearchCase::IgnoreCase);
}

void AHitscanWeapon::FireShot()
{
 if (!HasAuthority()) return;
	if (const auto* Character = Cast<AVanthexCharacter>(GetOwner())) { if (!Character->CanUseRifle()) { StopFire(); return; } }
	UWorld* World = GetWorld();
	if (!World || bReloading)
	{
		return;
	}

	if (const AVanthexGameMode* Mode = Cast<AVanthexGameMode>(World->GetAuthGameMode()))
	{
		if (!Mode->IsRoundActive())
		{
			return;
		}
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	if (const UHealthComponent* OwnerHealth = OwnerPawn->FindComponentByClass<UHealthComponent>())
	{
		if (OwnerHealth->IsDead())
		{
			StopFire();
			return;
		}
	}

	const float Now = World->GetTimeSeconds();
	if (Now + 0.02f < NextFireTime)
	{
		return;
	}

	if (AmmoInMagazine <= 0)
	{
		StartReload();
		return;
	}

	AController* Controller = OwnerPawn->GetController();
	if (!Controller)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	ViewLocation = OwnerPawn->GetPawnViewLocation();
	ViewRotation = Controller->GetControlRotation();

	const FVector TraceEnd = ViewLocation + (ViewRotation.Vector() * TraceRange);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(VanthexHitscan), false, OwnerPawn);
	Params.AddIgnoredActor(this);
	Params.bReturnPhysicalMaterial = true;

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, Params);
	MulticastShot(ViewRotation, Hit);

	NextFireTime = Now + FireInterval;
	SetMagazineAmmo(AmmoInMagazine - 1);
	UVanthexBulletAudioComponent::DispatchShot(World, OwnerPawn, ViewLocation, TraceEnd, Hit, bSupersonicBullet);

	if (!bHit || !Hit.GetActor())
	{
		UE_LOG(LogVanthex, Display, TEXT("Hitscan miss  ammo %d/%d"), AmmoInMagazine, MagazineSize);
		return;
	}

	const bool bHeadshot = IsHeadHit(Hit);
	const float Damage = bHeadshot ? BodyDamage * HeadshotMultiplier : BodyDamage;

	if (UHealthComponent* Health = Hit.GetActor()->FindComponentByClass<UHealthComponent>())
	{
		if (const AVanthexGameMode* Mode = Cast<AVanthexGameMode>(World->GetAuthGameMode()))
		{
			if (!Mode->CanDamage(OwnerPawn, Hit.GetActor()))
			{
				UE_LOG(LogVanthex, Display, TEXT("Hitscan blocked friendly fire on %s"), *GetNameSafe(Hit.GetActor()));
				return;
			}
		}

		FVanthexDamageRequest Request;
		Request.InstigatorActor = OwnerPawn;
		Request.InstigatorController = Controller;
		Request.Damage = Damage;
		Request.bHeadshot = bHeadshot;
		Request.Hit = Hit;
		Health->ApplyDamage(Request);
	}
	else
	{
		UE_LOG(LogVanthex, Display, TEXT("Hitscan hit %s/%s at %s (no health)  ammo %d/%d"),
			*GetNameSafe(Hit.GetActor()),
			*GetNameSafe(Hit.GetComponent()),
			*Hit.ImpactPoint.ToCompactString(),
			AmmoInMagazine,
			MagazineSize);
	}
}

void AHitscanWeapon::MulticastShot_Implementation(FRotator ViewRotation, FHitResult Hit) { Recoil = 2.2f; MuzzleTime = 0.035f; PresentShotFeedback(ViewRotation); if (Hit.bBlockingHit) PresentImpact(Hit); }
