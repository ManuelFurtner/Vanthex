#include "VanthexWeaponPickup.h"
#include "VanthexCharacter.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexWeaponProfile.h"
#include "VanthexWeaponPresentation.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AVanthexWeaponPickup::AVanthexWeaponPickup()
{
 bReplicates = true;
 SetReplicateMovement(true);
 SetNetUpdateFrequency(20.f);
 Collision = CreateDefaultSubobject<USphereComponent>(TEXT("DropCollision"));
 SetRootComponent(Collision);
 Collision->InitSphereRadius(8.f);
 Collision->SetCollisionObjectType(ECC_WorldDynamic);
 Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
 Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
 Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
 Collision->SetGenerateOverlapEvents(false);
 VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DroppedWeaponVisual"));
 VisualRoot->SetupAttachment(Collision);
 VisualRoot->SetRelativeRotation(FRotator(0, 0, 90));
 Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("DropMovement"));
 Movement->SetUpdatedComponent(Collision);
 Movement->bAutoActivate = false;
 Movement->bRotationFollowsVelocity = false;
 Movement->bShouldBounce = true;
 Movement->Bounciness = .2f;
 Movement->Friction = .8f;
 Movement->BounceVelocityStopSimulatingThreshold = 45.f;
 Movement->bForceSubStepping = true;
}

void AVanthexWeaponPickup::BeginPlay()
{
 Super::BeginPlay();
 if (HasAuthority()) SetLifeSpan(180.f);
 else Movement->SetComponentTickEnabled(false);
 RefreshVisual();
}

void AVanthexWeaponPickup::RefreshVisual()
{
 if (!GetWorld() || GetNetMode() == NM_DedicatedServer || WeaponData.Id.IsNone()) return;
 for (auto& Part : Parts) if (Part) Part->DestroyComponent();
 Parts.Reset();
 VisualRoot->SetRelativeScale3D(FVector(VanthexWeaponPresentation::WorldScale(WeaponData.Id)));
 FVector Muzzle;
 VanthexWeaponPresentation::Build(this, VisualRoot, WeaponData.Id, false, Parts, Muzzle);
 for (auto& Part : Parts) if (Part) {
  Part->SetOwnerNoSee(false);
  Part->SetOnlyOwnerSee(false);
  Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 }
}

AVanthexWeaponPickup* AVanthexWeaponPickup::SpawnDrop(AVanthexCharacter* Source, const FVanthexDroppedWeapon& Data, bool Throw)
{
 if (!Source || !Source->HasAuthority() || !VanthexWeapons::Find(Data.Id)) return nullptr;
 auto* World = Source->GetWorld();
 const FVector Direction = Source->GetControlRotation().Vector();
 const FVector Start = Source->GetActorLocation() + FVector(0, 0, 15);
 const FVector Desired = Start + Direction * 65.f + Source->GetActorRightVector() * (VanthexWeapons::Find(Data.Id)->bSidearm ? 20.f : -20.f);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(WeaponDropSpawn), false, Source);
 Query.AddIgnoredActor(Source->GetWeapon());
 FHitResult Hit;
 const bool Blocked = World->SweepSingleByChannel(Hit, Start, Desired, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(10.f), Query);
 if (Blocked && Hit.bStartPenetrating) return nullptr;
 const FVector Position = Blocked ? Hit.Location + Hit.Normal * 2.f : Desired;
 const FTransform Transform(FRotator(0, Source->GetActorRotation().Yaw, 0), Position);
 auto* Drop = World->SpawnActorDeferred<AVanthexWeaponPickup>(StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if (!Drop) return nullptr;
 Drop->WeaponData = Data;
 Drop->FinishSpawning(Transform);
 Drop->Movement->Velocity = Direction * (Throw ? 430.f : 100.f) + FVector(0, 0, Throw ? 140.f : 70.f);
 Drop->Movement->Activate();
 Drop->ForceNetUpdate();
 return Drop;
}

bool AVanthexWeaponPickup::CanReach(const AVanthexCharacter* Character) const
{
 if (!Character || bClaimed || IsActorBeingDestroyed() || !VanthexWeapons::Find(WeaponData.Id)
  || !Character->GetHealthComponent() || Character->GetHealthComponent()->IsDead()
  || FVector::DistSquared(Character->GetActorLocation(), GetActorLocation()) > FMath::Square(180.f)) return false;
 const FVector From = Character->GetPawnViewLocation();
 const FVector Offset = GetActorLocation() - From;
 if (FVector::DotProduct(Offset.GetSafeNormal(), Character->GetControlRotation().Vector()) < .55f) return false;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(WeaponPickupReach), false, Character);
 Query.AddIgnoredActor(Character->GetWeapon());
 Query.AddIgnoredActor(this);
 FHitResult Hit;
 return !GetWorld()->LineTraceSingleByChannel(Hit, From, GetActorLocation(), ECC_Visibility, Query);
}

AVanthexWeaponPickup* AVanthexWeaponPickup::FindFor(const AVanthexCharacter* Character)
{
 if (!Character || !Character->GetWorld()) return nullptr;
 AVanthexWeaponPickup* Best = nullptr;
 double BestDistance = TNumericLimits<double>::Max();
 for (TActorIterator<AVanthexWeaponPickup> It(Character->GetWorld()); It; ++It) {
  const double Distance = FVector::DistSquared(Character->GetActorLocation(), It->GetActorLocation());
  if (Distance < BestDistance && It->CanReach(Character)) { Best = *It; BestDistance = Distance; }
 }
 return Best;
}

bool AVanthexWeaponPickup::TryPickup(AVanthexCharacter* Character)
{
 if (!HasAuthority() || !CanReach(Character) || !Character->GetEquipment()) return false;
 // Game-thread transaction: the inventory is updated once before this actor disappears.
 bClaimed = true;
 if (!Character->GetEquipment()->TakeDroppedWeapon(WeaponData)) { bClaimed = false; return false; }
 Destroy();
 return true;
}

void AVanthexWeaponPickup::ClearForRound(UWorld* World)
{
 if (!World || World->GetNetMode() == NM_Client) return;
 for (TActorIterator<AVanthexWeaponPickup> It(World); It; ++It) It->Destroy();
}

void AVanthexWeaponPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(AVanthexWeaponPickup, WeaponData);
}
