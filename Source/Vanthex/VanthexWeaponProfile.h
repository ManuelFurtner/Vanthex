#pragma once
#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;

enum class EVanthexFireMode : uint8 { SemiAutomatic, Automatic };
enum class EVanthexFirearmShape : uint8 { Pistol, SMG, Rifle, Bullpup, Marksman, Shotgun };

/** Initial playtest tuning; all distances are centimetres, times seconds. */
struct FVanthexWeaponProfile
{
 FName Id;
 bool bSidearm = false;
 EVanthexFireMode FireMode = EVanthexFireMode::Automatic;
 EVanthexFirearmShape Shape = EVanthexFirearmShape::Rifle;
 int32 MagazineSize = 30, ReserveAmmo = 90, Pellets = 1;
 float BodyDamage = 32.f, HeadshotMultiplier = 2.5f;
 float FireInterval = .1f, ReloadDuration = 2.2f, TraceRange = 15000.f;
 float FalloffStart = 2500.f, FalloffEnd = 9000.f, MinimumDamageScale = .65f;
 float SpreadDegrees = .12f, MovingSpreadDegrees = 1.2f;
 float RecoilPitch = .7f, RecoilYaw = .15f;
 float MagazineOut = .25f, MagazineIn = .65f, Bolt = .86f;
 bool bSupersonic = true;
 bool IsAutomatic() const { return FireMode == EVanthexFireMode::Automatic; }
 float DamageAtDistance(float Distance) const;
 FVector2D RecoilForShot(int32 BurstIndex) const;
 FVector ShotDirection(const FRotator& Aim, uint32 ShotSequence, int32 Pellet, float Speed) const;
};

namespace VanthexWeapons
{
 const TArray<FVanthexWeaponProfile>& All();
 const FVanthexWeaponProfile* Find(FName Id);
 // Shared first/third-person prototype silhouettes, with native grip(-1,0,-3.2).
 void BuildVisual(AActor* Owner, USceneComponent* Parent, FName Id, bool FirstPerson,
  TArray<TObjectPtr<UStaticMeshComponent>>& Parts, FVector& Muzzle, UMaterialInterface* Accent = nullptr);
}
