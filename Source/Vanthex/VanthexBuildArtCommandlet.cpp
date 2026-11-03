#include "VanthexBuildArtCommandlet.h"

#include "Vanthex.h"
#include "VanthexBuildKit.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#endif

UVanthexBuildArtCommandlet::UVanthexBuildArtCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

#if WITH_EDITOR
namespace
{
template <typename T>
T* AddExpr(UMaterial* Material)
{
	T* Expression = NewObject<T>(Material);
	Material->GetExpressionCollection().AddExpression(Expression);
	return Expression;
}

void Connect(FExpressionInput& Input, UMaterialExpression* Expression, int32 OutputIndex = 0)
{
	Input.Expression = Expression;
	Input.OutputIndex = OutputIndex;
	if (!Expression)
	{
		return;
	}
	const TArray<FExpressionOutput> Outputs = Expression->GetOutputs();
	if (Outputs.IsValidIndex(OutputIndex))
	{
		Input.Mask = Outputs[OutputIndex].Mask;
		Input.MaskR = Outputs[OutputIndex].MaskR;
		Input.MaskG = Outputs[OutputIndex].MaskG;
		Input.MaskB = Outputs[OutputIndex].MaskB;
		Input.MaskA = Outputs[OutputIndex].MaskA;
	}
}

bool SaveMaterial(UPackage* Package, UMaterial* Material)
{
	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	Material->ForceRecompileForRendering();
	Package->MarkPackageDirty();

	const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	const bool bSaved = UPackage::SavePackage(Package, Material, *Filename, Args);
	UE_LOG(LogVanthex, Display, TEXT("Art material %s saved %d"), *Material->GetName(), bSaved ? 1 : 0);
	return bSaved;
}

UMaterial* CreateMaterial(const TCHAR* PackageName, const TCHAR* AssetName)
{
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	IFileManager::Get().Delete(*Filename, false, true, true);
	UPackage* Package = CreatePackage(PackageName);
	if (UObject* Existing = FindObject<UObject>(Package, AssetName))
	{
		Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		Existing->ClearFlags(RF_Standalone | RF_Public);
	}
	UMaterial* Material = NewObject<UMaterial>(Package, AssetName, RF_Public | RF_Standalone | RF_Transactional);
	Material->MaterialDomain = MD_Surface;
	Material->BlendMode = BLEND_Opaque;
	Material->SetShadingModel(MSM_DefaultLit);
	Material->TwoSided = false;
	return Material;
}

bool BuildSurface()
{
	UMaterial* Material = CreateMaterial(TEXT("/Game/Vanthex/Materials/M_VanthexSurface"), TEXT("M_VanthexSurface"));
	if (!Material)
	{
		return true;
	}

	Material->bTangentSpaceNormal = false;
	Material->bUsedWithInstancedStaticMeshes = true;
	Material->bUsedWithNanite = true;

	auto Scalar = [&](const TCHAR* Name, float Value)
	{
		UMaterialExpressionScalarParameter* Parameter = AddExpr<UMaterialExpressionScalarParameter>(Material);
		Parameter->ParameterName = Name;
		Parameter->DefaultValue = Value;
		return Parameter;
	};
	auto Vector = [&](const TCHAR* Name, const FLinearColor& Value)
	{
		UMaterialExpressionVectorParameter* Parameter = AddExpr<UMaterialExpressionVectorParameter>(Material);
		Parameter->ParameterName = Name;
		Parameter->DefaultValue = Value;
		return Parameter;
	};

	UMaterialExpressionVectorParameter* BaseColor = Vector(TEXT("BaseColor"), FLinearColor(0.42f, 0.41f, 0.39f));
	UMaterialExpressionScalarParameter* Roughness = Scalar(TEXT("Roughness"), 0.78f);
	UMaterialExpressionScalarParameter* Metallic = Scalar(TEXT("Metallic"), 0.f);
	UMaterialExpressionScalarParameter* Panel = Scalar(TEXT("Panel"), 180.f);
	UMaterialExpressionScalarParameter* Relief = Scalar(TEXT("Relief"), 1.f);
	UMaterialExpressionScalarParameter* Wear = Scalar(TEXT("Wear"), 0.45f);
	UMaterialExpressionScalarParameter* Style = Scalar(TEXT("Style"), 0.f);
	UMaterialExpressionWorldPosition* WorldPos = AddExpr<UMaterialExpressionWorldPosition>(Material);
	UMaterialExpressionVertexNormalWS* WorldNormal = AddExpr<UMaterialExpressionVertexNormalWS>(Material);

	UMaterialExpressionCustom* Surface = AddExpr<UMaterialExpressionCustom>(Material);
	Surface->Description = TEXT("VanthexSurface");
	Surface->OutputType = CMOT_Float3;
	Surface->ContainsClipInstruction = CMCI_No;
	Surface->Inputs.Reset();
	Surface->Code = TEXT(R"(
float3 n = normalize(WorldN);
float3 an = abs(n);
float2 uv = WorldPos.xy;
float3 tangent = float3(1, 0, 0);
float3 bitangent = float3(0, 1, 0);
if (an.z >= an.x && an.z >= an.y)
{
	uv = WorldPos.xy;
	tangent = float3(1, 0, 0);
	bitangent = float3(0, 1, 0);
}
else if (an.x >= an.y)
{
	uv = float2(WorldPos.z, WorldPos.y);
	tangent = float3(0, 0, 1);
	bitangent = float3(0, 1, 0);
}
else
{
	uv = float2(WorldPos.x, WorldPos.z);
	tangent = float3(1, 0, 0);
	bitangent = float3(0, 0, 1);
}

float size = max(Panel, 8.0);
float eps = size * 0.02;
float2 u0 = uv / size;
float2 ux = (uv + float2(eps, 0)) / size;
float2 uy = (uv + float2(0, eps)) / size;
float2 f0 = frac(u0);
float2 fx = frac(ux);
float2 fy = frac(uy);
float e0 = min(min(f0.x, 1.0 - f0.x), min(f0.y, 1.0 - f0.y));
float ex = min(min(fx.x, 1.0 - fx.x), min(fx.y, 1.0 - fx.y));
float ey = min(min(fy.x, 1.0 - fy.x), min(fy.y, 1.0 - fy.y));
float h0 = 1.0 - smoothstep(0.0, 0.05, e0);
float hx = 1.0 - smoothstep(0.0, 0.05, ex);
float hy = 1.0 - smoothstep(0.0, 0.05, ey);
if (Style > 1.5 && Style < 2.5)
{
	h0 = sin(uv.y * 0.45);
	hx = sin(uv.y * 0.45);
	hy = sin((uv.y + eps) * 0.45);
}
else if (Style > 3.5)
{
	h0 = frac(sin(dot(floor(u0), float2(127.1, 311.7))) * 43758.5453);
	hx = h0;
	hy = h0;
}
float relief = Relief * 1.6;
float3 shaped = normalize(n - tangent * ((hx - h0) / eps) * relief - bitangent * ((hy - h0) / eps) * relief);
float seam = 1.0 - smoothstep(0.0, 0.05, e0);
float grain = frac(sin(dot(floor(u0), float2(127.1, 311.7))) * 43758.5453);
float cell = lerp(0.9, 1.06, grain);
Albedo = BaseColor * cell * (1.0 - seam * Wear * 0.35);
Rough = saturate(Roughness + seam * 0.22 * max(Wear, 0.15));
return shaped;
)");

	auto AddCustomInput = [&](const TCHAR* Name)
	{
		FCustomInput Input;
		Input.InputName = Name;
		Surface->Inputs.Add(Input);
	};
	AddCustomInput(TEXT("WorldPos"));
	AddCustomInput(TEXT("WorldN"));
	AddCustomInput(TEXT("BaseColor"));
	AddCustomInput(TEXT("Roughness"));
	AddCustomInput(TEXT("Panel"));
	AddCustomInput(TEXT("Relief"));
	AddCustomInput(TEXT("Wear"));
	AddCustomInput(TEXT("Style"));
	Connect(Surface->Inputs[0].Input, WorldPos);
	Connect(Surface->Inputs[1].Input, WorldNormal);
	Connect(Surface->Inputs[2].Input, BaseColor);
	Connect(Surface->Inputs[3].Input, Roughness);
	Connect(Surface->Inputs[4].Input, Panel);
	Connect(Surface->Inputs[5].Input, Relief);
	Connect(Surface->Inputs[6].Input, Wear);
	Connect(Surface->Inputs[7].Input, Style);

	FCustomOutput AlbedoOut;
	AlbedoOut.OutputName = TEXT("Albedo");
	AlbedoOut.OutputType = CMOT_Float3;
	FCustomOutput RoughOut;
	RoughOut.OutputName = TEXT("Rough");
	RoughOut.OutputType = CMOT_Float1;
	Surface->AdditionalOutputs.Add(AlbedoOut);
	Surface->AdditionalOutputs.Add(RoughOut);
	Surface->RebuildOutputs();

	UMaterialExpressionVectorParameter* EmissiveColor = Vector(TEXT("EmissiveColor"), FLinearColor::Black);
	UMaterialExpressionScalarParameter* EmissiveStrength = Scalar(TEXT("EmissiveStrength"), 0.f);
	UMaterialExpressionMultiply* Emissive = AddExpr<UMaterialExpressionMultiply>(Material);
	Connect(Emissive->A, EmissiveColor);
	Connect(Emissive->B, EmissiveStrength);
	UMaterialExpressionConstant* Specular = AddExpr<UMaterialExpressionConstant>(Material);
	Specular->R = 0.28f;

	UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
	Connect(EditorOnly->BaseColor, Surface, 1);
	Connect(EditorOnly->Roughness, Surface, 2);
	Connect(EditorOnly->Normal, Surface, 0);
	Connect(EditorOnly->Metallic, Metallic);
	Connect(EditorOnly->Specular, Specular);
	Connect(EditorOnly->EmissiveColor, Emissive);
	return SaveMaterial(Material->GetPackage(), Material);
}

bool BuildDecal()
{
	UMaterial* Material = CreateMaterial(TEXT("/Game/Vanthex/Materials/M_VanthexDecal"), TEXT("M_VanthexDecal"));
	if (!Material)
	{
		return true;
	}

	Material->MaterialDomain = MD_DeferredDecal;
	Material->BlendMode = BLEND_Translucent;
	Material->DecalBlendMode = DBM_Translucent;

	UMaterialExpressionVectorParameter* Color = AddExpr<UMaterialExpressionVectorParameter>(Material);
	Color->ParameterName = TEXT("Color");
	Color->DefaultValue = FLinearColor(0.72f, 0.58f, 0.12f);
	UMaterialExpressionScalarParameter* Opacity = AddExpr<UMaterialExpressionScalarParameter>(Material);
	Opacity->ParameterName = TEXT("Opacity");
	Opacity->DefaultValue = 0.85f;
	UMaterialExpressionScalarParameter* Stripe = AddExpr<UMaterialExpressionScalarParameter>(Material);
	Stripe->ParameterName = TEXT("Stripe");
	Stripe->DefaultValue = 0.f;
	UMaterialExpressionWorldPosition* WorldPos = AddExpr<UMaterialExpressionWorldPosition>(Material);
	UMaterialExpressionCustom* Mask = AddExpr<UMaterialExpressionCustom>(Material);
	Mask->Description = TEXT("VanthexDecal");
	Mask->OutputType = CMOT_Float1;
	Mask->ContainsClipInstruction = CMCI_No;
	Mask->Inputs.Reset();
	Mask->Code = TEXT(R"(
float2 uv = WorldPos.xy * 0.05;
float band = step(0.5, frac(uv.x));
float mask = lerp(1.0, band, saturate(Stripe));
return Opacity * mask;
)");
	FCustomInput WorldInput;
	WorldInput.InputName = TEXT("WorldPos");
	Mask->Inputs.Add(WorldInput);
	FCustomInput OpacityInput;
	OpacityInput.InputName = TEXT("Opacity");
	Mask->Inputs.Add(OpacityInput);
	FCustomInput StripeInput;
	StripeInput.InputName = TEXT("Stripe");
	Mask->Inputs.Add(StripeInput);
	Connect(Mask->Inputs[0].Input, WorldPos);
	Connect(Mask->Inputs[1].Input, Opacity);
	Connect(Mask->Inputs[2].Input, Stripe);
	Mask->RebuildOutputs();

	UMaterialExpressionConstant* Roughness = AddExpr<UMaterialExpressionConstant>(Material);
	Roughness->R = 0.62f;
	UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
	Connect(EditorOnly->BaseColor, Color);
	Connect(EditorOnly->Opacity, Mask);
	Connect(EditorOnly->Roughness, Roughness);
	return SaveMaterial(Material->GetPackage(), Material);
}

bool BuildGlass()
{
	UMaterial* Material = CreateMaterial(TEXT("/Game/Vanthex/Materials/M_VanthexGlass"), TEXT("M_VanthexGlass"));
	if (!Material)
	{
		return true;
	}

	Material->BlendMode = BLEND_Translucent;
	Material->TwoSided = true;
	Material->bUsedWithInstancedStaticMeshes = true;
	Material->bUsedWithNanite = true;
	UMaterialExpressionVectorParameter* Color = AddExpr<UMaterialExpressionVectorParameter>(Material);
	Color->ParameterName = TEXT("BaseColor");
	Color->DefaultValue = FLinearColor(0.55f, 0.64f, 0.68f);
	UMaterialExpressionConstant* Roughness = AddExpr<UMaterialExpressionConstant>(Material);
	Roughness->R = 0.06f;
	UMaterialExpressionConstant* Opacity = AddExpr<UMaterialExpressionConstant>(Material);
	Opacity->R = 0.22f;
	UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
	Connect(EditorOnly->BaseColor, Color);
	Connect(EditorOnly->Roughness, Roughness);
	Connect(EditorOnly->Opacity, Opacity);
	return SaveMaterial(Material->GetPackage(), Material);
}

bool BuildEnergy()
{
	UMaterial* Material = CreateMaterial(TEXT("/Game/Vanthex/Materials/M_VanthexEnergy"), TEXT("M_VanthexEnergy"));
	if (!Material)
	{
		return true;
	}

	Material->SetShadingModel(MSM_Unlit);
	Material->bUsedWithInstancedStaticMeshes = true;
	Material->bUsedWithNanite = true;

	UMaterialExpressionVectorParameter* Color = AddExpr<UMaterialExpressionVectorParameter>(Material);
	Color->ParameterName = TEXT("Color");
	Color->DefaultValue = FLinearColor(0.25f, 0.85f, 1.f);
	UMaterialExpressionScalarParameter* Strength = AddExpr<UMaterialExpressionScalarParameter>(Material);
	Strength->ParameterName = TEXT("Strength");
	Strength->DefaultValue = 4.f;

	UMaterialExpressionTime* Time = AddExpr<UMaterialExpressionTime>(Material);
	UMaterialExpressionConstant* Speed = AddExpr<UMaterialExpressionConstant>(Material);
	Speed->R = 2.4f;
	UMaterialExpressionMultiply* Phase = AddExpr<UMaterialExpressionMultiply>(Material);
	Connect(Phase->A, Time);
	Connect(Phase->B, Speed);
	UMaterialExpressionSine* Pulse = AddExpr<UMaterialExpressionSine>(Material);
	Connect(Pulse->Input, Phase);
	UMaterialExpressionConstant* PulseAmount = AddExpr<UMaterialExpressionConstant>(Material);
	PulseAmount->R = 0.18f;
	UMaterialExpressionMultiply* PulseScaled = AddExpr<UMaterialExpressionMultiply>(Material);
	Connect(PulseScaled->A, Pulse);
	Connect(PulseScaled->B, PulseAmount);
	UMaterialExpressionAdd* Brightness = AddExpr<UMaterialExpressionAdd>(Material);
	Connect(Brightness->A, Strength);
	Connect(Brightness->B, PulseScaled);
	UMaterialExpressionMultiply* Emissive = AddExpr<UMaterialExpressionMultiply>(Material);
	Connect(Emissive->A, Color);
	Connect(Emissive->B, Brightness);

	UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
	Connect(EditorOnly->EmissiveColor, Emissive);
	Connect(EditorOnly->BaseColor, Color);
	return SaveMaterial(Material->GetPackage(), Material);
}

bool BuildSign()
{
	UMaterial* Material = CreateMaterial(TEXT("/Game/Vanthex/Materials/M_VanthexSign"), TEXT("M_VanthexSign"));
	if (!Material)
	{
		return true;
	}

	Material->SetShadingModel(MSM_Unlit);
	Material->TwoSided = true;

	UMaterialExpressionTextureSampleParameter2D* Mark = AddExpr<UMaterialExpressionTextureSampleParameter2D>(Material);
	Mark->ParameterName = TEXT("Mark");
	Mark->ExpressionGUID = FGuid::NewGuid();
	Mark->Texture = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
	Mark->SamplerType = SAMPLERTYPE_Color;

	UMaterialExpressionVectorParameter* Color = AddExpr<UMaterialExpressionVectorParameter>(Material);
	Color->ParameterName = TEXT("Color");
	Color->DefaultValue = FLinearColor::White;
	UMaterialExpressionMultiply* Lit = AddExpr<UMaterialExpressionMultiply>(Material);
	Connect(Lit->A, Mark);
	Connect(Lit->B, Color);

	UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
	Connect(EditorOnly->EmissiveColor, Lit);
	Connect(EditorOnly->BaseColor, Lit);
	return SaveMaterial(Material->GetPackage(), Material);
}
}
#endif

int32 UVanthexBuildArtCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	extern bool VanthexBakeFoundry(bool bGridV3);
	if(Params.Contains(TEXT("InspectStairs"))) { extern bool VanthexInspectStairs(); return VanthexInspectStairs()?0:1; }
	if(Params.Contains(TEXT("InspectOperator"))) { extern bool VanthexInspectOperator(); return VanthexInspectOperator()?0:1; }
	if(Params.Contains(TEXT("BuildOperatorArt"))) { extern bool VanthexBuildOperatorArt(); return VanthexBuildOperatorArt()?0:1; }
	if(Params.Contains(TEXT("BakeGridV3"))) return VanthexBakeFoundry(true)?0:1;
	if(Params.Contains(TEXT("BakeFoundry"))) return VanthexBakeFoundry(false)?0:1;
	if(Params.Contains(TEXT("PrepareFactory"))) {
  TArray<FString> Paths; TSet<UMaterialInterface*> Materials; TSet<UMaterial*> Bases;
  FFileHelper::LoadFileToStringArray(Paths,*(FPaths::ProjectSavedDir()/TEXT("FactoryPaths.txt")));
  for(const FString& Path:Paths) if(auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path))
   for(const auto& Slot:Mesh->GetStaticMaterials()) if(auto* M=Slot.MaterialInterface.Get()) { Materials.Add(M); Bases.Add(M->GetMaterial()); }
  for(auto* M:Bases) { M->SetMaterialUsage(MATUSAGE_InstancedStaticMeshes); M->PostEditChange(); Materials.Add(M); }
  if(auto* Ground=LoadObject<UMaterialInstanceConstant>(nullptr,TEXT("/Game/Materials/MI_GrayFloor01.MI_GrayFloor01"))) {
   Ground->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("roughness")),0.72f); Materials.Add(Ground);
  }
  bool OK=true;
  for(auto* M:Materials) {
   M->PostEditChange(); auto* Package=M->GetPackage(); Package->MarkPackageDirty();
   FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
   OK &= UPackage::SavePackage(Package,M,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Args);
  }
  UE_LOG(LogVanthex,Display,TEXT("Prepared %d factory materials (%d masters) for instancing"),Materials.Num(),Bases.Num()); return OK?0:1;
 }
	if(Params.Contains(TEXT("FactoryInventory"))) {
  TArray<FString> Paths,Rows;
  FFileHelper::LoadFileToStringArray(Paths,*(FPaths::ProjectSavedDir()/TEXT("FactoryPaths.txt")));
  for(const FString& Path:Paths) if(auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path)) {
   const FBox B=Mesh->GetBoundingBox();
   Rows.Add(FString::Printf(TEXT("%s\t%s\t%s\t%s"),*Mesh->GetName(),*Path,*B.Min.ToString(),*B.Max.ToString()));
  }
  FFileHelper::SaveStringArrayToFile(Rows,*(FPaths::ProjectSavedDir()/TEXT("FactoryBounds.tsv"))); return Rows.Num()==Paths.Num()?0:1;
 }
	const bool bSurface = BuildSurface();
	const bool bEnergy = BuildEnergy();
	const bool bSign = BuildSign();
	const bool bDecal = BuildDecal();
	const bool bGlass = BuildGlass();
	const bool bKit = VanthexBuildKitMeshes();
	return (bSurface && bEnergy && bSign && bDecal && bGlass && bKit) ? 0 : 1;
#else
	UE_LOG(LogVanthex, Warning, TEXT("VanthexBuildArt is editor-only"));
	return 1;
#endif
}
