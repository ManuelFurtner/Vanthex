#include "VanthexCore.h"
#include "Net/UnrealNetwork.h"

#include "Vanthex.h"
#include "VanthexMapLayout.h"
#include "VanthexArt.h"
#include "VanthexCharacter.h"
#include "VanthexGameMode.h"
#include "VanthexPlayerState.h"
#include "HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"

AVanthexCore::AVanthexCore()
{
	PrimaryActorTick.bCanEverTick = true;
 bReplicates = true;
 bAlwaysRelevant = true;
 SetReplicateMovement(true);
	bReplicates = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetRelativeScale3D(FVector(1.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetVisibility(false);
	Body->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Body->SetStaticMesh(CubeMesh.Object);
	}
}

void AVanthexCore::BeginPlay()
{
	Super::BeginPlay();
	BuildPresentation();
}

void AVanthexCore::BuildPresentation()
{
	UStaticMesh* Cube = VanthexArt::LoadCube();
	UStaticMesh* Sphere = VanthexArt::LoadSphere();
	UStaticMesh* Cylinder = VanthexArt::LoadCylinder();
	UMaterialInstanceDynamic* Metal = VanthexArt::MakeSurface(this, FLinearColor(0.16f, 0.17f, 0.18f), 0.38f, 0.82f);
	UMaterialInstanceDynamic* Energy = VanthexArt::MakeEnergy(this, FLinearColor(0.45f, 0.95f, 1.f), 8.f);
	const FLinearColor Cyan(0.25f, 0.85f, 1.f);
	UMaterialInstanceDynamic* Trim = VanthexArt::MakeSurface(this, FLinearColor(0.08f, 0.1f, 0.12f), 0.45f, 0.7f, Cyan, 0.35f, EVanthexSurface::DarkMetal);

	auto Part = [&](UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, UMaterialInterface* Material)
	{
		return VanthexArt::AddMesh(this, Body, Mesh, GetActorLocation() + Location, FRotator::ZeroRotator, Scale, Material, false);
	};

	// Licensed Energy geometry remains optional and local. Public checkouts use the original kit.
 UStaticMesh* Imported=nullptr;
 const TCHAR* EnergyPath=TEXT("/Game/Vanthex/Imported/Core/SM_EnergyCore");
 if(!FParse::Param(FCommandLine::Get(),TEXT("VanthexOwnCoreModel"))&&FPackageName::DoesPackageExist(EnergyPath))Imported=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Vanthex/Imported/Core/SM_EnergyCore.SM_EnergyCore"));
 if(Imported){
  auto* Visual=NewObject<UStaticMeshComponent>(this,TEXT("EnergyCapsule"));
  Visual->SetupAttachment(Body);Visual->SetStaticMesh(Imported);Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Visual->SetGenerateOverlapEvents(false);AddInstanceComponent(Visual);Visual->RegisterComponent();
  Visual->ComponentTags.Add(TEXT("VanthexEnergyCore"));
 }else{
 if (UStaticMesh* Device = VanthexArt::LoadKit(TEXT("SM_CoreDevice")))
	{
		if (UStaticMeshComponent* Frame = Part(Device, FVector::ZeroVector, FVector(1.15f), Metal))
		{
			Frame->SetMaterial(1, Trim);
		}
	}
	else
	{
		Part(Cube, FVector(0.f, 0.f, 0.f), FVector(0.05f, 0.05f, 0.62f), Metal);
		Part(Cube, FVector(0.f, 0.f, 26.f), FVector(0.5f, 0.5f, 0.035f), Metal);
		Part(Cube, FVector(0.f, 0.f, -26.f), FVector(0.5f, 0.5f, 0.035f), Metal);
		Part(Cube, FVector(22.f, 0.f, 0.f), FVector(0.035f, 0.28f, 0.52f), Trim);
		Part(Cube, FVector(-22.f, 0.f, 0.f), FVector(0.035f, 0.28f, 0.52f), Trim);
		Part(Cube, FVector(0.f, 22.f, 0.f), FVector(0.28f, 0.035f, 0.52f), Trim);
		Part(Cube, FVector(0.f, -22.f, 0.f), FVector(0.28f, 0.035f, 0.52f), Trim);
	}

	EnergyChamber = Part(Sphere, FVector::ZeroVector, FVector(0.32f), Energy);
	EnergyRing = Part(Cylinder, FVector::ZeroVector, FVector(0.46f, 0.46f, 0.012f), Energy);

	}
 UMaterialInstanceDynamic* TrailMaterial = VanthexArt::MakeEnergy(this, Imported?FLinearColor(1.f,.32f,.055f):FLinearColor(0.45f, 0.95f, 1.f), 2.2f);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		UStaticMeshComponent* Segment = VanthexArt::AddMesh(this, Body, Sphere, GetActorLocation(), FRotator::ZeroRotator, FVector(0.08f), TrailMaterial, false);
		if (!Segment)
		{
			continue;
		}
		Segment->SetUsingAbsoluteLocation(true);
		Segment->SetUsingAbsoluteScale(true);
		Segment->SetVisibility(false);
		TrailMeshes.Add(Segment);
	}
}

void AVanthexCore::UpdatePresentation(float DeltaSeconds)
{
	PresentationTime += DeltaSeconds;
	if (EnergyChamber)
	{
		const float Pulse = 0.3f + 0.03f * FMath::Sin(PresentationTime * 3.f);
		EnergyChamber->SetRelativeScale3D(FVector(Pulse));
	}
	if (EnergyRing)
	{
		EnergyRing->AddLocalRotation(FRotator(0.f, 70.f * DeltaSeconds, 0.f));
	}

	const bool bFlying = State == EVanthexCoreState::Thrown;
	if (!bFlying)
	{
		TrailPoints.Reset();
	}
	else
	{
		const FVector Now = GetActorLocation();
		if (TrailPoints.Num() == 0 || FVector::Dist(TrailPoints.Last(), Now) > 40.f)
		{
			TrailPoints.Add(Now);
			while (TrailPoints.Num() > TrailMeshes.Num())
			{
				TrailPoints.RemoveAt(0);
			}
		}
	}

	for (int32 Index = 0; Index < TrailMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Segment = TrailMeshes[Index];
		if (!Segment)
		{
			continue;
		}
		const int32 PointIndex = TrailPoints.Num() - 1 - Index;
		if (!bFlying || PointIndex < 0)
		{
			Segment->SetVisibility(false);
			continue;
		}
		const float Fade = 1.f - static_cast<float>(Index) / static_cast<float>(TrailMeshes.Num());
		Segment->SetVisibility(true);
		Segment->SetWorldLocation(TrailPoints[PointIndex]);
		Segment->SetWorldScale3D(FVector(0.16f * Fade));
	}
}

void AVanthexCore::SetHome(const FVector& InHome)
{
	Home = InHome;
	if (State == EVanthexCoreState::AtRest && !Carrier)
	{
		SetActorLocation(Home);
	}
}

void AVanthexCore::ResetToSpawn()
{
	bRewardEligible = false;
	if (Carrier)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	Carrier = nullptr;
	Thrower = nullptr;
	PickupLock = nullptr;
	Velocity = FVector::ZeroVector;
	bCatchEnabled = false;
	OutOfBoundsTime = 0.f;
	State = EVanthexCoreState::AtRest;
	SetActorLocation(Home);
	SetActorRotation(FRotator::ZeroRotator);
}

bool AVanthexCore::IsLiving(const AVanthexCharacter* Character) const
{
	const UHealthComponent* Health = Character ? Character->GetHealthComponent() : nullptr;
	return Character && Health && !Health->IsDead();
}

bool AVanthexCore::IsRoundActive() const
{
	const AVanthexGameMode* Mode = GetWorld() ? Cast<AVanthexGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
	return Mode && Mode->IsRoundActive();
}

EVanthexTeam AVanthexCore::GetCarrierTeam() const
{
	const AController* Controller = Carrier ? Carrier->GetController() : nullptr;
	const AVanthexPlayerState* PlayerState = Controller ? Controller->GetPlayerState<AVanthexPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetTeam() : EVanthexTeam::None;
}

bool AVanthexCore::IsAtHome() const
{
	return FVector::Dist2D(GetActorLocation(), Home) < 80.f && FMath::Abs(GetActorLocation().Z - Home.Z) < 40.f;
}

void AVanthexCore::AttachToCarrier(AVanthexCharacter* Character)
{
	Carrier = Character;
	Thrower = nullptr;
	Velocity = FVector::ZeroVector;
	bCatchEnabled = false;
	OutOfBoundsTime = 0.f;
	State = EVanthexCoreState::Carried;
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		SetActorRelativeLocation(FVector(50.f, 0.f, 30.f));
		SetActorRelativeRotation(FRotator::ZeroRotator);
	}
	UE_LOG(LogVanthex, Display, TEXT("Core carried by %s"), *GetNameSafe(Character));
}

bool AVanthexCore::TryPickup(AVanthexCharacter* Character)
{
	if (!HasAuthority() || !IsLiving(Character) || !IsRoundActive())
	{
		return false;
	}
	if (State != EVanthexCoreState::AtRest || Carrier)
	{
		return false;
	}
	if (Character == PickupLock && GetWorld() && GetWorld()->GetTimeSeconds() < PickupUnlockTime)
	{
		return false;
	}
	if (FVector::Dist(Character->GetActorLocation(), GetActorLocation()) > PickupRadius)
	{
		return false;
	}
	FCollisionQueryParams PickupQuery(SCENE_QUERY_STAT(CorePickupSight),false,this);
	PickupQuery.AddIgnoredActor(Character);
	FCollisionObjectQueryParams PickupObjects; PickupObjects.AddObjectTypesToQuery(ECC_WorldStatic); PickupObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult PickupHit;
	if (GetWorld()->LineTraceSingleByObjectType(PickupHit,Character->GetActorLocation(),GetActorLocation(),PickupObjects,PickupQuery)) return false;

	AttachToCarrier(Character);
	return true;
}

void AVanthexCore::Release(const FVector& WorldLocation, const FVector& NewVelocity, bool bAllowCatch)
{
	bRewardEligible = false;
	AVanthexCharacter* Previous = Carrier;
 FVector SafeLocation=WorldLocation;
 if (Previous) {
  FCollisionQueryParams Params(SCENE_QUERY_STAT(CoreRelease),false,this); Params.AddIgnoredActor(Previous);
  FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
  FHitResult Hit;
  if (GetWorld()->SweepSingleByObjectType(Hit,Previous->GetActorLocation(),WorldLocation,FQuat::Identity,Objects,FCollisionShape::MakeSphere(20.f),Params)) SafeLocation=Hit.Location+Hit.Normal;
 }
	if (Previous)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	Carrier = nullptr;
	Thrower = Previous;
	PickupLock = Previous;
	PickupUnlockTime = GetWorld() ? GetWorld()->GetTimeSeconds() + DropIgnoreTime : 0.f;
	ReleaseOrigin = SafeLocation;
	Velocity = NewVelocity;
	bCatchEnabled = bAllowCatch;
	ThrowTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	State = EVanthexCoreState::Thrown;
	SetActorLocation(SafeLocation);
	LastRewardPosition = SafeLocation;
	SetActorRotation(FRotator::ZeroRotator);
}

void AVanthexCore::Drop()
{
	if (!HasAuthority() || !Carrier || (State != EVanthexCoreState::Carried && State != EVanthexCoreState::Extracting))
	{
		return;
	}

	const FVector Forward = Carrier->GetControlRotation().Vector().GetSafeNormal2D();
	const FVector Origin = Carrier->GetActorLocation() + FVector(0.f, 0.f, 40.f) + Forward * 70.f;
	Release(Origin, Forward * 180.f + FVector(0.f, 0.f, 40.f), false);
	UE_LOG(LogVanthex, Display, TEXT("Core dropped"));
}

void AVanthexCore::Throw()
{
	if (!HasAuthority() || !IsRoundActive() || !IsLiving(Carrier) || (State != EVanthexCoreState::Carried && State != EVanthexCoreState::Extracting))
	{
		return;
	}

	const FVector Direction = Carrier->GetControlRotation().Vector().GetSafeNormal();
	const FVector Origin = Carrier->GetActorLocation() + FVector(0.f, 0.f, 60.f) + Direction * 90.f;
	Release(Origin, Direction * ThrowSpeed, true);
	if (++RewardThrowId == 0) ++RewardThrowId;
	bRewardEligible = true;
	UE_LOG(LogVanthex, Display, TEXT("Core thrown speed %.0f"), Velocity.Size());
}

void AVanthexCore::DropFromDeath()
{
	if (!HasAuthority() || !Carrier)
	{
		return;
	}

	const FVector Origin = Carrier->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	Release(Origin, FVector(0.f, 0.f, 80.f), true);
	UE_LOG(LogVanthex, Display, TEXT("Core dropped on death"));
}

void AVanthexCore::SetExtracting(bool bExtracting)
{
	if (!Carrier)
	{
		return;
	}
	State = bExtracting ? EVanthexCoreState::Extracting : EVanthexCoreState::Carried;
}

void AVanthexCore::MarkExtracted()
{
	if (Carrier)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	Carrier = nullptr;
	Velocity = FVector::ZeroVector;
	bCatchEnabled = false;
	State = EVanthexCoreState::Extracted;
}

void AVanthexCore::EnterAtRest()
{
	bRewardEligible = false;
	State = EVanthexCoreState::AtRest;
	Velocity = FVector::ZeroVector;
	bCatchEnabled = false;
	Carrier = nullptr;
	FVector Rest = GetActorLocation();
	if (!VanthexMap::IsGridV3(GetWorld())) Rest.Z = FMath::Max(Rest.Z, 40.f);
	SetActorLocation(Rest);
}

void AVanthexCore::TryCatch()
{
	if (!bCatchEnabled || State != EVanthexCoreState::Thrown || !GetWorld() || !IsRoundActive())
	{
		return;
	}
	if (GetWorld()->GetTimeSeconds() <= ThrowTime)
	{
		return;
	}

	AVanthexCharacter* Best = nullptr;
	float BestDist = CatchRadius;
	for (TActorIterator<AVanthexCharacter> It(GetWorld()); It; ++It)
	{
		AVanthexCharacter* Candidate = *It;
		if (!IsLiving(Candidate))
		{
			continue;
		}
		if (Candidate == Thrower && FVector::Dist(GetActorLocation(), ReleaseOrigin) < 250.f)
		{
			continue;
		}
		if (Candidate == PickupLock && GetWorld()->GetTimeSeconds() < PickupUnlockTime)
		{
			continue;
		}
		const float Dist = FVector::Dist(Candidate->GetActorLocation(), GetActorLocation());
		if (Dist <= BestDist)
		{
			FCollisionQueryParams CatchQuery(SCENE_QUERY_STAT(CoreCatchSight),false,this); CatchQuery.AddIgnoredActor(Candidate);
			FCollisionObjectQueryParams CatchObjects; CatchObjects.AddObjectTypesToQuery(ECC_WorldStatic); CatchObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
			FHitResult CatchHit;
			if (GetWorld()->LineTraceSingleByObjectType(CatchHit,GetActorLocation(),Candidate->GetActorLocation(),CatchObjects,CatchQuery)) continue;
			BestDist = Dist;
			Best = Candidate;
		}
	}

	if (Best)
	{
		if (bRewardEligible) if (auto* Mode = GetWorld()->GetAuthGameMode<AVanthexGameMode>()) Mode->NotifyCoreCaught(RewardThrowId, Thrower, Best, ReleaseOrigin, GetActorLocation());
		bRewardEligible = false;
		AttachToCarrier(Best);
		UE_LOG(LogVanthex, Display, TEXT("Core caught by %s"), *GetNameSafe(Best));
	}
}

void AVanthexCore::RecoverIfOutOfBounds(float DeltaSeconds)
{
	if (State == EVanthexCoreState::Carried || State == EVanthexCoreState::Extracted || State == EVanthexCoreState::Extracting)
	{
		OutOfBoundsTime = 0.f;
		return;
	}

	const FVector Location = GetActorLocation();
	const float RecoveryFloor = VanthexMap::IsGridV3(GetWorld()) ? -1800.f : -100.f;
	const bool bOutside = Location.Z < RecoveryFloor || Location.Z > 2500.f || FMath::Abs(Location.X) > VanthexMap::Extent(GetWorld()).X + 300.f || FMath::Abs(Location.Y) > VanthexMap::Extent(GetWorld()).Y + 300.f;
	if (!bOutside)
	{
		OutOfBoundsTime = 0.f;
		return;
	}

	OutOfBoundsTime += DeltaSeconds;
	if (OutOfBoundsTime >= 1.f)
	{
		UE_LOG(LogVanthex, Display, TEXT("Core recovered to center from %s"), *Location.ToCompactString());
		ResetToSpawn();
	}
}

void AVanthexCore::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
 if (!HasAuthority()) { UpdatePresentation(DeltaSeconds); return; }
	if (!HasAuthority())
	{
		return;
	}

	if (State == EVanthexCoreState::Carried && !IsLiving(Carrier))
	{
		DropFromDeath();
	}

	if (State == EVanthexCoreState::Thrown)
	{
		// External teleports/reset fixtures cannot turn a throw into a credit event.
		if (!GetActorLocation().Equals(LastRewardPosition, .1f)) bRewardEligible = false;
		float Remaining = DeltaSeconds;
		while (Remaining > KINDA_SMALL_NUMBER && State == EVanthexCoreState::Thrown)
		{
			const float Step = FMath::Min(Remaining, 0.02f);
			Remaining -= Step;
			Velocity.Z -= 980.f * Step;
			const FVector Current = GetActorLocation();
			FVector Next = Current + Velocity * Step;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(CoreMovement), false, this);
			FCollisionObjectQueryParams Objects;
			Objects.AddObjectTypesToQuery(ECC_WorldStatic);
 Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
			FHitResult Hit;
			if (GetWorld()->SweepSingleByObjectType(Hit, Current, Next, FQuat::Identity,
				Objects, FCollisionShape::MakeSphere(20.f), Params))
			{
				if(FParse::Param(FCommandLine::Get(),TEXT("GridV3GameTest")) && Velocity.Size()>1000.f)
				 UE_LOG(LogTemp,Display,TEXT("GRIDV3 CORE CONTACT position=%s normal=%s component=%s"),*Hit.Location.ToString(),*Hit.Normal.ToString(),*GetNameSafe(Hit.GetComponent()));
				SetActorLocation(Hit.Location + Hit.Normal * 0.5f);
				Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal) * 0.5f;
				if (Hit.Normal.Z > 0.5f && Velocity.Size() < 60.f) { EnterAtRest(); }
				TryCatch();
				continue;
			}
			if (!VanthexMap::IsGridV3(GetWorld()) && Next.Z <= 40.f && Velocity.Z <= 0.f)
			{
				Next.Z = 40.f;
				Velocity.Z = 0.f;
				Velocity.X *= 0.5f;
				Velocity.Y *= 0.5f;
				SetActorLocation(Next);
				if (Velocity.Size2D() < 60.f)
				{
					EnterAtRest();
				}
			}
			else
			{
				if (FMath::Abs(Next.X) > VanthexMap::Extent(GetWorld()).X + 100.f)
				{
					Next.X = FMath::Clamp(Next.X, -VanthexMap::Extent(GetWorld()).X - 100.f, VanthexMap::Extent(GetWorld()).X + 100.f);
					Velocity.X *= -0.2f;
				}
				if (FMath::Abs(Next.Y) > VanthexMap::Extent(GetWorld()).Y + 100.f)
				{
					Next.Y = FMath::Clamp(Next.Y, -VanthexMap::Extent(GetWorld()).Y - 100.f, VanthexMap::Extent(GetWorld()).Y + 100.f);
					Velocity.Y *= -0.2f;
				}
				SetActorLocation(Next);
			}
			TryCatch();
		}
	}

	RecoverIfOutOfBounds(DeltaSeconds);
	LastRewardPosition = GetActorLocation();
	UpdatePresentation(DeltaSeconds);
}

void AVanthexCore::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(AVanthexCore, State);
 DOREPLIFETIME(AVanthexCore, Carrier);
}
