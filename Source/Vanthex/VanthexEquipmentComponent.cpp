#include "VanthexEquipmentComponent.h"

#include "VanthexCharacter.h"
#include "VanthexGameMode.h"
#include "VanthexDamage.h"
#include "VanthexArt.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Controller.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UVanthexEquipmentComponent::UVanthexEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

AVanthexCharacter* UVanthexEquipmentComponent::Character() const
{
	return Cast<AVanthexCharacter>(GetOwner());
}

float UVanthexEquipmentComponent::ServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World) return 0.f;
	const AGameStateBase* GS = World->GetGameState();
	return GS ? GS->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

bool UVanthexEquipmentComponent::IsSwitching() const
{
	return ServerTime() < State.SwitchStart + State.SwitchDuration;
}

bool UVanthexEquipmentComponent::IsSwinging() const
{
	return ServerTime() < State.SwingStart + State.SwingDuration;
}

bool UVanthexEquipmentComponent::CanUseRifle() const
{
	return State.Selected == EVanthexWeaponSlot::Rifle && !IsSwitching();
}

bool UVanthexEquipmentComponent::CanAct() const
{
	const auto* OwnerCharacter = Character();
	const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AVanthexGameMode>() : nullptr;
	return OwnerCharacter && OwnerCharacter->HasAuthority() && OwnerCharacter->GetController()
		&& OwnerCharacter->GetHealthComponent() && !OwnerCharacter->GetHealthComponent()->IsDead()
		&& Mode && Mode->IsRoundActive();
}

void UVanthexEquipmentComponent::SelectRifle() { Select(EVanthexWeaponSlot::Rifle); }
void UVanthexEquipmentComponent::SelectKnife() { Select(EVanthexWeaponSlot::Knife); }

void UVanthexEquipmentComponent::Select(EVanthexWeaponSlot Slot)
{
	if (!GetOwner()->HasAuthority()) { ServerSelect(Slot); return; }
	if ((Slot != EVanthexWeaponSlot::Rifle && Slot != EVanthexWeaponSlot::Knife)
		|| !CanAct() || IsSwitching() || IsSwinging() || Slot == State.Selected) return;
	if (auto* Rifle = Character()->GetWeapon()) { Rifle->StopFire(); Rifle->CancelReload(); }
	State.Previous = State.Selected;
	State.Selected = Slot;
	State.SwitchStart = ServerTime();
	State.SwitchDuration = FMath::Max(0.1f, SwitchDuration);
	GetOwner()->ForceNetUpdate();
	UpdatePresentation();
}

void UVanthexEquipmentComponent::ServerSelect_Implementation(EVanthexWeaponSlot Slot) { Select(Slot); }
void UVanthexEquipmentComponent::Attack()
{
	if (!GetOwner()->HasAuthority()) { ServerAttack(); return; }
	if (!CanAct() || IsSwitching() || IsSwinging() || State.Selected != EVanthexWeaponSlot::Knife) return;
	State.SwingStart = ServerTime();
	State.SwingDuration = FMath::Max(0.1f, SwingDuration);
	GetWorld()->GetTimerManager().SetTimer(StrikeTimer, this, &UVanthexEquipmentComponent::ResolveKnifeStrike,
		FMath::Clamp(StrikeDelay, 0.01f, State.SwingDuration * 0.9f), false);
	GetOwner()->ForceNetUpdate();
}
void UVanthexEquipmentComponent::ServerAttack_Implementation() { Attack(); }

void UVanthexEquipmentComponent::ResolveKnifeStrike()
{
	if (!CanAct() || IsSwitching() || !IsSwinging() || State.Selected != EVanthexWeaponSlot::Knife) return;
	auto* OwnerCharacter = Character();
	const FVector Start = OwnerCharacter->GetPawnViewLocation();
	const FVector End = Start + OwnerCharacter->GetControlRotation().Vector() * FMath::Max(1.f, KnifeRange);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VanthexKnife), false, OwnerCharacter);
	Params.AddIgnoredActor(OwnerCharacter->GetWeapon());
	FHitResult Hit;
	// First blocking surface wins, including walls and friendly players. No through-wall damage.
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) return;
	auto* Target = Hit.GetActor();
	auto* Mode = GetWorld()->GetAuthGameMode<AVanthexGameMode>();
	if (!Target || !Mode->CanDamage(OwnerCharacter, Target)) return;
	if (auto* Health = Target->FindComponentByClass<UHealthComponent>())
	{
		FVanthexDamageRequest Damage;
		Damage.InstigatorActor = OwnerCharacter;
		Damage.InstigatorController = OwnerCharacter->GetController();
		Damage.Damage = KnifeDamage;
		Damage.Hit = Hit;
		Health->ApplyDamage(Damage);
	}
}

void UVanthexEquipmentComponent::ResetEquipment()
{
	if (!GetOwner()->HasAuthority()) return;
	GetWorld()->GetTimerManager().ClearTimer(StrikeTimer);
	State = FVanthexEquipmentState();
	GetOwner()->ForceNetUpdate();
	UpdatePresentation();
}

void UVanthexEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetWorld()->GetNetMode() != NM_DedicatedServer) BuildKnife();
}

void UVanthexEquipmentComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(StrikeTimer);
	Super::EndPlay(Reason);
}

void UVanthexEquipmentComponent::BuildKnife()
{
	auto* OwnerCharacter = Character();
	if (!OwnerCharacter) return;
	KnifeRoot = NewObject<USceneComponent>(OwnerCharacter, TEXT("KnifePivot"));
	KnifeRoot->SetupAttachment(OwnerCharacter->GetFirstPersonCamera());
	KnifeRoot->RegisterComponent();
	auto* Steel = VanthexArt::MakeSurface(OwnerCharacter, FLinearColor(0.3f, 0.32f, 0.34f), 0.3f, 0.9f);
	auto* Grip = VanthexArt::MakeSurface(OwnerCharacter, FLinearColor(0.025f, 0.028f, 0.03f), 0.8f, 0.05f);
	auto AddPart = [&](UStaticMesh* Mesh, const FVector& Position, const FRotator& Rotation, const FVector& Scale, UMaterialInterface* Material)
	{
		auto* Part = NewObject<UStaticMeshComponent>(OwnerCharacter);
		Part->SetupAttachment(KnifeRoot);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeLocation(Position);
		Part->SetRelativeRotation(Rotation);
		Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->SetMaterial(0, Material);
		Part->RegisterComponent();
	};
	if (KnifeMesh) AddPart(KnifeMesh, FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, Steel);
	else
	{
		// Replaceable prototype silhouette: grip, guard and tapered steel blade.
		AddPart(VanthexArt::LoadCube(), FVector(-4,0,0), FRotator::ZeroRotator, FVector(0.1f,0.025f,0.03f), Grip);
		AddPart(VanthexArt::LoadCube(), FVector(2,0,0), FRotator::ZeroRotator, FVector(0.015f,0.065f,0.025f), Steel);
		AddPart(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone")), FVector(12,0,0), FRotator(90,0,0), FVector(0.007f,0.045f,0.2f), Steel);
	}
	UpdatePresentation();
}

void UVanthexEquipmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetWorld()->GetNetMode() != NM_DedicatedServer) UpdatePresentation();
}

void UVanthexEquipmentComponent::UpdatePresentation()
{
	auto* OwnerCharacter = Character();
	if (!OwnerCharacter) return;
	const float Now = ServerTime();
	const float Progress = FMath::Clamp((Now - State.SwitchStart) / State.SwitchDuration, 0.f, 1.f);
	const bool bHolstering = Progress < 0.5f;
	const EVanthexWeaponSlot VisibleSlot = bHolstering ? State.Previous : State.Selected;
	const float LowerLinear = bHolstering ? Progress * 2.f : (1.f - Progress) * 2.f;
	const float Lower = FMath::SmoothStep(0.f, 1.f, LowerLinear);
	const bool bAlive = OwnerCharacter->GetHealthComponent() && !OwnerCharacter->GetHealthComponent()->IsDead();
	if (auto* Rifle = OwnerCharacter->GetWeapon())
	{
		if (Rifle->GetAttachParentActor() != OwnerCharacter)
			Rifle->AttachToComponent(OwnerCharacter->GetFirstPersonCamera(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Rifle->SetActorHiddenInGame(!bAlive || VisibleSlot != EVanthexWeaponSlot::Rifle);
		Rifle->SetActorRelativeLocation(FVector(30.f - 12.f * Lower, 14.f, -12.f - 55.f * Lower));
		Rifle->SetActorRelativeRotation(FRotator(-35.f * Lower, 0.f, 18.f * Lower));
	}
	if (KnifeRoot)
	{
		KnifeRoot->SetVisibility(bAlive && VisibleSlot == EVanthexWeaponSlot::Knife, true);
		const float SwingProgress = FMath::Clamp((Now - State.SwingStart) / State.SwingDuration, 0.f, 1.f);
		// Fast thrust, slower recovery; strike lands at the thrust's peak.
		const float Contact = FMath::Clamp(StrikeDelay / State.SwingDuration, 0.05f, 0.9f);
		const float Swing = FMath::SmoothStep(0.f, 1.f, SwingProgress < Contact ? SwingProgress / Contact : (1.f - SwingProgress) / (1.f - Contact));
		KnifeRoot->SetRelativeLocation(FVector(42.f + 20.f * Swing - 10.f * Lower, 18.f - 9.f * Swing, -20.f - 50.f * Lower + 4.f * Swing));
		KnifeRoot->SetRelativeRotation(FRotator(10.f - 35.f * Lower - 12.f * Swing, -8.f - 15.f * Swing, -12.f + 35.f * Lower));
	}
}

void UVanthexEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVanthexEquipmentComponent, State);
}
