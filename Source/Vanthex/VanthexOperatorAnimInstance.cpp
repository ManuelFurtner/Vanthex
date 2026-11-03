#include "VanthexOperatorAnimInstance.h"

#include "VanthexCharacter.h"
#include "HealthComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "BonePose.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Math/RotationMatrix.h"
#include "TwoBoneIK.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
FVector ShoulderPivot(float Crouch) { return FVector(-14., 0., 142. - 32. * Crouch); }
FQuat AimRotation(float Pitch) { return FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Pitch)); }
FVector RightGrip(float Pitch, float Crouch) { return ShoulderPivot(Crouch) + AimRotation(Pitch).RotateVector(FVector(0., 28., 0.)); }

FCompactPoseBoneIndex Bone(const FCompactPose& Pose, const TCHAR* Name)
{
 const FBoneContainer& Bones = Pose.GetBoneContainer();
 const int32 Index = Bones.GetPoseBoneIndexForBoneName(FName(Name));
 return Index == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index));
}

void SolveLimb(FCSPose<FCompactPose>& Pose, const TCHAR* UpperName, const TCHAR* LowerName,
 const TCHAR* EndName, const FVector& Target, const FVector& Pole, const FQuat* EndRotation = nullptr)
{
 const auto Upper = Bone(Pose.GetPose(), UpperName), Lower = Bone(Pose.GetPose(), LowerName), End = Bone(Pose.GetPose(), EndName);
 if (Upper.GetInt() == INDEX_NONE || Lower.GetInt() == INDEX_NONE || End.GetInt() == INDEX_NONE) return;
 FTransform A = Pose.GetComponentSpaceTransform(Upper);
 FTransform B = Pose.GetComponentSpaceTransform(Lower);
 FTransform C = Pose.GetComponentSpaceTransform(End);
 AnimationCore::SolveTwoBoneIK(A, B, C, Pole, Target, false, 1., 1.);
 if (EndRotation) C.SetRotation(*EndRotation);
 TArray<FBoneTransform, TInlineAllocator<3>> Changes;
 Changes.Emplace(Upper, A); Changes.Emplace(Lower, B); Changes.Emplace(End, C);
 Pose.SafeSetCSBoneTransforms(Changes);
}
}

struct FVanthexOperatorAnimProxy final : FAnimInstanceProxy
{
 explicit FVanthexOperatorAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
 {
  GroundAir.A.SetLinkNode(&Ground);
  GroundAir.B.SetLinkNode(&Air);
 }

 FAnimNode_BlendSpacePlayer_Standalone Ground;
 FAnimNode_SequencePlayer_Standalone Air;
 FAnimNode_TwoWayBlend GroundAir;
 UAnimSequence* Start = nullptr;
 UAnimSequence* Loop = nullptr;
 UAnimSequence* End = nullptr;
 float Speed = 0.f, Direction = 0.f, Pitch = 0.f, Crouch = 0.f, Death = 0.f;
 float AirTime = 0.f, LandingTime = 1.f, AuthoredRunSpeed = 375.f;
 bool WasFalling = false, Falling = false, Dead = false, SpeedOnX = false;

 virtual FAnimNode_Base* GetCustomRootNode() override { return &GroundAir; }
 virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override
 {
  Nodes.Add(&GroundAir); Nodes.Add(&Ground); Nodes.Add(&Air);
 }

 virtual void Initialize(UAnimInstance* Instance) override
 {
  const auto* Operator = CastChecked<UVanthexOperatorAnimInstance>(Instance);
  Ground.SetBlendSpace(Operator->Locomotion);
  Ground.SetLoop(true);
  SpeedOnX = Operator->Locomotion && Operator->Locomotion->GetBlendParameter(0).DisplayName.Contains(TEXT("Speed"));
  if (Operator->Locomotion)
   AuthoredRunSpeed = FMath::Max(1.f, Operator->Locomotion->GetBlendParameter(SpeedOnX ? 0 : 1).Max);
  Start = Operator->JumpStart; Loop = Operator->JumpLoop; End = Operator->JumpEnd;
  Air.SetSequence(Loop);
  Air.SetLoopAnimation(true);
  FAnimInstanceProxy::Initialize(Instance);
 }

 virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
 {
  FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
  auto* Operator = CastChecked<UVanthexOperatorAnimInstance>(Instance);
  const auto* Character = Cast<AVanthexCharacter>(Instance->TryGetPawnOwner());
  if (!Character) return;
  const auto* Movement = Character->GetCharacterMovement();
  Dead = Character->GetHealthComponent() && Character->GetHealthComponent()->IsDead();
  Speed = Dead ? 0.f : Character->GetVelocity().Size2D();
  const FVector LocalVelocity = Character->GetActorQuat().UnrotateVector(Character->GetVelocity());
  Direction = Speed > 5.f ? FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X)) : 0.f;
  Pitch = FMath::FInterpTo(Pitch, FMath::Clamp(FRotator::NormalizeAxis(Character->GetBaseAimRotation().Pitch), -60.f, 60.f), DeltaSeconds, 12.f);
  Crouch = FMath::FInterpTo(Crouch, Character->bIsCrouched ? 1.f : 0.f, DeltaSeconds, 12.f);
  // Round restart clears the death pose immediately; falling into death eases in.
  Death = Dead ? FMath::Min(1.f, Death + DeltaSeconds * 2.2f) : 0.f;
  Falling = Movement && Movement->IsFalling() && !Dead;
  if (Falling) { AirTime = WasFalling ? AirTime + DeltaSeconds : 0.f; LandingTime = 0.f; }
  else LandingTime = WasFalling ? 0.f : LandingTime + DeltaSeconds;
  WasFalling = Falling;
  Operator->AimPitch = Pitch;
  Operator->CrouchAlpha = Crouch;
  Operator->DeathAlpha = Death;
  Operator->MovementSpeed = Speed;
 }

 virtual void UpdateAnimationNode(const FAnimationUpdateContext& Context) override
 {
  const float DeltaSeconds = Context.GetDeltaTime();
  // The supplied one-dimensional blendspace authors its run at 375 cm/s.
  // Scale the cycle beyond that speed instead of letting 600/900 cm/s actors
  // slide over the floor with an unchanged jogging cadence.
  Ground.SetPosition(SpeedOnX ? FVector(FMath::Min(Speed, AuthoredRunSpeed), 0., 0.) : FVector(Direction, FMath::Min(Speed, AuthoredRunSpeed), 0.));
  Ground.SetPlayRate(Dead ? 0.f : FMath::Max(1.f, Speed / AuthoredRunSpeed));
  UAnimSequence* Desired = Falling ? ((AirTime < .15f && Start) ? Start : Loop) : (End ? End : Loop);
  if (Air.GetSequence() != Desired)
  {
   Air.SetSequence(Desired);
   Air.SetAccumulatedTime(0.f);
  }
  Air.SetLoopAnimation(Desired == Loop);
  const float WantedAir = Falling ? 1.f : (LandingTime < .12f ? .65f : 0.f);
  GroundAir.Alpha = FMath::FInterpTo(GroundAir.Alpha, WantedAir, DeltaSeconds, 15.f);
  FAnimInstanceProxy::UpdateAnimationNode(Context);
 }

 virtual bool Evaluate(FPoseContext& Output) override
 {
  GroundAir.Evaluate_AnyThread(Output);
  if (Output.Pose.GetNumBones() == 0) return true;

  // Preserve animated foot contacts while lowering the pelvis into a crouch.
  FCSPose<FCompactPose> ComponentPose;
  ComponentPose.InitPose(Output.Pose);
  const auto Pelvis = Bone(Output.Pose, TEXT("pelvis"));
  const auto FootR = Bone(Output.Pose, TEXT("foot_r")), FootL = Bone(Output.Pose, TEXT("foot_l"));
  if (Crouch > .001f && Pelvis.GetInt() != INDEX_NONE && FootR.GetInt() != INDEX_NONE && FootL.GetInt() != INDEX_NONE)
  {
   const FVector RightFoot = ComponentPose.GetComponentSpaceTransform(FootR).GetLocation();
   const FVector LeftFoot = ComponentPose.GetComponentSpaceTransform(FootL).GetLocation();
   // Pelvis translation is in its parent's (root) space.
   Output.Pose[Pelvis].AddToTranslation(FVector(0., 0., -32. * Crouch));
   ComponentPose.InitPose(Output.Pose);
   SolveLimb(ComponentPose, TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), RightFoot, RightFoot + FVector(0., 100., 35.));
   SolveLimb(ComponentPose, TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), LeftFoot, LeftFoot + FVector(0., 100., 35.));
  }

  if (Death < .2f)
  {
   const FVector Grip = RightGrip(Pitch, Crouch);
   const FQuat Aim = AimRotation(Pitch);
   const FVector Forward = Aim.RotateVector(FVector(0., 1., 0.));
   const FVector Up = Aim.RotateVector(FVector::UpVector);
   // The hand bone is the wrist: put the palm about 9 cm ahead onto each
   // grip. Targeting the palm with the wrist made the left arm overextend.
   const FVector RightWrist = Grip - Forward * 9.;
   const FVector LeftWrist = Grip + Aim.RotateVector(FVector(4., 10., 0.));
   // This skeleton mirrors hand translations: fingers extend along -X on
   // the right hand and +X on the left. Palm normals are their local Y axes.
   const FQuat RightHand = FRotationMatrix::MakeFromXY(-Forward, FVector::ForwardVector).ToQuat();
   const FQuat LeftHand = FRotationMatrix::MakeFromXY(Forward, Up).ToQuat();
   SolveLimb(ComponentPose, TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"), RightWrist, FVector(-65., 10., 105. - 32. * Crouch), &RightHand);
   SolveLimb(ComponentPose, TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), LeftWrist, FVector(65., 15., 105. - 32. * Crouch), &LeftHand);
  }
  FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(ComponentPose), Output.Pose);

  if (Death < .2f)
  {
   for (const TCHAR* Side : {TEXT("l"), TEXT("r")})
    for (const TCHAR* Finger : {TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky")})
     for (int32 Segment = 1; Segment <= 3; ++Segment)
     {
      const FString Name = FString::Printf(TEXT("%s_%02d_%s"), Finger, Segment, Side);
      const auto Index = Bone(Output.Pose, *Name);
      if (Index.GetInt() == INDEX_NONE) continue;
      const bool Right = Side[0] == TCHAR('r');
      const float Curl = Right ? (Segment == 1 ? -25.f : Segment == 2 ? -55.f : -35.f) : (Segment == 3 ? 30.f : 55.f);
      const float TriggerScale = Right && FCString::Strcmp(Finger, TEXT("index")) == 0 ? .55f : 1.f;
      FTransform& Transform = Output.Pose[Index];
      Transform.SetRotation(Transform.GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(Curl * TriggerScale)));
     }
  }

  if (Death > .001f)
  {
   // Cosmetic collapse, independent of server damage/collision and deterministic
   // on clients. Resetting health restores the ordinary pose next update.
   FTransform& Root = Output.Pose[FCompactPoseBoneIndex(0)];
   Root.SetRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(88.f * Death)) * Root.GetRotation());
   Root.AddToTranslation(FVector(0., 0., 25. * Death));
  }
  Output.Pose.NormalizeRotations();
  return true;
 }
};

UVanthexOperatorAnimInstance::UVanthexOperatorAnimInstance()
{
 static ConstructorHelpers::FObjectFinder<UBlendSpace> Ground(TEXT("/Game/Meshes/Mannequin/Animations/ThirdPerson_IdleRun_2D.ThirdPerson_IdleRun_2D"));
 static ConstructorHelpers::FObjectFinder<UAnimSequence> Start(TEXT("/Game/Meshes/Mannequin/Animations/ThirdPersonJump_Start.ThirdPersonJump_Start"));
 static ConstructorHelpers::FObjectFinder<UAnimSequence> Loop(TEXT("/Game/Meshes/Mannequin/Animations/ThirdPersonJump_Loop.ThirdPersonJump_Loop"));
 static ConstructorHelpers::FObjectFinder<UAnimSequence> End(TEXT("/Game/Meshes/Mannequin/Animations/ThirdPersonJump_End.ThirdPersonJump_End"));
 Locomotion = Ground.Object; JumpStart = Start.Object; JumpLoop = Loop.Object; JumpEnd = End.Object;
}

bool UVanthexOperatorAnimInstance::HasLocomotionAssets() const { return Locomotion && JumpStart && JumpLoop && JumpEnd; }

FTransform UVanthexOperatorAnimInstance::GetRifleTransform(float Pitch, float Crouch)
{
 const FQuat Rotation = AimRotation(Pitch) * FRotator(0., 90., 0.).Quaternion();
 constexpr double Scale = 1.7;
 const FVector NativeGrip(-1., 0., -3.2);
 return FTransform(Rotation, RightGrip(Pitch, Crouch) - Rotation.RotateVector(NativeGrip * Scale), FVector(Scale));
}

FAnimInstanceProxy* UVanthexOperatorAnimInstance::CreateAnimInstanceProxy() { return new FVanthexOperatorAnimProxy(this); }
void UVanthexOperatorAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
