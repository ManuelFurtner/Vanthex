#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Vanthex.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

namespace
{
template <typename T> T* OperatorExpression(UMaterial* Material)
{
 T* Expression = NewObject<T>(Material);
 Material->GetExpressionCollection().AddExpression(Expression);
 return Expression;
}

void OperatorConnect(FExpressionInput& Input, UMaterialExpression* Expression, int32 OutputIndex = 0)
{
 Input.Connect(OutputIndex, Expression);
}
}

// Build only ARC's new material; the supplied mannequin's textures and materials stay intact.
bool VanthexBuildOperatorArt()
{
 UTexture2D* Mask = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Meshes/Mannequin/Character/Textures/UE4_Mannequin_MAT_MASKA.UE4_Mannequin_MAT_MASKA"));
 UTexture2D* Normal = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Meshes/Mannequin/Character/Textures/UE4_Mannequin__normals.UE4_Mannequin__normals"));
 TArray64<uint8> Pixels;
 if (!Mask || !Normal || Mask->Source.GetFormat() != TSF_BGRA8 || !Mask->Source.GetMipData(Pixels, 0) || Pixels.IsEmpty() || Pixels.Num() % 4 != 0)
 {
  UE_LOG(LogVanthex, Error, TEXT("OPERATOR_ART missing mannequin textures or unsupported source mask format"));
  return false;
 }

 // Inspect the actual packed channels, choosing a sparse RGB region for team accents.
 // Alpha can be an opaque/coverage channel and is deliberately excluded from this choice.
 double Coverage[4] = {}, Mean[4] = {};
 const int32 BGRAOffset[] = {2, 1, 0, 3};
 const int64 Count = Pixels.Num() / 4;
 for (int64 Pixel = 0; Pixel < Count; ++Pixel) for (int32 Channel = 0; Channel < 4; ++Channel)
 {
  const uint8 Value = Pixels[Pixel * 4 + BGRAOffset[Channel]];
  Mean[Channel] += Value / 255.;
  Coverage[Channel] += Value >= 128 ? 1. : 0.;
 }
 int32 AccentChannel = INDEX_NONE, PlateChannel = INDEX_NONE;
 double AccentDistance = TNumericLimits<double>::Max(), PlateCoverage = -1.;
 TArray<FString> Report;
 for (int32 Channel = 0; Channel < 4; ++Channel)
 {
  Coverage[Channel] /= Count; Mean[Channel] /= Count;
  Report.Add(FString::Printf(TEXT("MASK channel=%c coverage_above_half=%.5f mean=%.5f"), TEXT("RGBA")[Channel], Coverage[Channel], Mean[Channel]));
  if (Channel < 3 && Coverage[Channel] > .025 && Coverage[Channel] < .8)
  {
   const double Distance = FMath::Abs(Coverage[Channel] - .14);
   if (Distance < AccentDistance) { AccentDistance = Distance; AccentChannel = Channel; }
   if (Coverage[Channel] > PlateCoverage) { PlateCoverage = Coverage[Channel]; PlateChannel = Channel; }
  }
 }
 if (AccentChannel == INDEX_NONE || PlateChannel == INDEX_NONE)
 {
  UE_LOG(LogVanthex, Error, TEXT("OPERATOR_ART no useful RGB mask regions found; material not changed"));
  return false;
 }
 Report.Add(FString::Printf(TEXT("SELECT accent=%c plate=%c texture=%s size=%dx%d"), TEXT("RGBA")[AccentChannel], TEXT("RGBA")[PlateChannel], *Mask->GetPathName(), Mask->Source.GetSizeX(), Mask->Source.GetSizeY()));

 const TCHAR* PackageName = TEXT("/Game/Vanthex/Operators/ARC/M_ARC_Armor");
 UMaterial* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/Vanthex/Operators/ARC/M_ARC_Armor.M_ARC_Armor"), nullptr, LOAD_NoWarn);
 UPackage* Package = Material ? Material->GetOutermost() : CreatePackage(PackageName);
 if (!Material) Material = NewObject<UMaterial>(Package, TEXT("M_ARC_Armor"), RF_Public | RF_Standalone | RF_Transactional);
 Material->PreEditChange(nullptr);
 Material->GetExpressionCollection().Empty();
 Material->MaterialDomain = MD_Surface;
 Material->BlendMode = BLEND_Opaque;
 Material->SetShadingModel(MSM_DefaultLit);
 Material->TwoSided = false;
 Material->bTangentSpaceNormal = true;
 Material->SetMaterialUsage(MATUSAGE_SkeletalMesh);

 auto Vector = [&](const TCHAR* Name, const FLinearColor& Value)
 {
  auto* Node = OperatorExpression<UMaterialExpressionVectorParameter>(Material);
  Node->ParameterName = Name; Node->DefaultValue = Value; return Node;
 };
 auto Scalar = [&](const TCHAR* Name, float Value)
 {
  auto* Node = OperatorExpression<UMaterialExpressionScalarParameter>(Material);
  Node->ParameterName = Name; Node->DefaultValue = Value; return Node;
 };
 auto Texture = [&](const TCHAR* Name, UTexture2D* Value)
 {
  auto* Node = OperatorExpression<UMaterialExpressionTextureSampleParameter2D>(Material);
  Node->ParameterName = Name; Node->Texture = Value; Node->AutoSetSampleType(); return Node;
 };
 auto* MaskSample = Texture(TEXT("ArmorMasks"), Mask);
 auto* NormalSample = Texture(TEXT("ArmorNormal"), Normal);
 auto Channel = [&](int32 Index)
 {
  auto* Node = OperatorExpression<UMaterialExpressionComponentMask>(Material);
  Node->R = Index == 0; Node->G = Index == 1; Node->B = Index == 2; Node->A = Index == 3;
  OperatorConnect(Node->Input, MaskSample); return Node;
 };
 auto* AccentMask = Channel(AccentChannel);
 auto* PlateMask = Channel(PlateChannel);
 auto* PlateColor = OperatorExpression<UMaterialExpressionLinearInterpolate>(Material);
 OperatorConnect(PlateColor->A, Vector(TEXT("UndersuitColor"), FLinearColor(.025f, .033f, .045f)));
 OperatorConnect(PlateColor->B, Vector(TEXT("ArmorColor"), FLinearColor(.22f, .28f, .36f)));
 OperatorConnect(PlateColor->Alpha, PlateMask);
 auto* AccentStrength = OperatorExpression<UMaterialExpressionMultiply>(Material);
 OperatorConnect(AccentStrength->A, AccentMask);
 OperatorConnect(AccentStrength->B, Scalar(TEXT("AccentStrength"), .72f));
 auto* TeamColor = Vector(TEXT("TeamColor"), FLinearColor(.02f, .38f, .8f));
 auto* Color = OperatorExpression<UMaterialExpressionLinearInterpolate>(Material);
 OperatorConnect(Color->A, PlateColor); OperatorConnect(Color->B, TeamColor); OperatorConnect(Color->Alpha, AccentStrength);
 auto* Roughness = OperatorExpression<UMaterialExpressionLinearInterpolate>(Material);
 Roughness->ConstA = .79f; Roughness->ConstB = .58f; OperatorConnect(Roughness->Alpha, PlateMask);
 auto* Metallic = OperatorExpression<UMaterialExpressionLinearInterpolate>(Material);
 Metallic->ConstA = .08f; Metallic->ConstB = .22f; OperatorConnect(Metallic->Alpha, PlateMask);
 auto* Emissive = OperatorExpression<UMaterialExpressionMultiply>(Material);
 OperatorConnect(Emissive->A, TeamColor); OperatorConnect(Emissive->B, AccentMask);
 auto* Glow = OperatorExpression<UMaterialExpressionMultiply>(Material);
 OperatorConnect(Glow->A, Emissive); OperatorConnect(Glow->B, Scalar(TEXT("AccentGlow"), .035f));
 auto* Editor = Material->GetEditorOnlyData();
 OperatorConnect(Editor->BaseColor, Color);
 OperatorConnect(Editor->Normal, NormalSample);
 OperatorConnect(Editor->Roughness, Roughness);
 OperatorConnect(Editor->Metallic, Metallic);
 OperatorConnect(Editor->EmissiveColor, Glow);
 Material->PostEditChange();
 Material->ForceRecompileForRendering();
 Package->MarkPackageDirty();
 const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
 FSavePackageArgs Args;
 Args.TopLevelFlags = RF_Public | RF_Standalone;
 const bool Saved = UPackage::SavePackage(Package, Material, *Filename, Args);
 Report.Add(FString::Printf(TEXT("%s material=%s normal=%s"), Saved ? TEXT("PASS") : TEXT("FAIL"), *Material->GetPathName(), *Normal->GetPathName()));
 FFileHelper::SaveStringArrayToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("OperatorArt.txt")));
 for (const FString& Line : Report) UE_LOG(LogVanthex, Display, TEXT("OPERATOR_ART %s"), *Line);
 return Saved;
}
#endif
