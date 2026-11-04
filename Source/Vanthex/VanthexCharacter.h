#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "VanthexTypes.h"
#include "VanthexWeaponAudioMovement.h"
#include "VanthexCharacter.generated.h"

class UCameraComponent;
class UHealthComponent;
class UInputAction;
class UInputMappingContext;
class UStaticMeshComponent;
class AHitscanWeapon;
class AVanthexCore;
class UVanthexOperatorComponent;
class UVanthexEquipmentComponent;
class UVanthexGadgetComponent;

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
 /** Owner request; gameplay ADS is accepted and replicated by the server. */
 void SetAiming(bool bRequested);
 bool CanAim() const;
 bool IsAiming() const;
 bool IsAuthoritativelyAiming() const { return bAiming && CanAim(); }
 float GetAimBlend() const { return AimBlend; }
 UFUNCTION(Server, Reliable) void ServerSetAiming(bool bRequested,uint16 RequestId);
 UFUNCTION(Client, Reliable) void ClientConfirmAiming(bool bAccepted,uint16 RequestId);
	void SetCrouching(bool bNewCrouching);
	void SetHoldForward(bool bHold);
	void RequestRestart();
	void PrepareForRound(const FTransform& Spawn);
 void SyncEconomyLoadout(bool bRefillAmmo);
 bool CanPerformRoundActions() const;
 void ApplyWeaponRecoil(FVector2D Kick);
 UFUNCTION(Client, Unreliable) void ClientWeaponRecoil(FVector2D Kick);
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
 UVanthexGadgetComponent* GetGadgets() const { return Gadgets; }
 virtual FVector GetPawnViewLocation() const override;
 virtual void OnStartCrouch(float HalfHeightAdjust,float ScaledHalfHeightAdjust) override;
 virtual void OnEndCrouch(float HalfHeightAdjust,float ScaledHalfHeightAdjust) override;
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	bool CanUseRifle() const;
	bool IsSprintWanted() const { return bSprintWanted; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
 void TickEquipmentMovementAudio(float DeltaSeconds);
 void TickAiming(float DeltaSeconds);
 void RefreshMovementSpeed();
 void StartAimingInput();
 void StopAimingInput();

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
 void SelectPistol();
	void SelectKnife();
 void UseGadgetOne();
 void UseGadgetTwo();
	void HandleDeath(AActor* DeadActor);
	void BuildInput();

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, Category = "Health")
	TObjectPtr<UHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<UVanthexEquipmentComponent> Equipment;
 UPROPERTY(VisibleAnywhere,Category="Gadgets") TObjectPtr<UVanthexGadgetComponent> Gadgets;

	UPROPERTY(VisibleAnywhere, Category = "Presentation")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Presentation")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, Category = "Operator")
	TObjectPtr<UVanthexOperatorComponent> OperatorPresentation;

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
 UPROPERTY() TObjectPtr<UInputAction> AimAction;

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
 UPROPERTY(Replicated) bool bAiming=false;
 bool bPredictedAiming=false,bAimInputHeld=false,bAimCameraManaged=false;
 float AimBlend=0.f,HipFieldOfView=90.f;
 uint16 AimRequestId=0,LastServerAimRequestId=0;
 FName AimedWeaponId;
	bool bHoldForward = false;
 EVanthexTeam CachedTeam = EVanthexTeam::None;
 FVanthexEquipmentStepClock EquipmentStepClock;
 uint32 EquipmentMovementActionId=0;
};
