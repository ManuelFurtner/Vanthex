#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Vanthex.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ReferenceSkeleton.h"

// Read-only diagnostic for choosing the operator's skeleton, attachments and locomotion.
bool VanthexInspectOperator()
{
 TArray<FString> Lines;
 bool Passed = true;
 auto Add = [&Lines](const FString& Line) { Lines.Add(Line); UE_LOG(LogVanthex, Display, TEXT("OPERATOR_INSPECTION %s"), *Line); };
 USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Meshes/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"));
 if (!Mesh)
 {
  UE_LOG(LogVanthex, Error, TEXT("OPERATOR_INSPECTION mannequin mesh unavailable"));
  return false;
 }
 const FBoxSphereBounds Bounds = Mesh->GetBounds();
 const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
 Add(FString::Printf(TEXT("mesh=%s skeleton=%s bounds_origin=%s bounds_extent=%s bones=%d materials=%d"),
  *Mesh->GetPathName(), *GetPathNameSafe(Mesh->GetSkeleton()), *Bounds.Origin.ToString(), *Bounds.BoxExtent.ToString(), Skeleton.GetNum(), Mesh->GetMaterials().Num()));
 const TArray<FTransform>& LocalPose = Skeleton.GetRefBonePose();
 TArray<FTransform> ComponentPose;
 ComponentPose.SetNum(Skeleton.GetNum());
 for (int32 Index = 0; Index < Skeleton.GetNum(); ++Index)
 {
  const int32 Parent = Skeleton.GetParentIndex(Index);
  ComponentPose[Index] = Parent == INDEX_NONE ? LocalPose[Index] : LocalPose[Index] * ComponentPose[Parent];
  Add(FString::Printf(TEXT("bone=%d name=%s parent=%d local=%s component=%s"),
   Index, *Skeleton.GetBoneName(Index).ToString(), Parent, *LocalPose[Index].ToHumanReadableString(), *ComponentPose[Index].ToHumanReadableString()));
 }
 for (int32 Slot = 0; Slot < Mesh->GetMaterials().Num(); ++Slot)
 {
  const FSkeletalMaterial& Entry = Mesh->GetMaterials()[Slot];
  UMaterialInterface* Material = Entry.MaterialInterface;
  Add(FString::Printf(TEXT("material_slot=%d slot_name=%s material=%s"), Slot, *Entry.MaterialSlotName.ToString(), *GetPathNameSafe(Material)));
  if (!Material) { Passed = false; continue; }
  TArray<FMaterialParameterInfo> Infos;
  TArray<FGuid> Ids;
  Material->GetAllScalarParameterInfo(Infos, Ids);
  for (const auto& Info : Infos) Add(FString::Printf(TEXT("material_slot=%d scalar=%s association=%d index=%d"), Slot, *Info.Name.ToString(), int32(Info.Association), Info.Index));
  Infos.Reset(); Ids.Reset();
  Material->GetAllVectorParameterInfo(Infos, Ids);
  for (const auto& Info : Infos) Add(FString::Printf(TEXT("material_slot=%d vector=%s association=%d index=%d"), Slot, *Info.Name.ToString(), int32(Info.Association), Info.Index));
  Infos.Reset(); Ids.Reset();
  Material->GetAllTextureParameterInfo(Infos, Ids);
  for (const auto& Info : Infos) Add(FString::Printf(TEXT("material_slot=%d texture=%s association=%d index=%d"), Slot, *Info.Name.ToString(), int32(Info.Association), Info.Index));
 }
 for (const TCHAR* Name : {TEXT("ThirdPersonIdle"), TEXT("ThirdPersonWalk"), TEXT("ThirdPersonRun"), TEXT("ThirdPersonJump_Start"), TEXT("ThirdPersonJump_Loop"), TEXT("ThirdPersonJump_End"), TEXT("ThirdPerson_Jump"), TEXT("ThirdPerson_IdleRun_2D")})
 {
  const FString Path = FString::Printf(TEXT("/Game/Meshes/Mannequin/Animations/%s.%s"), Name, Name);
  UAnimationAsset* Asset = LoadObject<UAnimationAsset>(nullptr, *Path);
  if (!Asset) { Add(FString::Printf(TEXT("FAIL animation_missing=%s"), *Path)); Passed = false; continue; }
  const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Asset);
  const bool Compatible = Asset->GetSkeleton() == Mesh->GetSkeleton();
  Passed &= Compatible;
  Add(FString::Printf(TEXT("animation=%s class=%s skeleton=%s duration=%.3f exact_skeleton_match=%d"),
   *Asset->GetPathName(), *Asset->GetClass()->GetName(), *GetPathNameSafe(Asset->GetSkeleton()), Sequence ? Sequence->GetPlayLength() : -1.f, Compatible));
  if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(Asset))
  {
   for (int32 Axis = 0; Axis < 2; ++Axis)
   {
    const FBlendParameter& Parameter = BlendSpace->GetBlendParameter(Axis);
    Add(FString::Printf(TEXT("blend_axis=%d name=%s min=%.3f max=%.3f"), Axis, *Parameter.DisplayName, Parameter.Min, Parameter.Max));
   }
   for (const FBlendSample& Sample : BlendSpace->GetBlendSamples())
    Add(FString::Printf(TEXT("blend_sample=%s value=%s rate=%.3f"), *GetPathNameSafe(Sample.Animation), *Sample.SampleValue.ToString(), Sample.RateScale));
  }
 }
 UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/Meshes/Mannequin/Animations/ThirdPerson_AnimBP.ThirdPerson_AnimBP"));
 const bool BlueprintCompatible = Blueprint && Blueprint->TargetSkeleton == Mesh->GetSkeleton();
 Passed &= BlueprintCompatible;
 Add(FString::Printf(TEXT("anim_blueprint=%s class=%s skeleton=%s exact_skeleton_match=%d"),
  *GetPathNameSafe(Blueprint), Blueprint ? *GetPathNameSafe(Blueprint->GeneratedClass) : TEXT("None"), Blueprint ? *GetPathNameSafe(Blueprint->TargetSkeleton) : TEXT("None"), BlueprintCompatible));
 Lines.Insert(Passed ? TEXT("OVERALL PASS") : TEXT("OVERALL FAIL"), 0);
 const FString Output = FPaths::ProjectSavedDir() / TEXT("OperatorInspection.txt");
 const bool Saved = FFileHelper::SaveStringArrayToFile(Lines, *Output);
 UE_LOG(LogVanthex, Display, TEXT("OPERATOR_INSPECTION %s report=%s"), Passed && Saved ? TEXT("PASS") : TEXT("FAIL"), *Output);
 return Passed && Saved;
}
#endif
