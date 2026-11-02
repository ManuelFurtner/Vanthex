#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "VanthexTypes.h"
#include "VanthexCharacter.generated.h"

class UCameraComponent;
class UHealthComponent;
class UInputAction;
class UInputMappingContext;
class UStaticMeshComponent;
class AHitscanWeapon;
class AVanthexCore;
class UVanthexEquipmentComponent;

UCLASS()
class AVanthexCharacter : public ACharacter
{
	GENERATED_BODY()
public:
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	AVanthexCharacter();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(Server, Reliable) void ServerAction(uint8 Action, bool bEnabled);
 void DoMove(float Right, float Forward);
	void DoAim(float Yaw, float Pitch);
	void SetSprinting(bool bNewSprinting);
	void SetCrouching(bool bNewCrouching);
	void SetHoldForward(bool bHold);
	void RequestRestart();
	void PrepareForRound(const FTransform& Spawn);
	void SetTeamAppearance(EVanthexTeam Team);
	void TryPickupCore();
	void DropCarriedCore();
	void ThrowCarriedCore();
	bool IsCarryingCore() const;
 void InteractShift();
 void UseOperator();
 UFUNCTION(Server, Reliable) void ServerUseDevice(bool bRemote);
 UPROPERTY(Replicated) float AbilityReadyTime = 0.f;

	UHealthComponent* GetHealthComponent() const { return Health; }
	AHitscanWeapon* GetWeapon() const { return Weapon; }
	UVanthexEquipmentComponent* GetEquipment() const { return Equipment; }
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	bool CanUseRifle() const;
	bool IsSprintWanted() const { return bSprintWanted; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	void MoveInput(const FInputActionValue& Value);
	void LookInput(const FInputActionValue& Value);
	void StartJump();
	void StopJump();
	void StartSprint();
	void StopSprint();
	void StartCrouchInput();
	void StopCrouchInput();
	void StartFire();
	void StopFire();
	void StartReload();
	void SelectRifle();
	void SelectKnife();
	void HandleDeath(AActor* DeadActor);
	void BuildInput();

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, Category = "Health")
	TObjectPtr<UHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<UVanthexEquipmentComponent> Equipment;

	UPROPERTY(VisibleAnywhere, Category = "Presentation")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Presentation")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AHitscanWeapon> WeaponClass;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Weapon")
	TObjectPtr<AHitscanWeapon> Weapon;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY()
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY()
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY()
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY()
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY()
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY()
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY()
	TObjectPtr<UInputAction> RestartAction;

	UPROPERTY()
	TObjectPtr<UInputAction> PickupAction;

	UPROPERTY()
	TObjectPtr<UInputAction> DropAction;

	UPROPERTY()
	TObjectPtr<UInputAction> ThrowAction;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float WalkSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SprintSpeed = 900.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float LookSensitivity = 1.f;

	bool bSprintWanted = false;
	bool bHoldForward = false;
 EVanthexTeam CachedTeam = EVanthexTeam::None;
};
