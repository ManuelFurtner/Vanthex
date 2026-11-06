#include "HitscanWeapon.h"
#include "VanthexCharacter.h"
#include "VanthexPlayerController.h"
#include "VanthexPlayerState.h"
#include "VanthexTestTarget.h"
#include "VanthexWeaponAudioComponent.h"
#include "VanthexWeaponPresentation.h"

#include "Vanthex.h"
#include "VanthexArt.h"
#include "VanthexDamage.h"
#include "VanthexBulletAudioComponent.h"
#include "VanthexGameMode.h"
#include "VanthexOperatorComponent.h"
#include "HealthComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Math/RotationMatrix.h"
#include "Engine/Engine.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"

AHitscanWeapon::AHitscanWeapon()
{
	ViewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ViewMesh"));
	PrimaryActorTick.bCanEverTick = true;
	ViewMesh->SetupAttachment(WeaponRoot);
	ViewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ViewMesh->SetRelativeScale3D(FVector(0.25f, 0.06f, 0.08f));
	ViewMesh->SetRelativeLocation(FVector(25.f, 0.f, 0.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ViewMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AHitscanWeapon::BeginPlay()
{
 Super::BeginPlay();
 RiflePivot=NewObject<USceneComponent>(this,TEXT("RiflePivot")); RiflePivot->SetupAttachment(WeaponRoot);
 RiflePivot->SetRelativeLocation(FVector(28,8,-6)); RiflePivot->RegisterComponent();
 if(ViewMesh) ViewMesh->SetVisibility(false);
 MuzzleFlash=VanthexArt::AddMesh(this,RiflePivot,VanthexArt::LoadSphere(),FVector::ZeroVector,FRotator::ZeroRotator,
  FVector(.07f,.04f,.04f),VanthexArt::MakeEnergy(this,FLinearColor(1,.42f,.08f),3.2f),false);
 if(MuzzleFlash) { MuzzleFlash->SetOnlyOwnerSee(true); MuzzleFlash->SetVisibility(false); }
 OnWeaponProfileChanged();
}
void AHitscanWeapon::OnWeaponProfileChanged()
{
 StopPredictedFire();
 if(GetWeaponId()==TEXT("Knife")) return;
 const auto& P=GetProfile(); TraceRange=P.TraceRange; bSupersonicBullet=P.bSupersonic;
 BurstIndex=0; LastShotTime=-100.f;
 if(!RiflePivot) return;
 FVector Muzzle;
 VanthexWeapons::BuildVisual(this,RiflePivot,GetWeaponId(),true,FirearmParts,Muzzle);
 if(MuzzleFlash) MuzzleFlash->SetRelativeLocation(Muzzle);
}

void AHitscanWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
 if(PredictionBurst != 0 && (!PredictionOwner.IsValid() || GetPredictionController() != PredictionOwner.Get()
   || PredictionWeaponId != GetWeaponId()))
 {
  StopPredictedFire();
 }
 VanthexWeaponPresentation::Animate(FirearmParts,GetWeaponId(),GetReloadProgress(),FMath::Clamp(MuzzleTime/.035f,0.f,1.f));
	Recoil = FMath::FInterpTo(Recoil, 0.f, DeltaSeconds, 14.f);
	if (RiflePivot)
	{
		float Sprint = 0.f;
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			Sprint = OwnerPawn->GetVelocity().Size2D() > 750.f ? 1.f : 0.f;
		}
		const float Kick = Sprint * 3.5f;
  const auto* Character=Cast<AVanthexCharacter>(GetOwner());
  const float Aim=Character?Character->GetAimBlend():0.f;
  const auto Sight=VanthexWeaponPresentation::Aim(GetWeaponId());
  const FVector Hip(15.f-Recoil*1.4f+Kick,0.f,2.f-Kick);
  const FVector Aimed=Sight.CameraRelativeOrigin-FVector(30,14,-12)-FVector(Recoil*.24f,0,0);
  RiflePivot->SetRelativeLocation(FMath::Lerp(Hip,Aimed,Aim));
  const float Progress=GetReloadProgress();
  const float ReloadPose=Progress>=0?FMath::Sin(Progress*PI):0.f;
  const FRotator HipRotation(Recoil*.35f-8.f*ReloadPose,-8.f*ReloadPose,Sprint*-2.f-24.f*ReloadPose);
  RiflePivot->SetRelativeRotation(FMath::Lerp(HipRotation,Sight.Rotation+FRotator(Recoil*.05f,0,0),Aim));
  const bool ScopeView=Sight.Sight==EVanthexSight::Scope&&Aim>.85f;
  for(const auto& Part:FirearmParts) if(Part) Part->SetVisibility(!ScopeView);
	}
	if (MuzzleFlash)
	{
		MuzzleTime = FMath::Max(0.f, MuzzleTime - DeltaSeconds);
  const auto* Character=Cast<AVanthexCharacter>(GetOwner());
  const bool ScopeView=Character&&Character->GetAimBlend()>.85f&&VanthexWeaponPresentation::Aim(GetWeaponId()).Sight==EVanthexSight::Scope;
		MuzzleFlash->SetVisibility(MuzzleTime > 0.f&&!ScopeView);
	}
}

void AHitscanWeapon::PresentImpact(const FHitResult& Hit)
{
	UWorld* World = GetWorld();
	if (!World || !Hit.bBlockingHit)
	{
		return;
	}

	const bool bMetal = Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(TEXT("Metal"));
	const FLinearColor DecalColor = bMetal ? FLinearColor(0.12f, 0.11f, 0.1f) : FLinearColor(0.32f, 0.3f, 0.26f);
	const FLinearColor SparkColor = bMetal ? FLinearColor(1.f, 0.55f, 0.16f) : FLinearColor(0.42f, 0.39f, 0.34f);
	if (Hit.GetActor() && Hit.GetComponent() && !Hit.GetActor()->FindComponentByClass<UHealthComponent>())
	{
		UDecalComponent* Decal = NewObject<UDecalComponent>(Hit.GetActor());
		Decal->SetupAttachment(Hit.GetComponent());
		Decal->SetWorldLocationAndRotation(Hit.ImpactPoint + Hit.ImpactNormal, FRotationMatrix::MakeFromX(-Hit.ImpactNormal).Rotator());
		Decal->DecalSize = FVector(6.f, 7.f, 7.f);
		Decal->RegisterComponent();
		if (UMaterialInterface* DecalMaterial = VanthexArt::GetDecalMaterial())
		{
			if (UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(DecalMaterial, Hit.GetActor()))
			{
				Instance->SetVectorParameterValue(TEXT("Color"), DecalColor);
				Instance->SetScalarParameterValue(TEXT("Opacity"), bMetal ? 0.8f : 0.55f);
				Instance->SetScalarParameterValue(TEXT("Stripe"), 0.f);
				Decal->SetDecalMaterial(Instance);
			}
		}
		Decal->SetFadeOut(1.2f, 2.f, false);
	}

	AActor* Spark = World->SpawnActor<AActor>(Hit.ImpactPoint, FRotator::ZeroRotator);
	if (!Spark)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Spark);
	Spark->SetRootComponent(Root);
	Root->RegisterComponent();
	UStaticMesh* Sphere = VanthexArt::LoadSphere();
	const int32 SpeckCount = bMetal ? 3 : 2;
	for (int32 Index = 0; Index < SpeckCount; ++Index)
	{
		const FVector Offset = Hit.ImpactNormal * (1.5f + Index * 2.f) + FVector(Index == 1 ? 2.f : -1.f, Index == 2 ? 2.f : 0.f, 1.f);
		UStaticMeshComponent* Speck = VanthexArt::AddMesh(Spark, Root, Sphere, Hit.ImpactPoint + Offset, FRotator::ZeroRotator, FVector(bMetal ? 0.025f : 0.055f), VanthexArt::MakeEnergy(Spark, SparkColor, bMetal ? 3.2f : 0.25f), false);
		if (Speck)
		{
			Speck->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	Spark->SetLifeSpan(bMetal ? 0.08f : 0.16f);
}

void AHitscanWeapon::PresentShotFeedback(const FRotator& ViewRotation)
{
	UWorld* World = GetWorld();
	if (!World || !MuzzleFlash)
	{
		return;
	}

	const auto* OwnerPawn=Cast<APawn>(GetOwner());
	auto* Operator=OwnerPawn?OwnerPawn->FindComponentByClass<UVanthexOperatorComponent>():nullptr;
	if(Operator) Operator->PresentShot();
	const bool FirstPerson=OwnerPawn&&OwnerPawn->IsPlayerControlled()&&OwnerPawn->IsLocallyControlled();
	const FVector Muzzle=Operator&&!FirstPerson?Operator->GetMuzzleLocation():MuzzleFlash->GetComponentLocation();
	MuzzleFlash->SetVisibility(true);
	const FVector Right = ViewRotation.RotateVector(FVector(0.f, 1.f, 0.f));
	const FVector Up = FVector::UpVector;

	if (AActor* Smoke = World->SpawnActor<AActor>(Muzzle, ViewRotation))
	{
		USceneComponent* Root = NewObject<USceneComponent>(Smoke);
		Smoke->SetRootComponent(Root);
		Root->RegisterComponent();
		if (UStaticMeshComponent* Puff = VanthexArt::AddMesh(Smoke, Root, VanthexArt::LoadSphere(), Muzzle + ViewRotation.Vector() * 14.f, ViewRotation, FVector(0.04f, 0.035f, 0.035f), VanthexArt::MakeEnergy(Smoke, FLinearColor(0.22f, 0.21f, 0.19f), 0.18f), false))
		{
			Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Smoke->SetLifeSpan(0.18f);
	}

	if (AActor* Shell = World->SpawnActor<AActor>(Muzzle, ViewRotation))
	{
		UStaticMeshComponent* Casing = NewObject<UStaticMeshComponent>(Shell);
		Casing->SetStaticMesh(VanthexArt::LoadCylinder());
		Casing->SetRelativeScale3D(FVector(0.012f, 0.012f, 0.035f));
		Casing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Casing->SetCastShadow(false);
		Shell->SetRootComponent(Casing);
		Casing->RegisterComponent();
		if (UMaterialInstanceDynamic* Brass = VanthexArt::MakeSurface(Shell, FLinearColor(0.55f, 0.4f, 0.16f), 0.28f, 0.9f, FLinearColor::Black, 0.f, EVanthexSurface::BareSteel))
		{
			Casing->SetMaterial(0, Brass);
		}
		UProjectileMovementComponent* Move = NewObject<UProjectileMovementComponent>(Shell);
		Move->UpdatedComponent = Casing;
		Move->bRotationFollowsVelocity = true;
		Move->ProjectileGravityScale = 1.4f;
		Move->MaxSpeed = 900.f;
		Move->Velocity = Right * 260.f + Up * 140.f - ViewRotation.Vector() * 30.f;
		Move->RegisterComponent();
		Shell->SetLifeSpan(0.55f);
	}
}

void AHitscanWeapon::ResetForRound()
{
 StopPredictedFire();
 NextPredictionFireTime = 0.f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimer);
	}
	NextFireTime = 0.f;
	Super::ResetForRound();
}

void AHitscanWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
 StopPredictedFire();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AHitscanWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME_CONDITION(AHitscanWeapon, ShotSequence, COND_OwnerOnly);
}

AVanthexPlayerController* AHitscanWeapon::GetPredictionController() const
{
 if(HasAuthority() || bReloading || GetWeaponId() == TEXT("Knife")) return nullptr;

 auto* Character = Cast<AVanthexCharacter>(GetOwner());
 if(!Character || Character->GetWeapon() != this || !Character->CanUseRifle()) return nullptr;

 auto* PC = Cast<AVanthexPlayerController>(Character->GetController());
 return PC && PC->GetPawn() == Character && PC->CanPredictDamageFeedback() ? PC : nullptr;
}

uint32 AHitscanWeapon::StartPredictedFire()
{
 auto* PC = GetPredictionController();
 auto* World = GetWorld();
 if(!PC || !World || PredictionBurst != 0 || AmmoInMagazine <= 0) return 0;

 const float Delay = NextPredictionFireTime - World->GetTimeSeconds();
 if(Delay > KINDA_SMALL_NUMBER && !IsAutomatic()) return 0;

 const uint32 Burst = PC->BeginHitPrediction();
 if(Burst == 0) return 0;

 PredictionBurst = Burst;
 PredictionShot = 0;
 PredictionSequence = ShotSequence;
 PredictionAmmoRemaining = AmmoInMagazine;
 PredictionWeaponId = GetWeaponId();
 PredictionOwner = PC;
 if(Delay > KINDA_SMALL_NUMBER)
  World->GetTimerManager().SetTimer(PredictionTimer, this, &AHitscanWeapon::PredictShot, Delay, false);
 else
  PredictShot();
 return Burst;
}

void AHitscanWeapon::StopPredictedFire()
{
 if(auto* World = GetWorld()) World->GetTimerManager().ClearTimer(PredictionTimer);
 PredictionBurst = 0;
 PredictionAmmoRemaining = 0;
 PredictionOwner.Reset();
}

void AHitscanWeapon::PredictShot()
{
 auto* PC = GetPredictionController();
 auto* World = GetWorld();
 if(!PC || !World || PredictionBurst == 0 || PC != PredictionOwner.Get()
   || PredictionWeaponId != GetWeaponId() || PredictionAmmoRemaining <= 0 || AmmoInMagazine <= 0)
 {
  StopPredictedFire();
  return;
 }

 const uint32 Shot = ++PredictionShot;
 const uint32 Sequence = ++PredictionSequence;
 --PredictionAmmoRemaining;
 NextPredictionFireTime = World->GetTimeSeconds() + FireInterval;
 if(PC->CanPredictShot(PredictionBurst, Shot))
 {
  const auto* Character = CastChecked<AVanthexCharacter>(GetOwner());
  const auto* ShooterState = Character->GetPlayerState<AVanthexPlayerState>();
  const auto& Profile = GetProfile();
  const FVector Start = Character->GetPawnViewLocation();
  const FRotator View = PC->GetControlRotation();
  const float SpreadScale = Character->IsAiming() ? VanthexWeaponPresentation::Aim(GetWeaponId()).MovingSpreadScale : 1.f;
  const float MovementForSpread = Character->GetVelocity().Size2D() * SpreadScale;
  FCollisionQueryParams Params(SCENE_QUERY_STAT(VanthexPredictedHitscan), false, Character);
  Params.AddIgnoredActor(this);
  for(int32 Pellet = 0; Pellet < Profile.Pellets; ++Pellet)
  {
   FHitResult Hit;
   TracePellet(Hit, Params, Start, View, Sequence, Pellet, MovementForSpread);
   const auto* Target = Hit.GetActor();
   if(!Hit.bBlockingHit || !Target || Profile.DamageAtDistance(Hit.Distance) <= 0.f) continue;

   const auto* Health = Target->FindComponentByClass<UHealthComponent>();
   if(!Health || Health->IsDead() || Health->GetHealth() <= 0.f) continue;

   if(const auto* TargetCharacter = Cast<AVanthexCharacter>(Target))
   {
    const auto* TargetState = TargetCharacter->GetPlayerState<AVanthexPlayerState>();
    if(!ShooterState || !TargetState || !TargetState->IsAlive()
      || ShooterState->GetTeam() == EVanthexTeam::None || TargetState->GetTeam() == EVanthexTeam::None
      || ShooterState->GetTeam() == TargetState->GetTeam()) continue;
   }
   else if(!Target->IsA<AVanthexTestTarget>())
   {
    continue;
   }

   PC->ShowPredictedHit(PredictionBurst, Shot);
   break;
  }
 }

 if(IsAutomatic() && PredictionAmmoRemaining > 0)
  World->GetTimerManager().SetTimer(PredictionTimer, this, &AHitscanWeapon::PredictShot, FireInterval, false);
 else
  StopPredictedFire();
}

void AHitscanWeapon::ClearServerPrediction()
{
 if(ServerPredictionBurst != 0 && ServerPredictionShot == 0)
  if(auto* PC = ServerPredictionOwner.Get())
   PC->ClientResolvePredictedShot(ServerPredictionBurst, 0, false, false);

 ServerPredictionBurst = 0;
 ServerPredictionShot = 0;
 ServerPredictionOwner.Reset();
}

void AHitscanWeapon::StartFire()
{
 StartTaggedFire(0);
}

void AHitscanWeapon::StartTaggedFire(uint32 Burst)
{
 if(!HasAuthority()) return;

 const auto* OwnerPawn = Cast<APawn>(GetOwner());
 auto* PC = OwnerPawn ? Cast<AVanthexPlayerController>(OwnerPawn->GetController()) : nullptr;
 const auto* Character = Cast<AVanthexCharacter>(OwnerPawn);
 if(bTriggerHeld || (Character && !Character->CanUseRifle()))
 {
  if(Burst != 0 && PC) PC->ClientResolvePredictedShot(Burst, 0, false, false);
  return;
 }

 ServerPredictionBurst = PC ? Burst : 0;
 ServerPredictionShot = 0;
 ServerPredictionOwner = ServerPredictionBurst != 0 ? PC : nullptr;
 Super::StartFire();
 if(bReloading)
 {
  ClearServerPrediction();
  return;
 }

 FireShot();
 if(!bTriggerHeld || bReloading || !IsAutomatic())
 {
  if(ServerPredictionShot == 0) ClearServerPrediction();
  return;
 }
 ScheduleAutomaticFire();
}
void AHitscanWeapon::ScheduleAutomaticFire()
{
 if(!GetWorld()||!bTriggerHeld||bReloading||!IsAutomatic()) return;
 const float Delay=FMath::Max(.001f,NextFireTime-GetWorld()->GetTimeSeconds());
 GetWorld()->GetTimerManager().SetTimer(FireTimer,this,&AHitscanWeapon::ContinueAutomaticFire,Delay,false);
}
void AHitscanWeapon::ContinueAutomaticFire()
{
 FireShot();
 ScheduleAutomaticFire();
}
void AHitscanWeapon::StartReload()
{
 StopPredictedFire();
 if(!HasAuthority()||bReloading||AmmoInMagazine>=MagazineSize||GetReserveAmmo()<=0) return;
 if(const auto* Character=Cast<AVanthexCharacter>(GetOwner())) if(!Character->CanUseRifle()) return;
 if(auto* Character=Cast<AVanthexCharacter>(GetOwner())) Character->SetAiming(false);
 if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(FireTimer);
 if(ServerPredictionShot == 0) ClearServerPrediction();
 Super::StartReload();
}
void AHitscanWeapon::StopFire()
{
 Super::StopFire();
 StopPredictedFire();
 ClearServerPrediction();
 if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(FireTimer);
}
void AHitscanWeapon::FinishReload()
{
 const bool Resume=bTriggerHeld&&IsAutomatic();
 Super::FinishReload();
 if(Resume)
 {
  FireShot();
  ScheduleAutomaticFire();
 }
}

bool AHitscanWeapon::IsHeadHit(const FHitResult& Hit) const
{
	if (const UPrimitiveComponent* HitComponent = Hit.GetComponent())
	{
		if (HitComponent->ComponentHasTag(TEXT("Head")))
		{
			return true;
		}
	}

	const FString Bone = Hit.BoneName.ToString();
	return Bone.Contains(TEXT("head"), ESearchCase::IgnoreCase);
}

FVector AHitscanWeapon::TracePellet(FHitResult& Hit, const FCollisionQueryParams& Params,
 const FVector& Start, const FRotator& View, uint32 Sequence, int32 Pellet, float MovementForSpread) const
{
 const FVector End = Start + GetProfile().ShotDirection(View, Sequence, Pellet, MovementForSpread) * TraceRange;
 GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
 return End;
}

void AHitscanWeapon::FireShot()
{
 if(!HasAuthority()) return;
 auto* Character=Cast<AVanthexCharacter>(GetOwner());
 if(Character&&!Character->CanUseRifle()) { StopFire(); return; }
 auto* World=GetWorld(); if(!World||bReloading) return;
 auto* Mode=Cast<AVanthexGameMode>(World->GetAuthGameMode());
 if(Mode&&!Mode->IsRoundActive()) { StopFire(); return; }
 auto* OwnerPawn=Cast<APawn>(GetOwner());
 auto* Controller=OwnerPawn?OwnerPawn->GetController():nullptr;
 if(!Controller || (ServerPredictionBurst != 0 && ServerPredictionOwner.Get() != Controller))
 {
  StopFire();
  return;
 }
 if(const auto* Health=OwnerPawn->FindComponentByClass<UHealthComponent>()) if(Health->IsDead()) { StopFire(); return; }
 const float Now=World->GetTimeSeconds();
 if(Now+KINDA_SMALL_NUMBER<NextFireTime) return;
 if(AmmoInMagazine<=0) {
  if(GetWeaponId()==TEXT("LegacyRifle")&&GetReserveAmmo()>0) StartReload();
  else { UVanthexWeaponAudioComponent::DispatchEvent(this,GetWeaponId(),EVanthexWeaponAudioEvent::DryFire,NextActionId()); StopFire(); NextFireTime=Now+.2f; }
  return;
 }
 const FVector Start=OwnerPawn->GetPawnViewLocation();
 const FRotator View=Controller->GetControlRotation();
 const auto& Profile=GetProfile();
 if(Now-LastShotTime>FMath::Max(.35f,FireInterval*2.5f)) BurstIndex=0;
 LastShotTime=Now;
 NextFireTime=Now+FireInterval;
 const uint32 Sequence=++ShotSequence;
 // A kill may end the round and clear the trigger before this shot is resolved.
 const uint32 Burst=ServerPredictionBurst;
 const uint32 Shot=Burst != 0 ? ++ServerPredictionShot : 0;
 TWeakObjectPtr<AVanthexPlayerController> FeedbackController=Cast<AVanthexPlayerController>(Controller);
 SetMagazineAmmo(AmmoInMagazine-1);
 const uint32 Action=NextActionId();
 UVanthexWeaponAudioComponent::DispatchEvent(this,GetWeaponId(),EVanthexWeaponAudioEvent::Shot,Action);
 UVanthexWeaponAudioComponent::DispatchEvent(this,GetWeaponId(),EVanthexWeaponAudioEvent::Mechanic,Action);
 FCollisionQueryParams Params(SCENE_QUERY_STAT(VanthexHitscan),false,OwnerPawn);
 Params.AddIgnoredActor(this); Params.bReturnPhysicalMaterial=true;
 const float SpreadScale=Character&&Character->IsAuthoritativelyAiming()?VanthexWeaponPresentation::Aim(GetWeaponId()).MovingSpreadScale:1.f;
 const float MovementForSpread=OwnerPawn->GetVelocity().Size2D()*SpreadScale;
 FHitResult FirstHit;
 TArray<FHitResult> ExtraImpacts;
 bool bAppliedHit=false, bAppliedHeadshot=false;
 for(int32 Pellet=0;Pellet<Profile.Pellets;++Pellet) {
  FHitResult Hit;
  const FVector End=TracePellet(Hit,Params,Start,View,Sequence,Pellet,MovementForSpread);
  if(Pellet==0) {
   FirstHit=Hit;
   // One representative ballistic audio event per trigger, also for shotguns.
   UVanthexBulletAudioComponent::DispatchShot(World,OwnerPawn,Start,End,Hit,bSupersonicBullet);
  } else if(Hit.bBlockingHit) ExtraImpacts.Add(Hit);
  if(!Hit.bBlockingHit||!Hit.GetActor()) continue;
  auto* TargetHealth=Hit.GetActor()->FindComponentByClass<UHealthComponent>();
  if(!TargetHealth||(Mode&&!Mode->CanDamage(OwnerPawn,Hit.GetActor()))) continue;
  const bool Head=IsHeadHit(Hit);
  FVanthexDamageRequest Request;
  Request.InstigatorActor=OwnerPawn; Request.InstigatorController=Controller;
  Request.Damage=Profile.DamageAtDistance(Hit.Distance)*(Head?HeadshotMultiplier:1.f);
  Request.bHeadshot=Head; Request.DamageKind=EVanthexDamageKind::Bullet; Request.Hit=Hit;
  Request.bDeferHitMarker=true;
  const float PreviousHealth=TargetHealth->GetHealth();
  TargetHealth->ApplyDamage(Request);
  if(TargetHealth->GetHealth()<PreviousHealth)
  {
   bAppliedHit=true;
   bAppliedHeadshot|=Head;
  }
 }
 if(auto* PC=FeedbackController.Get())
 {
  if(Burst != 0)
   PC->ClientResolvePredictedShot(Burst,Shot,bAppliedHit,bAppliedHeadshot);
  else if(bAppliedHit)
   PC->ClientConfirmHit(bAppliedHeadshot);
 }
 MulticastShot(View,FirstHit);
 if(!ExtraImpacts.IsEmpty()) MulticastPelletImpacts(ExtraImpacts);
 if(Character) Character->ApplyWeaponRecoil(Profile.RecoilForShot(BurstIndex++));
}
void AHitscanWeapon::MulticastShot_Implementation(FRotator View,FHitResult Hit)
{
 Recoil=GetWeaponId()==TEXT("LegacyRifle")?2.2f:FMath::Max(.5f,GetProfile().RecoilPitch*2.f);
 MuzzleTime=.035f; PresentShotFeedback(View); if(Hit.bBlockingHit) PresentImpact(Hit);
}
void AHitscanWeapon::MulticastPelletImpacts_Implementation(const TArray<FHitResult>& Hits)
{
 for(const auto& Hit:Hits) if(Hit.bBlockingHit) PresentImpact(Hit);
}
