#include "HealthComponent.h"
#include "Net/UnrealNetwork.h"

#include "Vanthex.h"
#include "VanthexPlayerController.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
 SetIsReplicatedByDefault(true);
	Health = MaxHealth;
}

void UHealthComponent::ResetHealth()
{
 if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	Health = MaxHealth;
	bDead = false;
	LastDamage = 0.f;
	bLastHitWasHeadshot = false;
}

void UHealthComponent::ApplyDamage(const FVanthexDamageRequest& Request)
{
 if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (bDead || Request.Damage <= 0.f)
	{
		return;
	}

	Health = FMath::Max(0.f, Health - Request.Damage);
	LastDamage = Request.Damage;
 if (auto* PC=Cast<AVanthexPlayerController>(Request.InstigatorController)) PC->ClientConfirmHit(Request.bHeadshot);
	bLastHitWasHeadshot = Request.bHeadshot;

	const TCHAR* Zone = Request.bHeadshot ? TEXT("HEAD") : TEXT("BODY");
	const FString Message = FString::Printf(
		TEXT("%s  %.0f   HP %.0f"),
		Zone,
		Request.Damage,
		Health);

	UE_LOG(LogVanthex, Display, TEXT("%s hit %s: %s"),
		*GetNameSafe(Request.InstigatorActor),
		*GetNameSafe(GetOwner()),
		*Message);

	if (Health <= 0.f)
	{
		bDead = true;
		UE_LOG(LogVanthex, Display, TEXT("%s died"), *GetNameSafe(GetOwner()));
		OnDeath.Broadcast(GetOwner());
	}
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(UHealthComponent, Health);
 DOREPLIFETIME(UHealthComponent, bDead);
 DOREPLIFETIME(UHealthComponent, LastDamage);
 DOREPLIFETIME(UHealthComponent, bLastHitWasHeadshot);
}
