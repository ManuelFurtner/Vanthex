#include "CoreMinimal.h"
#if WITH_EDITOR
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"

namespace {
template<class T>T* Expr(UMaterial* M){auto* E=NewObject<T>(M);M->GetExpressionCollection().AddExpression(E);return E;}
void Link(FExpressionInput& Input,UMaterialExpression* E,int32 Output=0){Input.Expression=E;Input.OutputIndex=Output;}
}

UMaterial* VanthexBuildRefineryMaterial(UTexture2D* ColorTexture,UTexture2D* NormalTexture,UTexture2D* MaskTexture)
{
 const FString Path=TEXT("/Game/Vanthex/Blender/Refinery/M_Industrial");
 auto* Package=CreatePackage(*Path);Package->FullyLoad();
 if(auto* Old=FindObject<UObject>(Package,TEXT("M_Industrial"))){Old->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);Old->ClearFlags(RF_Public|RF_Standalone);}
 auto* M=NewObject<UMaterial>(Package,TEXT("M_Industrial"),RF_Public|RF_Standalone);
 M->bTangentSpaceNormal=false;M->SetMaterialUsage(MATUSAGE_InstancedStaticMeshes);M->SetMaterialUsage(MATUSAGE_Nanite);
 auto* P=Expr<UMaterialExpressionWorldPosition>(M);auto* N=Expr<UMaterialExpressionVertexNormalWS>(M);
 auto Scalar=[&](const TCHAR* Name,float Value){auto* E=Expr<UMaterialExpressionScalarParameter>(M);E->ParameterName=Name;E->DefaultValue=Value;return E;};
 auto* Tint=Expr<UMaterialExpressionVectorParameter>(M);Tint->ParameterName=TEXT("Tint");Tint->DefaultValue=FLinearColor(.4f,.42f,.37f);
 auto* Tile=Scalar(TEXT("TileCm"),200);auto* Rough=Scalar(TEXT("Roughness"),.8f);auto* Metal=Scalar(TEXT("Metallic"),0);
 auto* Coated=Scalar(TEXT("Coated"),0);auto* Glow=Scalar(TEXT("Glow"),0);
 auto* UV=Expr<UMaterialExpressionCustom>(M);UV->OutputType=CMOT_Float2;UV->ContainsClipInstruction=CMCI_No;
 UV->Code=TEXT("float3 a=abs(N); return (a.z>=a.x&&a.z>=a.y?P.xy:a.x>a.y?P.yz:P.xz)/max(Tile,1.0);");
 const TCHAR* UVNames[]={TEXT("P"),TEXT("N"),TEXT("Tile")};UMaterialExpression* UVValues[]={P,N,Tile};
 for(int32 I=0;I<3;++I){FCustomInput In;In.InputName=UVNames[I];Link(In.Input,UVValues[I]);UV->Inputs.Add(In);}
 auto Sample=[&](const TCHAR* Name,UTexture2D* Texture,EMaterialSamplerType Type){auto* E=Expr<UMaterialExpressionTextureSampleParameter2D>(M);E->ParameterName=Name;E->Texture=Texture;E->SamplerType=Type;Link(E->Coordinates,UV);return E;};
 auto* BC=Sample(TEXT("BaseMap"),ColorTexture,SAMPLERTYPE_Color);
 auto* NM=Sample(TEXT("NormalMap"),NormalTexture,SAMPLERTYPE_Normal);
 auto* ORM=Sample(TEXT("MaskMap"),MaskTexture,SAMPLERTYPE_Masks);
 auto* S=Expr<UMaterialExpressionCustom>(M);S->Description=TEXT("Baked PBR microstructure in physical world units");S->OutputType=CMOT_Float3;S->ContainsClipInstruction=CMCI_No;
 S->Code=TEXT(R"(
float3 n=normalize(N),a=abs(n);
float3 t=a.z>=a.x&&a.z>=a.y?float3(1,0,0):a.x>a.y?float3(0,1,0):float3(1,0,0);
float3 b=a.z>=a.x&&a.z>=a.y?float3(0,1,0):float3(0,0,1);
float chip=saturate(Mask.b)*Coated;
Albedo=lerp(Color*Tint,Color*float3(.26,.28,.25),chip);
Rough=saturate(lerp(Roughness+(Mask.g-.5)*.22,.52,chip));
Metal=lerp(Metallic*Mask.b,.88,chip);
return normalize(n+(t*Normal.x+b*Normal.y)*.65);
)");
 const TCHAR* Names[]={TEXT("N"),TEXT("Color"),TEXT("Normal"),TEXT("Mask"),TEXT("Tint"),TEXT("Roughness"),TEXT("Metallic"),TEXT("Coated")};
 UMaterialExpression* Values[]={N,BC,NM,ORM,Tint,Rough,Metal,Coated};
 for(int32 I=0;I<8;++I){FCustomInput In;In.InputName=Names[I];Link(In.Input,Values[I]);S->Inputs.Add(In);}
 for(const auto& Name:{TEXT("Albedo"),TEXT("Rough"),TEXT("Metal")}){FCustomOutput O;O.OutputName=Name;O.OutputType=FCString::Strcmp(Name,TEXT("Albedo"))==0?CMOT_Float3:CMOT_Float1;S->AdditionalOutputs.Add(O);}S->RebuildOutputs();
 auto* Em=Expr<UMaterialExpressionMultiply>(M);Link(Em->A,Tint);Link(Em->B,Glow);
 auto* E=M->GetEditorOnlyData();Link(E->BaseColor,S,1);Link(E->Normal,S);Link(E->Roughness,S,2);Link(E->Metallic,S,3);Link(E->AmbientOcclusion,ORM,1);Link(E->EmissiveColor,Em);
 M->PostEditChange();M->ForceRecompileForRendering();Package->MarkPackageDirty();
 const FString File=FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension());IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 return UPackage::SavePackage(Package,M,*File,Args)?M:nullptr;
}
#endif
