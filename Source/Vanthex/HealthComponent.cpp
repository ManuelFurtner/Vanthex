#include "HealthComponent.h"
#include "Net/UnrealNetwork.h"

#include "Vanthex.h"
#include "VanthexPlayerController.h"
#include "VanthexCharacter.h"
#include "VanthexPlayerState.h"
#include "VanthexGameMode.h"
#include "VanthexGameState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"

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
	OnRep_Dead();
	GetOwner()->ForceNetUpdate();
	LastDamage = 0.f;
	LastAppliedHPDamage = 0.f;
	bLastHitWasHeadshot = false;
	if (auto* Character = Cast<AVanthexCharacter>(GetOwner()))
	{
		if (auto* Mode = GetWorld()->GetAuthGameMode<AVanthexGameMode>()) Mode->ClearDamageHistory(Character);
	}
}

void UHealthComponent::ApplyDamage(const FVanthexDamageRequest& Request)
{
 if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (bDead || !FMath::IsFinite(Request.Damage) || Request.Damage <= 0.f)
	{
		return;
	}

	AVanthexCharacter* Character = Cast<AVanthexCharacter>(GetOwner());
	float HPDamage = Request.Damage;
	if (Character && (Request.DamageKind == EVanthexDamageKind::Bullet || Request.DamageKind == EVanthexDamageKind::Explosion))
	{
		if (auto* State = Character->GetPlayerState<AVanthexPlayerState>())
			HPDamage = State->ApplyArmorDamage(Request.Damage, Request.bHeadshot, false);
	}
	const float PreviousHealth = Health;
	Health = FMath::Max(0.f, Health - HPDamage);
	LastDamage = Request.Damage;
	LastAppliedHPDamage = PreviousHealth - Health;
 if (!Request.bDeferHitMarker && LastAppliedHPDamage > 0.f)
 {
  if (auto* PC = Cast<AVanthexPlayerController>(Request.InstigatorController)) PC->ClientConfirmHit(Request.bHeadshot);
 }
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

	// Credit accounting must precede OnDeath: its listeners may finish the round.
	if (Character)
	{
		if (auto* Mode = GetWorld()->GetAuthGameMode<AVanthexGameMode>())
			Mode->RecordDamage(Character, Request, LastAppliedHPDamage, Health <= 0.f);
	}

	if (Health <= 0.f)
	{
		bDead = true;
  if(Character) if(auto* Match=GetWorld()->GetGameState<AVanthexGameState>()) {
   const auto* Killer=Request.InstigatorController?Request.InstigatorController->GetPlayerState<AVanthexPlayerState>():nullptr;
   if(!Killer) if(const auto* Attacker=Cast<AVanthexCharacter>(Request.InstigatorActor)) Killer=Attacker->GetPlayerState<AVanthexPlayerState>();
   Match->PublishElimination(Killer,Character->GetPlayerState<AVanthexPlayerState>());
  }
		// Disable the whole character immediately; corpses must never absorb the
		// next bullet while the cosmetic collapse continues to animate.
		OnRep_Dead();
		GetOwner()->ForceNetUpdate();
		UE_LOG(LogVanthex, Display, TEXT("%s died"), *GetNameSafe(GetOwner()));
		OnDeath.Broadcast(GetOwner());
	}
}

void UHealthComponent::OnRep_Dead()
{
	// Leave non-character targets and their individual collision policies alone.
	if (auto* Character = Cast<AVanthexCharacter>(GetOwner()))
	{
		if (auto* PC = Cast<AVanthexPlayerController>(Character->GetController()); PC && PC->IsLocalController())
		{
			PC->ResetDamageFeedback();
		}
		if (!bDead && !Character->GetActorEnableCollision())
		{
			// Revive can occur after the mesh tick. Rebuild the living pose before
			// exposing its bodies, so same-frame shots cannot see the death pose.
			auto* Mesh = Character->GetMesh();
			if (Mesh && Mesh->GetAnimInstance() && Mesh->GetPhysicsAsset())
			{
				Mesh->HandleExistingParallelEvaluationTask(true, true);
				Mesh->TickAnimation(0.f, false);
				Mesh->RefreshBoneTransforms();
			}
		}
		Character->SetActorEnableCollision(!bDead);
	}
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(UHealthComponent, Health);
 DOREPLIFETIME(UHealthComponent, bDead);
 DOREPLIFETIME(UHealthComponent, LastDamage);
 DOREPLIFETIME(UHealthComponent, LastAppliedHPDamage);
 DOREPLIFETIME(UHealthComponent, bLastHitWasHeadshot);
}
