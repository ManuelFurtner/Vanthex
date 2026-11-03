#include "VanthexGameMode.h"
#include "VanthexCharacter.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexOperatorComponent.h"
#include "VanthexDamage.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

// Runs after the original M1-M3 checks in -VanthexSelfTest; live bots are already disabled.
void AVanthexGameMode::AdvanceEquipmentSelfTest()
{
	auto* Human = GetTestCharacter();
	auto* Enemy = FindCombatant(EVanthexTeam::Bravo, 0);
	auto* Friend = FindCombatant(EVanthexTeam::Alpha, 1);
	if (++SelfTestTicks > 300 || !Human || !Enemy || !Friend || !Human->GetEquipment())
	{
		SelfTestExpect(false, TEXT("EQUIPMENT_FIXTURE"), TEXT("missing actors or timeout"));
		SelfTestStage = EVanthexSelfTestStage::Finish;
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	auto* Equipment = Human->GetEquipment();
	static TWeakObjectPtr<UWorld> PresentationWorld;
	static bool NoDoubleWorldWeapon = true;
	static int32 PresentationSamples = 0;
	if(PresentationWorld.Get()!=GetWorld() || EquipmentTestStep==0) {
		PresentationWorld=GetWorld(); NoDoubleWorldWeapon=true; PresentationSamples=0;
	}
	auto* Operator=Human->FindComponentByClass<UVanthexOperatorComponent>();
	auto* WorldRifle=Operator?Operator->GetWorldRifle():nullptr;
	auto* WorldKnife=Equipment->GetWorldKnifeRoot();
	if(EquipmentTestStep>0 && WorldRifle && WorldKnife) {
		NoDoubleWorldWeapon &= !(WorldRifle->IsVisible() && WorldKnife->IsVisible());
		++PresentationSamples;
	}
	if (Now < EquipmentTestDeadline) return;
	auto* Rifle = Human->GetWeapon();
	auto Expect = [&](bool Value, const TCHAR* Name) { SelfTestExpect(Value, Name, TEXT("equipment integration")); };
	auto Next = [&](float Wait) { ++EquipmentTestStep; EquipmentTestDeadline = Now + Wait; };
	auto Move = [](AVanthexCharacter* Pawn, FVector Position)
	{
		Pawn->GetCharacterMovement()->StopMovementImmediately();
		Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		Pawn->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
	};
	switch (EquipmentTestStep)
	{
	case 0:
		GetWorldTimerManager().ClearTimer(FlowTimer);
		Phase = EVanthexRoundPhase::RoundActive;
		for (int32 Index = 0; Index < Combatants.Num(); ++Index)
		{
			Combatants[Index]->GetHealthComponent()->ResetHealth();
			Move(Combatants[Index], FVector(1000 + Index * 200, 1000, 1000));
		}
		Move(Human, FVector(0,0,1000)); Move(Enemy, FVector(120,0,1000));
		Human->GetController()->SetControlRotation(FRotator::ZeroRotator);
		Equipment->ResetEquipment();
		Rifle->StopFire(); Rifle->CancelReload();
		Rifle->SetMagazineAmmo(5); Rifle->StartReload();
		Equipment->SelectKnife();
		Expect(Equipment->IsSwitching() && !Equipment->CanUseRifle() && !Rifle->IsReloading(), TEXT("SWITCH_CANCELS_RELOAD"));
		Equipment->SelectRifle();
		Expect(Equipment->GetSelectedSlot() == EVanthexWeaponSlot::Knife, TEXT("SWITCH_SPAM_REJECTED"));
		Rifle->FireShot();
		Expect(Rifle->GetAmmoInMagazine() == 5, TEXT("SWITCH_BLOCKS_SHOT"));
		Next(Equipment->SwitchDuration + 0.1f); return;
	case 1:
		Expect(!Equipment->IsSwitching(), TEXT("KNIFE_DRAW_COMPLETES"));
		Expect(WorldRifle && WorldKnife && !WorldRifle->IsVisible() && WorldKnife->IsVisible(), TEXT("ARC_WORLD_KNIFE_REPLACES_RIFLE"));
		Rifle->StartReload(); Rifle->StartFire();
		Expect(!Rifle->IsReloading() && !Rifle->IsTriggerHeld() && Rifle->GetAmmoInMagazine() == 5, TEXT("HOLSTERED_RIFLE_DISABLED"));
		Equipment->Attack(); Equipment->Attack();
		Expect(Enemy->GetHealthComponent()->GetHealth() == 100.f, TEXT("KNIFE_WINDUP_NO_INSTANT_DAMAGE"));
		Next(Equipment->StrikeDelay + 0.06f); return;
	case 2:
		Expect(Enemy->GetHealthComponent()->GetHealth() == 50.f, TEXT("KNIFE_SINGLE_STRIKE"));
		Equipment->SelectRifle(); Equipment->Attack();
		Expect(Equipment->GetSelectedSlot() == EVanthexWeaponSlot::Knife, TEXT("SWING_CANNOT_CANCEL_RECOVERY"));
		Next(Equipment->SwingDuration); return;
	case 3:
		Expect(Enemy->GetHealthComponent()->GetHealth() == 50.f, TEXT("KNIFE_SPAM_COOLDOWN"));
		Move(Enemy, FVector(500,0,1000)); Move(Friend, FVector(120,0,1000));
		Equipment->Attack(); Next(Equipment->SwingDuration + 0.1f); return;
	case 4:
		Expect(Friend->GetHealthComponent()->GetHealth() == 100.f, TEXT("KNIFE_FRIENDLY_FIRE_OFF"));
		Move(Friend, FVector(1000,1000,1000)); Move(Enemy, FVector(120,0,1000));
		EquipmentTestWall = GetWorld()->SpawnActor<AActor>();
		{
			auto* Box = NewObject<UBoxComponent>(EquipmentTestWall);
			EquipmentTestWall->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(5,100,100));
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent();
			EquipmentTestWall->SetActorLocation(FVector(60,0,1064));
		}
		Equipment->Attack(); Next(Equipment->SwingDuration + 0.1f); return;
	case 5:
		Expect(Enemy->GetHealthComponent()->GetHealth() == 50.f, TEXT("KNIFE_WALL_BLOCKS_DAMAGE"));
		EquipmentTestWall->Destroy(); EquipmentTestWall = nullptr;
		Move(Enemy, FVector(500,0,1000));
		Equipment->Attack(); Next(Equipment->SwingDuration + 0.1f); return;
	case 6:
		Expect(Enemy->GetHealthComponent()->GetHealth() == 50.f, TEXT("KNIFE_RANGE_LIMIT"));
		Move(Enemy, FVector(120,0,1000)); Equipment->Attack();
		{
			FVanthexDamageRequest Death; Death.Damage = 100.f;
			Human->GetHealthComponent()->ApplyDamage(Death);
		}
		Next(Equipment->SwingDuration + 0.1f); return;
	case 7:
		Expect(Enemy->GetHealthComponent()->GetHealth() == 50.f, TEXT("DEATH_CANCELS_PENDING_STRIKE"));
		Expect(WorldRifle && WorldKnife && !WorldRifle->IsVisible() && !WorldKnife->IsVisible(), TEXT("DEATH_HIDES_WORLD_WEAPONS"));
		Human->PrepareForRound(FTransform(FVector(0,0,1000)));
		Expect(Equipment->CanUseRifle() && !Equipment->IsSwinging() && !Equipment->IsSwitching(), TEXT("ROUND_RESETS_EQUIPMENT"));
		Phase = EVanthexRoundPhase::RoundEnding;
		Equipment->SelectKnife();
		Expect(Equipment->CanUseRifle(), TEXT("ROUND_END_REJECTS_SELECTION"));
		Phase = EVanthexRoundPhase::RoundActive;
		Equipment->SelectKnife(); Next(Equipment->SwitchDuration + 0.1f); return;
	case 8:
		Equipment->SelectRifle();
		Expect(Equipment->IsSwitching() && !Equipment->CanUseRifle(), TEXT("RIFLE_DRAW_LOCKOUT"));
		Next(Equipment->SwitchDuration + 0.1f); return;
	case 9:
		Expect(Equipment->CanUseRifle(), TEXT("RIFLE_DRAW_COMPLETES"));
		Expect(WorldRifle && WorldKnife && WorldRifle->IsVisible() && !WorldKnife->IsVisible(), TEXT("ARC_WORLD_RIFLE_REPLACES_KNIFE"));
		Expect(NoDoubleWorldWeapon && PresentationSamples>10, TEXT("WORLD_WEAPONS_NEVER_OVERLAP_DURING_SWITCH"));
		ResetCombatants();
		SelfTestStage = EVanthexSelfTestStage::Finish;
		return;
	}
}
