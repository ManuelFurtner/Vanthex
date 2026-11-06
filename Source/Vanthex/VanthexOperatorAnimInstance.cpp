#include "VanthexOperatorAnimInstance.h"

#include "VanthexCharacter.h"
#include "VanthexEquipmentComponent.h"
#include "HitscanWeapon.h"
#include "VanthexWeaponPresentation.h"
#include "Components/SkeletalMeshComponent.h"
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
FQuat RifleAim(float Pitch, float Lower)
{
 Lower = FMath::Clamp(Lower, 0.f, 1.f);
 return AimRotation(Pitch * (1.f - Lower) - 35.f * Lower);
}
FVector RightGrip(float Pitch, float Crouch, float Lower = 0.f)
{
 Lower = FMath::Clamp(Lower, 0.f, 1.f);
 return ShoulderPivot(Crouch) + FVector(10. * Lower, 0., -24. * Lower)
  + RifleAim(Pitch, Lower).RotateVector(FVector(0., 28. - 16. * Lower, 0.));
}

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
 if (EndRotation)
 {
  // Match forearm roll to the wrist's reference orientation. Keeping the idle
  // forearm roll while rotating the hand produces a pinched, twisted wrist.
  const FQuat WristReference=Pose.GetPose().GetBoneContainer().GetRefPoseTransform(End).GetRotation();
  const FQuat ForearmReference=*EndRotation*WristReference.Inverse();
  B.SetRotation(FRotationMatrix::MakeFromXY(B.GetRotation().GetAxisX(),ForearmReference.GetAxisY()).ToQuat());
  C.SetRotation(*EndRotation);
 }
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
 float Lower = 0.f, Swing = 0.f;
 float AirTime = 0.f, LandingTime = 1.f, LandingWeight = 0.f, FallSpeed = 0.f, AuthoredRunSpeed = 375.f;
 bool JumpTakeoff = false;
 bool WasFalling = false, Falling = false, Dead = false, SpeedOnX = false;
 bool Knife = false, Sidearm = false;
 bool FirstPerson = false, HaveFirstPersonMount = false;
 FName WeaponId=TEXT("M4");
 FTransform FirstPersonMount;
 FVector PalmRight, PalmLeft;

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
  // Snapshot the replicated equipment presentation on the game thread. Pose
  // evaluation never reads an actor/component from an animation worker.
  Knife = false; Sidearm = false; Lower = 0.f; Swing = 0.f;
  if (const auto* Equipment = Character->FindComponentByClass<UVanthexEquipmentComponent>())
  {
   const FVanthexEquipmentPresentation Presentation = Equipment->GetPresentation();
   Knife = Presentation.VisibleSlot == EVanthexWeaponSlot::Knife;
   Sidearm = Presentation.VisibleSlot == EVanthexWeaponSlot::Pistol;
   Lower = FMath::Clamp(Presentation.Lower, 0.f, 1.f);
   Swing = FMath::Clamp(Presentation.Swing, 0.f, 1.f);
  }
  const auto* Weapon=Character->GetWeapon();
  WeaponId=Knife?FName(TEXT("Knife")):Weapon?Weapon->GetWeaponId():FName(TEXT("Pistol"));
  PalmRight=VanthexWeaponPresentation::RightPalm(WeaponId);
  PalmLeft=VanthexWeaponPresentation::LeftPalm(WeaponId,Weapon?Weapon->GetReloadProgress():-1.f);
  const auto* PoseMesh=Instance->GetSkelMeshComponent();
  FirstPerson=PoseMesh&&PoseMesh->ComponentHasTag(TEXT("VanthexFirstPersonArms"));
  HaveFirstPersonMount=false;
  if(FirstPerson) {
   const auto* Mount=Knife?Character->GetEquipment()->GetFirstPersonKnifeRoot():Weapon?Weapon->GetFirstPersonPivot():nullptr;
   if(Mount){FirstPersonMount=Mount->GetComponentTransform().GetRelativeTransform(PoseMesh->GetComponentTransform());HaveFirstPersonMount=true;}
   Crouch=0;Speed=0;Falling=false;
  }
  // Round restart clears the death pose immediately; falling into death eases in.
  Death = Dead ? FMath::Min(1.f, Death + DeltaSeconds * 2.2f) : 0.f;
  Falling = !FirstPerson && Movement && Movement->IsFalling() && !Dead;
  if (Falling)
  {
   if (!WasFalling) JumpTakeoff = LocalVelocity.Z > 80.f;
   AirTime = WasFalling ? AirTime + DeltaSeconds : 0.f;
   FallSpeed = FMath::Max(0.f, -LocalVelocity.Z);
  }
  else if (WasFalling)
  {
   LandingTime = 0.f;
   LandingWeight = Dead ? 0.f : FMath::GetMappedRangeValueClamped(
    FVector2D(140.f, 650.f), FVector2D(0.f, .8f), FallSpeed);
  }
  else
  {
   LandingTime += DeltaSeconds;
  }
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
  UAnimSequence* Desired = Falling ? ((JumpTakeoff && AirTime < .15f && Start) ? Start : Loop) : (End ? End : Loop);
  if (Air.GetSequence() != Desired)
  {
   Air.SetSequence(Desired);
   Air.SetAccumulatedTime(0.f);
  }
  Air.SetLoopAnimation(Desired == Loop);
  const float WantedAir = Falling ? 1.f : LandingWeight * FMath::Clamp(1.f - LandingTime / .22f, 0.f, 1.f);
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
   const FTransform Mount = FirstPerson&&HaveFirstPersonMount?FirstPersonMount:Knife ? UVanthexOperatorAnimInstance::GetKnifeTransform(Pitch, Crouch, Lower, Swing)
    : UVanthexOperatorAnimInstance::GetRifleTransform(Pitch, Crouch, Lower,WeaponId);
   const FVector Grip = Mount.TransformPosition(PalmRight);
   const FVector Forward = Mount.GetRotation().GetAxisX();
   const FVector Up = Mount.GetRotation().GetAxisZ();
   const FVector Side = Mount.GetRotation().GetAxisY();
   // Wrist targets leave room for the palm. A support hand crosses the fore-end:
   // its fingers wrap around the barrel rather than pointing along it.
   const FVector RightWrist = Grip - Forward * 11.;
   const FVector LeftDirection = Sidearm ? Forward : Side;
   const FVector LeftSupport = Mount.TransformPosition(PalmLeft) - LeftDirection*10. - (Sidearm?FVector::ZeroVector:Up*2.);
   const FVector LeftGuard(23. + 5. * Swing, 13. - 6. * Swing, 124. - 32. * Crouch - 4. * Swing);
   // The free hand guards the chest during knife use. Both poses converge at
   // full holster, where VisibleSlot changes, avoiding a discontinuous wrist.
   const FVector EgoGuard(17,38,127);
   const FVector LeftWrist = Knife ? FMath::Lerp(FirstPerson?EgoGuard:LeftGuard, LeftSupport, double(Lower)) : LeftSupport;
   // This skeleton mirrors hand translations: fingers extend along -X on
   // the right hand and +X on the left. Palm normals are their local Y axes.
   const FQuat RightHand = FRotationMatrix::MakeFromXY(-Forward, -Mount.GetRotation().GetAxisY()).ToQuat();
   const FQuat LeftHand = FRotationMatrix::MakeFromXY(LeftDirection, Sidearm?Side:Up).ToQuat();
   const float Thrust = Knife ? Swing * (1.f - Lower) : 0.f;
   SolveLimb(ComponentPose, TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"), RightWrist, FirstPerson?FVector(-35,40,85):FVector(-65. + 10. * Thrust, 10. + 12. * Thrust, 105. - 32. * Crouch + 8. * Thrust), &RightHand);
   SolveLimb(ComponentPose, TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), LeftWrist, FirstPerson?FVector(35,40,85):FVector(65., 15., 105. - 32. * Crouch), &LeftHand);
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
      const float Curl = Right ? (Segment == 1 ? -15.f : Segment == 2 ? -83.f : -99.f) : (Segment == 1 ? 65.f : Segment == 2 ? 107.f : 81.f);
      const bool TriggerFinger = Right && FCString::Strcmp(Finger, TEXT("index")) == 0;
      const float TriggerScale = TriggerFinger ? (Knife ? FMath::Lerp(1.f, .55f, Lower) : .55f) : 1.f;
      const float GuardScale = Knife && !Right ? FMath::Lerp(.45f, 1.f, Lower) : 1.f;
      FTransform& Transform = Output.Pose[Index];
      // Author from the reference fingers, not a locomotion clip's arbitrary hand
      // curl. Otherwise adding a grip rotation can straighten or reverse a fist.
      Transform.SetRotation(Output.Pose.GetBoneContainer().GetRefPoseTransform(Index).GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(Curl * TriggerScale * GuardScale)));
     }
   for(const TCHAR* Side:{TEXT("l"),TEXT("r")})for(int32 Segment=1;Segment<=3;++Segment){
    const auto Index=Bone(Output.Pose,*FString::Printf(TEXT("thumb_%02d_%s"),Segment,Side));
    if(Index.GetInt()==INDEX_NONE)continue;
    const float Curl=(Side[0]==TCHAR('r')?-1.f:1.f)*(Segment==1?25.f:35.f);
    Output.Pose[Index].SetRotation(Output.Pose.GetBoneContainer().GetRefPoseTransform(Index).GetRotation()*FQuat(FVector::UpVector,FMath::DegreesToRadians(Curl)));
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

FTransform UVanthexOperatorAnimInstance::GetRifleTransform(float Pitch, float Crouch, float Lower,FName WeaponId)
{
 const FQuat Rotation = RifleAim(Pitch, Lower) * FRotator(0., 90., 0.).Quaternion();
 const double Scale = VanthexWeaponPresentation::WorldScale(WeaponId);
 const FVector NativeGrip=VanthexWeaponPresentation::RightPalm(WeaponId);
 return FTransform(Rotation, RightGrip(Pitch, Crouch, Lower) - Rotation.RotateVector(NativeGrip * Scale), FVector(Scale));
}

FTransform UVanthexOperatorAnimInstance::GetKnifeTransform(float Pitch, float Crouch, float Lower, float Swing)
{
 Lower = FMath::Clamp(Lower, 0.f, 1.f);
 Swing = FMath::Clamp(Swing, 0.f, 1.f);
 const FQuat ReadyAim = AimRotation(Pitch + 8.f - 12.f * Swing);
 const FVector ReadyGrip = FVector(-22. + 12. * Swing, 0., 128. - 32. * Crouch + 13. * Swing)
  + ReadyAim.RotateVector(FVector(0., 24. + 18. * Swing, 0.));
 const FQuat Aim = FQuat::Slerp(ReadyAim, RifleAim(Pitch, 1.f), Lower).GetNormalized();
 const FQuat Rotation = Aim * FRotator(0., 90., 0.).Quaternion();
 const FVector Grip = FMath::Lerp(ReadyGrip, RightGrip(Pitch, Crouch, 1.f), double(Lower));
 // The knife geometry is authored at centimetre scale, with its palm grip at
 // local X=-4. Its replicated swing envelope moves the same grip used by IK.
 return FTransform(Rotation, Grip - Rotation.RotateVector(FVector(-4., 0., 0.)), FVector::OneVector);
}

FAnimInstanceProxy* UVanthexOperatorAnimInstance::CreateAnimInstanceProxy() { return new FVanthexOperatorAnimProxy(this); }
void UVanthexOperatorAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
