#include "VanthexCharacter.h"
#include "VanthexLocalSettings.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexGadgetComponent.h"
#include "VanthexWeaponAudioComponent.h"
#include "VanthexWeaponPresentation.h"
#include "Net/UnrealNetwork.h"

#include "Vanthex.h"
#include "VanthexArt.h"
#include "VanthexOperatorComponent.h"
#include "VanthexCore.h"
#include "EngineUtils.h"
#include "VanthexShiftDevice.h"
#include "VanthexGameMode.h"
#include "VanthexGameState.h"
#include "VanthexPlayerController.h"
#include "VanthexPlayerState.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Materials/MaterialInstanceDynamic.h"

AVanthexCharacter::AVanthexCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->MaxAcceleration = 2048.f;
	Movement->BrakingDecelerationWalking = 2048.f;
	Movement->JumpZVelocity = 520.f;
	Movement->AirControl = 0.35f;
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxWalkSpeedCrouched = 280.f;
	Movement->SetCrouchedHalfHeight(60.f);
	Movement->NavAgentProps.bCanCrouch = true;

	GetMesh()->SetVisibility(false);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0,0,-96),FRotator(0,-90,0));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	BodyMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.5f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BodyMesh->ComponentTags.Add(TEXT("Body"));
	BodyMesh->SetOwnerNoSee(true);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	HeadMesh->SetupAttachment(GetCapsuleComponent());
	HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	HeadMesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.35f));
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeadMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	HeadMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HeadMesh->ComponentTags.Add(TEXT("Head"));
	HeadMesh->SetOwnerNoSee(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMesh.Object);
		HeadMesh->SetStaticMesh(CubeMesh.Object);
	}

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(90.f);
	FirstPersonCamera->PostProcessBlendWeight = 1.f;
	FirstPersonCamera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	FirstPersonCamera->PostProcessSettings.AutoExposureMinBrightness = 1.f;
	FirstPersonCamera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	FirstPersonCamera->PostProcessSettings.AutoExposureMaxBrightness = 1.f;

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	OperatorPresentation = CreateDefaultSubobject<UVanthexOperatorComponent>(TEXT("OperatorPresentation"));
	Equipment = CreateDefaultSubobject<UVanthexEquipmentComponent>(TEXT("Equipment"));
 Gadgets = CreateDefaultSubobject<UVanthexGadgetComponent>(TEXT("Gadgets"));
	WeaponClass = AHitscanWeapon::StaticClass();
}

void AVanthexCharacter::SetHoldForward(bool bHold)
{
	bHoldForward = bHold;
	SetActorTickEnabled(true);
}

void AVanthexCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
 TickEquipmentMovementAudio(DeltaSeconds);
 TickAiming(DeltaSeconds);
 if (const auto* PS = GetPlayerState<AVanthexPlayerState>()) { if (CachedTeam != PS->GetTeam()) { CachedTeam = PS->GetTeam(); SetTeamAppearance(CachedTeam); } }
 if (!HasAuthority() && Health) {
  if (Health->IsDead()) GetCharacterMovement()->DisableMovement();
  else if (CanPerformRoundActions() && GetCharacterMovement()->MovementMode == MOVE_None) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  else if (!CanPerformRoundActions() && GetCharacterMovement()->MovementMode != MOVE_None) GetCharacterMovement()->DisableMovement();
 }

	if (bHoldForward)
	{
		DoMove(0.f, 1.f);
	}
}

void AVanthexCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (Health)
	{
		Health->OnDeath.AddUObject(this, &AVanthexCharacter::HandleDeath);
	}

	if (!WeaponClass)
	{
		WeaponClass = AHitscanWeapon::StaticClass();
	}

	if (!HasAuthority()) return;
 FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Weapon = GetWorld()->SpawnActor<AHitscanWeapon>(WeaponClass, GetActorTransform(), Params);
	if (Weapon && FirstPersonCamera)
	{
		Weapon->AttachToComponent(FirstPersonCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Weapon->SetActorRelativeLocation(FVector(30.f, 14.f, -12.f));
		Weapon->SetActorRelativeRotation(FRotator::ZeroRotator);
	}

	SyncEconomyLoadout(false);
 if(Equipment) Equipment->SelectRoundWeapon();
 UE_LOG(LogVanthex, Display, TEXT("Controls: WASD move, mouse look, Shift sprint, Ctrl crouch, mouse wheel weapons, Tab scoreboard, LMB fire, R reload, E pickup, G drop, Q throw, F5 restart"));
}

void AVanthexCharacter::TickEquipmentMovementAudio(float DeltaSeconds)
{
 if(!HasAuthority()) return;
 const auto* Movement=GetCharacterMovement();
 if(!Movement) return;
 if(EquipmentStepClock.Advance(GetActorLocation(),Movement->Velocity,DeltaSeconds,Movement->IsMovingOnGround(),
  Health && !Health->IsDead(),CanPerformRoundActions(),Movement->bJustTeleported,bIsCrouched)) {
  if(++EquipmentMovementActionId==0) ++EquipmentMovementActionId;
  UVanthexWeaponAudioComponent::DispatchEvent(this,NAME_None,EVanthexWeaponAudioEvent::EquipmentMove,EquipmentMovementActionId);
 }
}

void AVanthexCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (AVanthexGameMode* Mode = Cast<AVanthexGameMode>(World->GetAuthGameMode()))
		{
			if (AVanthexCore* Core = Mode->GetCore())
			{
				if (Core->GetCarrier() == this)
				{
					Core->DropFromDeath();
				}
			}
		}
	}
	if (Weapon)
	{
		Weapon->Destroy();
		Weapon = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AVanthexCharacter::BuildInput()
{
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));

	MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	LookAction = NewObject<UInputAction>(this, TEXT("IA_Look"));
	LookAction->ValueType = EInputActionValueType::Axis2D;
	JumpAction = NewObject<UInputAction>(this, TEXT("IA_Jump"));
	SprintAction = NewObject<UInputAction>(this, TEXT("IA_Sprint"));
	CrouchAction = NewObject<UInputAction>(this, TEXT("IA_Crouch"));
	FireAction = NewObject<UInputAction>(this, TEXT("IA_Fire"));
 AimAction = NewObject<UInputAction>(this,TEXT("IA_AimDownSights"));
	ReloadAction = NewObject<UInputAction>(this, TEXT("IA_Reload"));
	RestartAction = NewObject<UInputAction>(this, TEXT("IA_Restart"));
	PickupAction = NewObject<UInputAction>(this, TEXT("IA_Pickup"));
	DropAction = NewObject<UInputAction>(this, TEXT("IA_Drop"));
	ThrowAction = NewObject<UInputAction>(this, TEXT("IA_Throw"));
 BuildGameplayMappingKeys();
}

void AVanthexCharacter::BuildGameplayMappingKeys()
{
 const auto* Settings = UVanthexLocalSettings::Get();
 using Action = EVanthexGameplayAction;
 const auto Key = [Settings](Action Binding) { return Settings->GetGameplayKey(Binding); };
 MappingContext->UnmapAll();

	auto MapAxis = [this](UInputAction* Action, const FKey& Key, bool bNegate, bool bSwizzle)
	{
		FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(Action, Key);
		if (bNegate)
		{
			UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(MappingContext);
			Mapping.Modifiers.Add(Negate);
		}
		if (bSwizzle)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(MappingContext);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
	};

	MapAxis(MoveAction, Key(Action::Right), false, false);
	MapAxis(MoveAction, Key(Action::Left), true, false);
	MapAxis(MoveAction, Key(Action::Forward), false, true);
	MapAxis(MoveAction, Key(Action::Back), true, true);
	MapAxis(LookAction, EKeys::MouseX, false, false);
	MapAxis(LookAction, EKeys::MouseY, true, true);

	MappingContext->MapKey(JumpAction, Key(Action::Jump));
	MappingContext->MapKey(SprintAction, Key(Action::Sprint));
 MappingContext->MapKey(AimAction, Key(Action::Aim));
	MappingContext->MapKey(CrouchAction, Key(Action::Crouch));
 if (Key(Action::Crouch) == EKeys::LeftControl) MappingContext->MapKey(CrouchAction, EKeys::RightControl);
	MappingContext->MapKey(FireAction, Key(Action::Fire));
	MappingContext->MapKey(ReloadAction, Key(Action::Reload));
	MappingContext->MapKey(RestartAction, EKeys::F5);
	MappingContext->MapKey(PickupAction, Key(Action::Pickup));
	MappingContext->MapKey(DropAction, Key(Action::Drop));
	MappingContext->MapKey(ThrowAction, Key(Action::Throw));
}

void AVanthexCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
 const auto* Settings = UVanthexLocalSettings::Get();
 using Action = EVanthexGameplayAction;
 LegacyGameplayBindingIndices.Reset();
 const auto BindGameplayKey = [&](Action Binding, auto Method)
 {
  LegacyGameplayBindingIndices.Add(Binding, PlayerInputComponent->KeyBindings.Num());
  PlayerInputComponent->BindKey(Settings->GetGameplayKey(Binding), IE_Pressed, this, Method);
 };
 BindGameplayKey(Action::Interact, &AVanthexCharacter::InteractShift);
 BindGameplayKey(Action::Operator, &AVanthexCharacter::UseOperator);
 BindGameplayKey(Action::Primary, &AVanthexCharacter::SelectRifle);
 BindGameplayKey(Action::Sidearm, &AVanthexCharacter::SelectPistol);
 BindGameplayKey(Action::Knife, &AVanthexCharacter::SelectKnife);
 BindGameplayKey(Action::NextWeapon, &AVanthexCharacter::NextWeapon);
 BindGameplayKey(Action::PreviousWeapon, &AVanthexCharacter::PreviousWeapon);
 BindGameplayKey(Action::GadgetOne, &AVanthexCharacter::UseGadgetOne);
 BindGameplayKey(Action::GadgetTwo, &AVanthexCharacter::UseGadgetTwo);

	if (!MappingContext)
	{
		BuildInput();
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
			{
				Subsystem->RemoveMappingContext(MappingContext);
				Subsystem->AddMappingContext(MappingContext, 0);
			}
		}
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogVanthex, Error, TEXT("Enhanced Input component missing. Check DefaultInput.ini."));
		return;
	}

	EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AVanthexCharacter::MoveInput);
	EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AVanthexCharacter::LookInput);
	EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AVanthexCharacter::StartJump);
	EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &AVanthexCharacter::StopJump);
	EnhancedInput->BindAction(JumpAction, ETriggerEvent::Canceled, this, &AVanthexCharacter::StopJump);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &AVanthexCharacter::StartSprint);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &AVanthexCharacter::StopSprint);
	EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &AVanthexCharacter::StartCrouchInput);
	EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AVanthexCharacter::StopCrouchInput);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AVanthexCharacter::StartFire);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &AVanthexCharacter::StopFire);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Canceled, this, &AVanthexCharacter::StopFire);
 EnhancedInput->BindAction(AimAction,ETriggerEvent::Started,this,&AVanthexCharacter::StartAimingInput);
 EnhancedInput->BindAction(AimAction,ETriggerEvent::Completed,this,&AVanthexCharacter::StopAimingInput);
 EnhancedInput->BindAction(AimAction,ETriggerEvent::Canceled,this,&AVanthexCharacter::StopAimingInput);
	EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &AVanthexCharacter::StartReload);
	EnhancedInput->BindAction(RestartAction, ETriggerEvent::Started, this, &AVanthexCharacter::RequestRestart);
	EnhancedInput->BindAction(PickupAction, ETriggerEvent::Started, this, &AVanthexCharacter::TryPickupCore);
	EnhancedInput->BindAction(DropAction, ETriggerEvent::Started, this, &AVanthexCharacter::DropCarriedCore);
	EnhancedInput->BindAction(ThrowAction, ETriggerEvent::Started, this, &AVanthexCharacter::ThrowCarriedCore);
}

void AVanthexCharacter::RefreshGameplayBindings()
{
 auto* PC = Cast<APlayerController>(GetController());
 if (!PC || !PC->IsLocalController() || !MappingContext || !InputComponent) return;

 StopJumping();
 if (bIsCrouched) StopCrouchInput();
 BuildGameplayMappingKeys();
 const auto* Settings = UVanthexLocalSettings::Get();
 for (const auto& Binding : LegacyGameplayBindingIndices)
 {
  if (InputComponent->KeyBindings.IsValidIndex(Binding.Value))
   InputComponent->KeyBindings[Binding.Value].Chord.Key = Settings->GetGameplayKey(Binding.Key);
 }
 if (auto* LocalPlayer = PC->GetLocalPlayer())
 {
  if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
   Subsystem->RequestRebuildControlMappings(FModifyContextOptions(), EInputMappingRebuildType::RebuildWithFlush);
 }
}

void AVanthexCharacter::MoveInput(const FInputActionValue& Value)
{
	const FVector2D Move = Value.Get<FVector2D>();
	DoMove(Move.X, Move.Y);
}

void AVanthexCharacter::LookInput(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>();
 const float MouseSensitivity = UVanthexLocalSettings::Get()->GetMouseSensitivity(AimBlend);
	DoAim(Look.X * MouseSensitivity, Look.Y * MouseSensitivity);
}

void AVanthexCharacter::StartSprint()
{
	SetSprinting(true);
}

void AVanthexCharacter::StopSprint()
{
	SetSprinting(false);
}

void AVanthexCharacter::StartCrouchInput()
{
	SetCrouching(true);
}

void AVanthexCharacter::StopCrouchInput()
{
	SetCrouching(false);
}

void AVanthexCharacter::DoMove(float Right, float Forward)
{
 if(!CanPerformRoundActions()) return;
	if (!GetController() || (Health && Health->IsDead()))
	{
		return;
	}

	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Forward);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Right);
}

void AVanthexCharacter::DoAim(float Yaw, float Pitch)
{
	if (!GetController() || (Health && Health->IsDead()))
	{
		return;
	}

 const float AimSensitivity=FirstPersonCamera?FMath::Lerp(1.f,FirstPersonCamera->FieldOfView/90.f,AimBlend):1.f;
	AddControllerYawInput(Yaw * LookSensitivity * AimSensitivity);
	AddControllerPitchInput(Pitch * LookSensitivity * AimSensitivity);
}

bool AVanthexCharacter::CanAim() const
{
 return Controller && Weapon && VanthexWeapons::Find(Weapon->GetWeaponId()) && CanUseRifle() && !Weapon->IsReloading() && !bSprintWanted;
}
bool AVanthexCharacter::IsAiming() const
{
 return (IsLocallyControlled()&&!HasAuthority()?bPredictedAiming:bAiming) && CanAim();
}
void AVanthexCharacter::SetAiming(bool Requested)
{
 const bool Accepted=Requested&&CanAim();
 if(!Requested) bAimInputHeld=false;
 AimedWeaponId=Accepted?Weapon->GetWeaponId():NAME_None;
 if(HasAuthority()) {
  bAiming=Accepted; bPredictedAiming=Accepted; ForceNetUpdate();
  if(IsPlayerControlled()&&!IsLocallyControlled()) ClientConfirmAiming(Accepted,LastServerAimRequestId);
 } else if(IsLocallyControlled()) {
  bPredictedAiming=Accepted; ++AimRequestId; ServerSetAiming(Requested,AimRequestId);
 }
 RefreshMovementSpeed();
}
void AVanthexCharacter::ServerSetAiming_Implementation(bool Requested,uint16 RequestId)
{
 LastServerAimRequestId=RequestId; SetAiming(Requested);
}
void AVanthexCharacter::ClientConfirmAiming_Implementation(bool Accepted,uint16 RequestId)
{
 // An older press acknowledgement must not undo a newer local release.
 if(RequestId!=AimRequestId) return;
 bPredictedAiming=Accepted;
 AimedWeaponId=Accepted&&Weapon?Weapon->GetWeaponId():NAME_None;
 if(!Accepted) bAimInputHeld=false;
 RefreshMovementSpeed();
}
void AVanthexCharacter::StartAimingInput() { bAimInputHeld=true; SetAiming(true); }
void AVanthexCharacter::StopAimingInput() { SetAiming(false); }
void AVanthexCharacter::RefreshMovementSpeed()
{
 if(!GetCharacterMovement()) return;
 const float Scale=IsAiming()&&Weapon?VanthexWeaponPresentation::Aim(Weapon->GetWeaponId()).MovementScale:1.f;
 // Full speed only after the knife draw finishes; switching cannot bypass the weapon penalty.
 const bool bKnifeReady=Equipment&&Equipment->GetSelectedSlot()==EVanthexWeaponSlot::Knife&&!Equipment->IsSwitching();
 const float EquipmentScale=!Equipment||bKnifeReady?1.f:FMath::Clamp(WeaponMovementScale,.1f,1.f);
 GetCharacterMovement()->MaxWalkSpeed=(bSprintWanted?SprintSpeed:WalkSpeed)*Scale*EquipmentScale;
 GetCharacterMovement()->MaxWalkSpeedCrouched=280.f*(IsAiming()?.7f:1.f)*EquipmentScale;
}
void AVanthexCharacter::TickAiming(float Delta)
{
 const bool Pending=HasAuthority()?bAiming:bPredictedAiming;
 if(Pending&&(!CanAim()||!Weapon||AimedWeaponId!=Weapon->GetWeaponId())) SetAiming(false);
 if(IsLocallyControlled()) {
  if(const auto* PC=Cast<APlayerController>(Controller))
   if((bAimInputHeld&&!PC->IsInputKeyDown(UVanthexLocalSettings::Get()->GetGameplayKey(EVanthexGameplayAction::Aim)))||((bAiming||bPredictedAiming)&&PC->bShowMouseCursor)) SetAiming(false);
  const bool Active=IsAiming();
  if(Active&&!bAimCameraManaged&&FirstPersonCamera) { HipFieldOfView=FirstPersonCamera->FieldOfView; bAimCameraManaged=true; }
  AimBlend=FMath::FInterpTo(AimBlend,Active?1.f:0.f,Delta,16.f);
  if(AimBlend<.001f) AimBlend=0;
  if(AimBlend>.999f) AimBlend=1;
  if(bAimCameraManaged&&FirstPersonCamera) {
   const float Target=Weapon?VanthexWeaponPresentation::Aim(Weapon->GetWeaponId()).FieldOfView:HipFieldOfView;
   FirstPersonCamera->SetFieldOfView(FMath::Lerp(HipFieldOfView,Target,AimBlend));
   if(!Active&&AimBlend==0) bAimCameraManaged=false;
  }
 }
 if(HasAuthority()||IsLocallyControlled()) RefreshMovementSpeed();
}

void AVanthexCharacter::SetSprinting(bool bNewSprinting)
{
 if(bNewSprinting) SetAiming(false);
 if(bNewSprinting&&!CanPerformRoundActions()) return;
 if (!HasAuthority()) ServerAction(6, bNewSprinting);
	if (Health && Health->IsDead())
	{
		return;
	}

	if (bNewSprinting && bIsCrouched)
	{
		bSprintWanted = false;
		RefreshMovementSpeed();
		return;
	}

	bSprintWanted = bNewSprinting;
 RefreshMovementSpeed();
}

void AVanthexCharacter::SetCrouching(bool bNewCrouching)
{
 if(bNewCrouching&&!CanPerformRoundActions()) return;
 if (bNewCrouching) SetSprinting(false);
	if (Health && Health->IsDead())
	{
		return;
	}

	if (bNewCrouching)
	{
		bSprintWanted = false;
		RefreshMovementSpeed();
		Crouch();
	}
	else
	{
		UnCrouch();
	}
}

bool AVanthexCharacter::CanJumpInternal_Implementation() const
{
 return CanPerformRoundActions() && Health && !Health->IsDead() && Super::CanJumpInternal_Implementation();
}

void AVanthexCharacter::StartJump()
{
 if(!CanPerformRoundActions()) return;
	if (Health && Health->IsDead())
	{
		return;
	}
	Jump();
}

void AVanthexCharacter::StopJump()
{
	StopJumping();
}

void AVanthexCharacter::StartFire()
{
 if(!CanPerformRoundActions()) return;
	if (Equipment && Equipment->GetSelectedSlot() == EVanthexWeaponSlot::Knife) { Equipment->Attack(); return; }
 if (!HasAuthority())
 {
  if (!IsLocallyControlled()) return;
  const uint32 Burst = Weapon ? Weapon->StartPredictedFire() : 0;
  if (Burst != 0) ServerStartPredictedFire(Burst);
  else ServerAction(0, false);
  return;
 }
	if (Weapon && CanUseRifle() && !(Health && Health->IsDead()))
	{
		Weapon->StartFire();
	}
}

void AVanthexCharacter::StopFire()
{
 if (!HasAuthority())
 {
  if (Weapon) Weapon->StopPredictedFire();
  ServerAction(1, false);
  return;
 }
	if (Weapon)
	{
		Weapon->StopFire();
	}
}

void AVanthexCharacter::StartReload()
{
 SetAiming(false);
 if(!CanPerformRoundActions()) return;
 if (!HasAuthority())
 {
  if (Weapon && !Weapon->IsReloading() && Weapon->GetAmmoInMagazine() < Weapon->GetMagazineSize() && Weapon->GetReserveAmmo() > 0)
  {
   Weapon->StopPredictedFire();
  }
  ServerAction(2, false);
  return;
 }
	if (Weapon && CanUseRifle() && !(Health && Health->IsDead()))
	{
		Weapon->StartReload();
	}
}

void AVanthexCharacter::RequestRestart()
{
	if (AVanthexPlayerController* PlayerController = Cast<AVanthexPlayerController>(GetController()))
	{
		PlayerController->RestartTest();
	}
}

void AVanthexCharacter::TryPickupCore()
{
 if(!CanPerformRoundActions()) return;
 if (!HasAuthority()) { ServerAction(3, false); return; }
	if (Health && Health->IsDead())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (AVanthexGameMode* Mode = Cast<AVanthexGameMode>(World->GetAuthGameMode()))
		{
			if (AVanthexCore* Core = Mode->GetCore())
			{
				Core->TryPickup(this);
				Mode->RefreshCoreReadout();
			}
		}
	}
}

void AVanthexCharacter::DropCarriedCore()
{
 if(!CanPerformRoundActions()) return;
 if (!HasAuthority()) { ServerAction(4, false); return; }
	if (Health && Health->IsDead())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (AVanthexGameMode* Mode = Cast<AVanthexGameMode>(World->GetAuthGameMode()))
		{
			if (AVanthexCore* Core = Mode->GetCore())
			{
				if (Core->GetCarrier() == this) { Core->Drop(); }
				Mode->RefreshCoreReadout();
			}
		}
	}
}

void AVanthexCharacter::ThrowCarriedCore()
{
 if(!CanPerformRoundActions()) return;
 if (!HasAuthority()) { ServerAction(5, false); return; }
	if (Health && Health->IsDead())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (AVanthexGameMode* Mode = Cast<AVanthexGameMode>(World->GetAuthGameMode()))
		{
			if (AVanthexCore* Core = Mode->GetCore())
			{
				if (Core->GetCarrier() == this) { Core->Throw(); }
				Mode->RefreshCoreReadout();
			}
		}
	}
}

bool AVanthexCharacter::IsCarryingCore() const
{
	const UWorld* World = GetWorld();
	const AVanthexGameMode* Mode = World ? Cast<AVanthexGameMode>(World->GetAuthGameMode()) : nullptr;
	const AVanthexCore* Core = Mode ? Mode->GetCore() : nullptr;
	if (Core) return Core->GetCarrier() == this;
 for (TActorIterator<AVanthexCore> It(GetWorld()); It; ++It) if (It->GetCarrier() == this) return true;
 return false;
}

void AVanthexCharacter::PrepareForRound(const FTransform& Spawn)
{
 SetAiming(false);
 if(Gadgets)Gadgets->ResetForRound();
	SetHoldForward(false);
 AbilityReadyTime = 0.f;
	bSprintWanted = false;
	StopJumping();
	if (bIsCrouched)
	{
		UnCrouch();
	}

	if (Health)
	{
		Health->ResetHealth();
	}
	if (Weapon)
	{
		Weapon->ResetForRound();
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Walking);
		RefreshMovementSpeed();
	}

	const FVector Location = Spawn.GetLocation();
	const FRotator Rotation = Spawn.GetRotation().Rotator();
	if (!TeleportTo(Location, Rotation, false, true))
	{
		SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (AController* OwnedController = GetController())
	{
		OwnedController->SetControlRotation(FRotator(0.f, Rotation.Yaw, 0.f));
 if (auto* PC = Cast<APlayerController>(OwnedController)) PC->ClientSetRotation(FRotator(0.f, Rotation.Yaw, 0.f), true);
		if (AVanthexPlayerState* RosterState = OwnedController->GetPlayerState<AVanthexPlayerState>())
		{
			RosterState->SetAlive(true);
		}
	}
 SyncEconomyLoadout(true);
 if(Equipment) Equipment->SelectRoundWeapon();
}

void AVanthexCharacter::SetTeamAppearance(EVanthexTeam Team)
{
	if (OperatorPresentation) OperatorPresentation->SetTeamAppearance(Team);
	const bool bBravo = Team == EVanthexTeam::Bravo;
	const FLinearColor Color = bBravo ? FLinearColor(0.62f, 0.24f, 0.1f) : FLinearColor(0.16f, 0.32f, 0.62f);
	const FLinearColor Head = bBravo ? FLinearColor(0.45f, 0.2f, 0.1f) : FLinearColor(0.12f, 0.22f, 0.4f);
	UMaterialInstanceDynamic* BodyMaterial = VanthexArt::MakeSurface(this, Color, 0.48f, 0.12f);
	UMaterialInstanceDynamic* HeadMaterial = VanthexArt::MakeSurface(this, Head, 0.4f, 0.2f);
	if (BodyMesh && BodyMaterial)
	{
		BodyMesh->SetMaterial(0, BodyMaterial);
	}
	if (HeadMesh && HeadMaterial)
	{
		HeadMesh->SetMaterial(0, HeadMaterial);
	}
}

void AVanthexCharacter::HandleDeath(AActor* /*DeadActor*/)
{
 SetAiming(false);
	if (Equipment) Equipment->ResetEquipment();
	StopFire();
	if (Weapon) { Weapon->CancelReload(); }
	GetCharacterMovement()->DisableMovement();
	if (AController* OwnedController = GetController())
	{
		if (AVanthexPlayerState* RosterState = OwnedController->GetPlayerState<AVanthexPlayerState>())
		{
			RosterState->SetAlive(false);
		}
	}
	if (UWorld* World = GetWorld())
	{
		if (AVanthexGameMode* Mode = Cast<AVanthexGameMode>(World->GetAuthGameMode()))
		{
			Mode->NotifyCombatantDied(this);
		}
	}
	UE_LOG(LogVanthex, Display, TEXT("%s eliminated"), *GetName());
}

void AVanthexCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(AVanthexCharacter, Weapon);
 DOREPLIFETIME(AVanthexCharacter, AbilityReadyTime);
 DOREPLIFETIME(AVanthexCharacter,bAiming);
}

void AVanthexCharacter::ServerStartPredictedFire_Implementation(uint32 Burst)
{
 if (Burst == 0) return;
 if (Weapon && CanUseRifle())
 {
  Weapon->StartTaggedFire(Burst);
  return;
 }
 if (auto* PC = Cast<AVanthexPlayerController>(GetController()))
 {
  PC->ClientResolvePredictedShot(Burst, 0, false, false);
 }
}

void AVanthexCharacter::ServerAction_Implementation(uint8 Action, bool bEnabled) {
 switch(Action) { case 0: StartFire(); break; case 1: StopFire(); break; case 2: StartReload(); break; case 3: TryPickupCore(); break; case 4: DropCarriedCore(); break; case 5: ThrowCarriedCore(); break; case 6: SetSprinting(bEnabled); break; default: break; }
}


void AVanthexCharacter::InteractShift() { ServerUseDevice(false); }
void AVanthexCharacter::PrepareWeaponSelection()
{
 SetAiming(false);
 if (Weapon) Weapon->StopPredictedFire();
}
void AVanthexCharacter::SelectRifle() { PrepareWeaponSelection(); if (Equipment) Equipment->SelectRifle(); }
void AVanthexCharacter::SelectPistol() { PrepareWeaponSelection(); if (Equipment) Equipment->SelectPistol(); }
void AVanthexCharacter::NextWeapon() { PrepareWeaponSelection(); if (Equipment) Equipment->CycleWeapon(1); }
void AVanthexCharacter::PreviousWeapon() { PrepareWeaponSelection(); if (Equipment) Equipment->CycleWeapon(-1); }
void AVanthexCharacter::SelectKnife() { PrepareWeaponSelection(); if (Equipment) Equipment->SelectKnife(); }
bool AVanthexCharacter::CanUseRifle() const { return CanPerformRoundActions() && (!Health || !Health->IsDead()) && (!Equipment || Equipment->CanUseRifle()); }
void AVanthexCharacter::UseOperator() { ServerUseDevice(true); }
void AVanthexCharacter::ServerUseDevice_Implementation(bool bRemote) {
 if (!CanPerformRoundActions() || Health->IsDead() || (Gadgets&&Gadgets->IsJammed()) || (bRemote && GetWorld()->GetTimeSeconds()<AbilityReadyTime)) return;
 AVanthexShiftDevice* Best=nullptr; float Distance=bRemote ? 1200.f : 250.f;
 for (TActorIterator<AVanthexShiftDevice> It(GetWorld()); It; ++It) {
  const FVector Offset=It->InteractionPoint(GetActorLocation())-GetPawnViewLocation();
  const bool OnLift=It->Kind==2 && FVector::Dist2D(GetActorLocation(),It->Home)<220.f;
  if (Offset.Size()<Distance && (OnLift || FVector::DotProduct(Offset.GetSafeNormal(),GetControlRotation().Vector())>0.65f)) { Best=*It; Distance=Offset.Size(); }
 }
 if (Best && Best->Activate(this,bRemote ? 1200.f : 250.f) && bRemote) AbilityReadyTime=GetWorld()->GetTimeSeconds()+15.f;
}
bool AVanthexCharacter::CanPerformRoundActions() const
{
 if(!GetWorld()) return false;
 if(HasAuthority()) if(const auto* Mode=GetWorld()->GetAuthGameMode<AVanthexGameMode>()) return Mode->IsRoundActive();
 const auto* State=GetWorld()->GetGameState<AVanthexGameState>();
 return State&&State->GetPhase()==EVanthexRoundPhase::RoundActive;
}
void AVanthexCharacter::SyncEconomyLoadout(bool Refill)
{
 if(!HasAuthority()||!Equipment||!Weapon) return;
 const auto* Mode=GetWorld()->GetAuthGameMode<AVanthexGameMode>();
 if(Mode&&Mode->UsesLegacyCombatFixture()) { Equipment->SyncLoadout(TEXT("LegacyRifle"),TEXT("Pistol"),Refill); return; }
 const auto* PS=GetPlayerState<AVanthexPlayerState>();
 Equipment->SyncLoadout(PS?PS->GetEconomy().PrimaryWeapon:NAME_None,PS?PS->GetEconomy().Sidearm:FName(TEXT("Pistol")),Refill);
}
void AVanthexCharacter::ApplyWeaponRecoil(FVector2D Kick)
{
 if(!HasAuthority()||!GetController()||Kick.IsNearlyZero()) return;
 auto Rotation=GetController()->GetControlRotation();
 Rotation.Pitch=FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch)+Kick.X,-89.f,89.f);
 Rotation.Yaw+=Kick.Y; GetController()->SetControlRotation(Rotation);
 if(auto* PC=Cast<APlayerController>(GetController())) if(!PC->IsLocalController()) ClientWeaponRecoil(Kick);
}
void AVanthexCharacter::ClientWeaponRecoil_Implementation(FVector2D Kick)
{
 if(!IsLocallyControlled()||!GetController()) return;
 auto Rotation=GetController()->GetControlRotation();
 Rotation.Pitch=FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch)+Kick.X,-89.f,89.f);
 Rotation.Yaw+=Kick.Y; GetController()->SetControlRotation(Rotation);
}

void AVanthexCharacter::UseGadgetOne(){if(Gadgets)Gadgets->UseSlot(0);}
void AVanthexCharacter::UseGadgetTwo(){if(Gadgets)Gadgets->UseSlot(1);}
FVector AVanthexCharacter::GetPawnViewLocation()const
{
 return FirstPersonCamera?FirstPersonCamera->GetComponentLocation():Super::GetPawnViewLocation();
}
void AVanthexCharacter::OnStartCrouch(float HalfHeightAdjust,float ScaledHalfHeightAdjust)
{
 Super::OnStartCrouch(HalfHeightAdjust,ScaledHalfHeightAdjust);
 if(FirstPersonCamera)FirstPersonCamera->SetRelativeLocation(FVector(0,0,42));
 // Fallback hit geometry follows the shortened capsule. ARC adds its own bone hit zones.
 if(BodyMesh){BodyMesh->SetRelativeLocation(FVector(0,0,-8));BodyMesh->SetRelativeScale3D(FVector(.7,.7,.85));}
 if(HeadMesh)HeadMesh->SetRelativeLocation(FVector(0,0,42));
}
void AVanthexCharacter::OnEndCrouch(float HalfHeightAdjust,float ScaledHalfHeightAdjust)
{
 Super::OnEndCrouch(HalfHeightAdjust,ScaledHalfHeightAdjust);
 if(FirstPersonCamera)FirstPersonCamera->SetRelativeLocation(FVector(0,0,64));
 if(BodyMesh){BodyMesh->SetRelativeLocation(FVector::ZeroVector);BodyMesh->SetRelativeScale3D(FVector(.7,.7,1.5));}
 if(HeadMesh)HeadMesh->SetRelativeLocation(FVector(0,0,70));
}
