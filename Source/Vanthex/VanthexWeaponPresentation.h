#pragma once
#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class UStaticMeshComponent;

enum class EVanthexSight : uint8 { Iron, Reflex, Scope };
struct FVanthexAimPresentation
{
 EVanthexSight Sight=EVanthexSight::Iron;
 float FieldOfView=72.f;
 FVector SightPoint=FVector(0,0,4);
 FVector CameraRelativeOrigin=FVector(26,0,-4);
 FRotator Rotation=FRotator::ZeroRotator;
 float MovementScale=.55f;
 float MovingSpreadScale=.25f;
};

namespace VanthexWeaponPresentation
{
 // Palm contact points in authored weapon centimetres, shared by ego/world IK.
 FVector RightPalm(FName WeaponId);
 FVector LeftPalm(FName WeaponId, float ReloadProgress = -1.f);
 float WorldScale(FName WeaponId);
 FVanthexAimPresentation Aim(FName WeaponId);
 FName ModelId(FName WeaponId);
 FVector Muzzle(FName WeaponId);
 void Build(AActor* Owner,USceneComponent* Parent,FName WeaponId,bool FirstPerson,
  TArray<TObjectPtr<UStaticMeshComponent>>& Parts,FVector& OutMuzzle);
 /** ReloadProgress < 0 is idle. ShotPulse is 0..1, cosmetic only. */
 void Animate(const TArray<TObjectPtr<UStaticMeshComponent>>& Parts,FName WeaponId,float ReloadProgress,float ShotPulse);
}
