#include "VanthexCharacter.h"
#include "VanthexEquipmentComponent.h"
#include "Net/UnrealNetwork.h"

#include "Vanthex.h"
#include "VanthexArt.h"
#include "VanthexOperatorComponent.h"
#include "VanthexCore.h"
#include "EngineUtils.h"
#include "VanthexShiftDevice.h"
#include "VanthexGameMode.h"
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
 if (const auto* PS = GetPlayerState<AVanthexPlayerState>()) { if (CachedTeam != PS->GetTeam()) { CachedTeam = PS->GetTeam(); SetTeamAppearance(CachedTeam); } }
 if (!HasAuthority() && Health) {
  if (Health->IsDead()) GetCharacterMovement()->DisableMovement();
  else if (GetCharacterMovement()->MovementMode == MOVE_None) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
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

	UE_LOG(LogVanthex, Display, TEXT("Controls: WASD move, mouse look, Shift sprint, Ctrl crouch, Space jump, LMB fire, R reload, E pickup, G drop, Q throw, F5 restart"));
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
	ReloadAction = NewObject<UInputAction>(this, TEXT("IA_Reload"));
	RestartAction = NewObject<UInputAction>(this, TEXT("IA_Restart"));
	PickupAction = NewObject<UInputAction>(this, TEXT("IA_Pickup"));
	DropAction = NewObject<UInputAction>(this, TEXT("IA_Drop"));
	ThrowAction = NewObject<UInputAction>(this, TEXT("IA_Throw"));

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

	MapAxis(MoveAction, EKeys::D, false, false);
	MapAxis(MoveAction, EKeys::A, true, false);
	MapAxis(MoveAction, EKeys::W, false, true);
	MapAxis(MoveAction, EKeys::S, true, true);
	MapAxis(LookAction, EKeys::MouseX, false, false);
	MapAxis(LookAction, EKeys::MouseY, true, true);

	MappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	MappingContext->MapKey(SprintAction, EKeys::LeftShift);
	MappingContext->MapKey(CrouchAction, EKeys::LeftControl);
	MappingContext->MapKey(FireAction, EKeys::LeftMouseButton);
	MappingContext->MapKey(ReloadAction, EKeys::R);
	MappingContext->MapKey(RestartAction, EKeys::F5);
	MappingContext->MapKey(PickupAction, EKeys::E);
	MappingContext->MapKey(DropAction, EKeys::G);
	MappingContext->MapKey(ThrowAction, EKeys::Q);
}

void AVanthexCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
 PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AVanthexCharacter::InteractShift);
 PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &AVanthexCharacter::UseOperator);
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AVanthexCharacter::SelectRifle);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AVanthexCharacter::SelectKnife);

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
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &AVanthexCharacter::StartSprint);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &AVanthexCharacter::StopSprint);
	EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &AVanthexCharacter::StartCrouchInput);
	EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AVanthexCharacter::StopCrouchInput);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AVanthexCharacter::StartFire);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &AVanthexCharacter::StopFire);
	EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &AVanthexCharacter::StartReload);
	EnhancedInput->BindAction(RestartAction, ETriggerEvent::Started, this, &AVanthexCharacter::RequestRestart);
	EnhancedInput->BindAction(PickupAction, ETriggerEvent::Started, this, &AVanthexCharacter::TryPickupCore);
	EnhancedInput->BindAction(DropAction, ETriggerEvent::Started, this, &AVanthexCharacter::DropCarriedCore);
	EnhancedInput->BindAction(ThrowAction, ETriggerEvent::Started, this, &AVanthexCharacter::ThrowCarriedCore);
}

void AVanthexCharacter::MoveInput(const FInputActionValue& Value)
{
	const FVector2D Move = Value.Get<FVector2D>();
	DoMove(Move.X, Move.Y);
}

void AVanthexCharacter::LookInput(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>();
	DoAim(Look.X, Look.Y);
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

	AddControllerYawInput(Yaw * LookSensitivity);
	AddControllerPitchInput(Pitch * LookSensitivity);
}

void AVanthexCharacter::SetSprinting(bool bNewSprinting)
{
 if (!HasAuthority()) ServerAction(6, bNewSprinting);
	if (Health && Health->IsDead())
	{
		return;
	}

	if (bNewSprinting && bIsCrouched)
	{
		bSprintWanted = false;
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		return;
	}

	bSprintWanted = bNewSprinting;
	GetCharacterMovement()->MaxWalkSpeed = bSprintWanted ? SprintSpeed : WalkSpeed;
}

void AVanthexCharacter::SetCrouching(bool bNewCrouching)
{
 if (bNewCrouching) SetSprinting(false);
	if (Health && Health->IsDead())
	{
		return;
	}

	if (bNewCrouching)
	{
		bSprintWanted = false;
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		Crouch();
	}
	else
	{
		UnCrouch();
	}
}

void AVanthexCharacter::StartJump()
{
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
	if (Equipment && Equipment->GetSelectedSlot() == EVanthexWeaponSlot::Knife) { Equipment->Attack(); return; }
 if (!HasAuthority()) { ServerAction(0, false); return; }
	if (Weapon && CanUseRifle() && !(Health && Health->IsDead()))
	{
		Weapon->StartFire();
	}
}

void AVanthexCharacter::StopFire()
{
 if (!HasAuthority()) { ServerAction(1, false); return; }
	if (Weapon)
	{
		Weapon->StopFire();
	}
}

void AVanthexCharacter::StartReload()
{
 if (!HasAuthority()) { ServerAction(2, false); return; }
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
	if (Equipment) Equipment->ResetEquipment();
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
		Movement->MaxWalkSpeed = WalkSpeed;
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
}

void AVanthexCharacter::ServerAction_Implementation(uint8 Action, bool bEnabled) {
 switch(Action) { case 0: StartFire(); break; case 1: StopFire(); break; case 2: StartReload(); break; case 3: TryPickupCore(); break; case 4: DropCarriedCore(); break; case 5: ThrowCarriedCore(); break; case 6: SetSprinting(bEnabled); break; default: break; }
}


void AVanthexCharacter::InteractShift() { ServerUseDevice(false); }
void AVanthexCharacter::SelectRifle() { if (Equipment) Equipment->SelectRifle(); }
void AVanthexCharacter::SelectKnife() { if (Equipment) Equipment->SelectKnife(); }
bool AVanthexCharacter::CanUseRifle() const { return !Equipment || Equipment->CanUseRifle(); }
void AVanthexCharacter::UseOperator() { ServerUseDevice(true); }
void AVanthexCharacter::ServerUseDevice_Implementation(bool bRemote) {
 if (Health->IsDead() || (bRemote && GetWorld()->GetTimeSeconds()<AbilityReadyTime)) return;
 AVanthexShiftDevice* Best=nullptr; float Distance=bRemote ? 1200.f : 250.f;
 for (TActorIterator<AVanthexShiftDevice> It(GetWorld()); It; ++It) {
  const FVector Offset=It->InteractionPoint(GetActorLocation())-GetPawnViewLocation();
  const bool OnLift=It->Kind==2 && FVector::Dist2D(GetActorLocation(),It->Home)<220.f;
  if (Offset.Size()<Distance && (OnLift || FVector::DotProduct(Offset.GetSafeNormal(),GetControlRotation().Vector())>0.65f)) { Best=*It; Distance=Offset.Size(); }
 }
 if (Best && Best->Activate(this,bRemote ? 1200.f : 250.f) && bRemote) AbilityReadyTime=GetWorld()->GetTimeSeconds()+15.f;
}
