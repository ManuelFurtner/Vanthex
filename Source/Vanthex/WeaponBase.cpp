#include "WeaponBase.h"
#include "Net/UnrealNetwork.h"

#include "Vanthex.h"
#include "Engine/Engine.h"

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
 bNetUseOwnerRelevancy = true;

	WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
	SetRootComponent(WeaponRoot);

	AmmoInMagazine = MagazineSize;
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	SetMagazineAmmo(MagazineSize);
}

void AWeaponBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AWeaponBase::SetMagazineAmmo(int32 NewAmmo)
{
	AmmoInMagazine = FMath::Clamp(NewAmmo, 0, MagazineSize);
}

void AWeaponBase::ResetForRound()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimer);
	}
	bReloading = false;
	bTriggerHeld = false;
	SetMagazineAmmo(MagazineSize);
}

void AWeaponBase::StartFire()
{
	bTriggerHeld = true;
}

void AWeaponBase::StopFire()
{
	bTriggerHeld = false;
}

void AWeaponBase::StartReload()
{
	if (bReloading || AmmoInMagazine >= MagazineSize)
	{
		return;
	}

	bReloading = true;
	UE_LOG(LogVanthex, Display, TEXT("Reload started (%.1fs)"), ReloadDuration);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ReloadTimer, this, &AWeaponBase::FinishReload, ReloadDuration, false);
	}
}

void AWeaponBase::FinishReload()
{
	bReloading = false;
	SetMagazineAmmo(MagazineSize);
	UE_LOG(LogVanthex, Display, TEXT("Reload finished (%d/%d)"), AmmoInMagazine, MagazineSize);
}

void AWeaponBase::CancelReload()
{
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(ReloadTimer); }
	bReloading = false;
}

void AWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(AWeaponBase, AmmoInMagazine);
 DOREPLIFETIME(AWeaponBase, bReloading);
}
