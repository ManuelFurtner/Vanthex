#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "VanthexOperatorAnimInstance.generated.h"

class UBlendSpace;
class UAnimSequence;
struct FVanthexOperatorAnimProxy;

/** Native locomotion and equipment poses shared by human and AI ARC operators. */
UCLASS(Transient)
class UVanthexOperatorAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 UVanthexOperatorAnimInstance();
 float GetAimPitch() const { return AimPitch; }
 float GetCrouchAlpha() const { return CrouchAlpha; }
 float GetDeathAlpha() const { return DeathAlpha; }
 float GetMovementSpeed() const { return MovementSpeed; }
 bool HasLocomotionAssets() const;

 // Mesh faces +Y; the authored rifle faces +X. Its grip, not its origin,
 // is anchored to the same target used by the right-arm IK.
 static FTransform GetRifleTransform(float Pitch, float Crouch, float Lower = 0.f);
 static FTransform GetKnifeTransform(float Pitch, float Crouch, float Lower = 0.f, float Swing = 0.f);

protected:
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
 virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
 friend struct FVanthexOperatorAnimProxy;
 UPROPERTY() TObjectPtr<UBlendSpace> Locomotion;
 UPROPERTY() TObjectPtr<UAnimSequence> JumpStart;
 UPROPERTY() TObjectPtr<UAnimSequence> JumpLoop;
 UPROPERTY() TObjectPtr<UAnimSequence> JumpEnd;
 float AimPitch = 0.f;
 float CrouchAlpha = 0.f;
 float DeathAlpha = 0.f;
 float MovementSpeed = 0.f;
};
