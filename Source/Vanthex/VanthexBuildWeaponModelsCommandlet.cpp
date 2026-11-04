#include "VanthexBuildWeaponModelsCommandlet.h"
#include "Vanthex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/MemoryReader.h"
#include "UObject/SavePackage.h"

namespace
{
const TCHAR* Materials[]={TEXT("Anodized"),TEXT("Steel"),TEXT("Silver"),TEXT("Polymer"),TEXT("Rubber"),TEXT("Olive"),TEXT("Walnut"),TEXT("Brass"),TEXT("Optic"),TEXT("Marking")};
const TCHAR* Weapons[]={TEXT("Pistol"),TEXT("P250"),TEXT("MP5"),TEXT("MP7"),TEXT("M4"),TEXT("AK"),TEXT("AUG"),TEXT("DMR"),TEXT("Shotgun"),TEXT("Knife")};

bool Save(UObject* Asset)
{
 auto* Package=Asset->GetPackage(); Package->MarkPackageDirty();
 const FString Filename=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
 FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
 const bool OK=UPackage::SavePackage(Package,Asset,*Filename,Args);
 if(OK) FAssetRegistryModule::AssetCreated(Asset);
 return OK;
}

template<typename T> T* Fresh(const FString& Folder,const FString& Name)
{
 const FString Path=Folder/Name;
 auto* Package=CreatePackage(*Path);
 // A repeat import must load the old exports before replacing them.
 Package->FullyLoad();
 if(auto* Existing=FindObject<UObject>(Package,*Name)) {
  Existing->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);
  Existing->ClearFlags(RF_Public|RF_Standalone);
 }
 return NewObject<T>(Package,FName(Name),RF_Public|RF_Standalone);
}
template<typename T> T* Expr(UMaterial* Material)
{
 auto* E=NewObject<T>(Material); Material->GetExpressionCollection().AddExpression(E); return E;
}

UMaterial* Material(int32 Index)
{
 const FLinearColor Colors[]={FLinearColor(.09,.11,.135),FLinearColor(.24,.28,.32),FLinearColor(.57,.61,.65),
  FLinearColor(.055,.067,.078),FLinearColor(.023,.029,.034),FLinearColor(.23,.28,.15),FLinearColor(.26,.09,.028),
  FLinearColor(.62,.43,.15),FLinearColor(.035,.22,.29),FLinearColor(.68,.71,.66)};
 const float Rough[]={.33f,.30f,.22f,.65f,.85f,.50f,.43f,.25f,.10f,.52f};
 const float Metal[]={.85f,1.f,1.f,0.f,0.f,.25f,0.f,1.f,.8f,0.f};
 auto* M=Fresh<UMaterial>(TEXT("/Game/Vanthex/Weapons/Materials"),FString(TEXT("M_WPN_"))+Materials[Index]);
 M->SetShadingModel(MSM_DefaultLit); M->TwoSided=false; M->bTangentSpaceNormal=true;
 auto* UV=Expr<UMaterialExpressionTextureCoordinate>(M);
 auto* Color=Expr<UMaterialExpressionConstant3Vector>(M); Color->Constant=Colors[Index];
 auto* Roughness=Expr<UMaterialExpressionConstant>(M); Roughness->R=Rough[Index];
 auto* Metallic=Expr<UMaterialExpressionConstant>(M); Metallic->R=Metal[Index];
 auto* Style=Expr<UMaterialExpressionConstant>(M); Style->R=Index==6?2.f:(Index==3||Index==4?1.f:0.f);
 auto* Detail=Expr<UMaterialExpressionCustom>(M);
 Detail->Description=TEXT("Original object-UV machining grain and polymer microtexture"); Detail->OutputType=CMOT_Float3;
 Detail->Code=TEXT(R"(
float2 p=UV*96.0;
float grain=frac(sin(dot(floor(p),float2(127.1,311.7)))*43758.5453);
float fade=1.0-saturate(max(fwidth(p.x),fwidth(p.y))*.7);
float g=(grain-.5)*fade;
float fine=sin(UV.x*720.0)*sin(UV.y*680.0)*fade;
float wood=sin(UV.x*18.0+sin(UV.y*3.2)*2.0+sin(UV.x*4.0));
float variation=Style>1.5 ? .82+.18*wood : 1.0+g*.075;
Rough=saturate(RoughBase+g*.10+(Style>1.5?wood*.045:0.0));
Normal=normalize(float3(g*(Style>.5?.12:.035),fine*(Style>.5?.06:.018),1.0));
return Base*variation;
)");
 Detail->Inputs.Reset();
 for(const auto* Name:{TEXT("UV"),TEXT("Base"),TEXT("RoughBase"),TEXT("Style")}) { FCustomInput In; In.InputName=Name; Detail->Inputs.Add(In); }
 Detail->Inputs[0].Input.Connect(0,UV); Detail->Inputs[1].Input.Connect(0,Color);
 Detail->Inputs[2].Input.Connect(0,Roughness); Detail->Inputs[3].Input.Connect(0,Style);
 FCustomOutput RoughOut; RoughOut.OutputName=TEXT("Rough"); RoughOut.OutputType=CMOT_Float1; Detail->AdditionalOutputs.Add(RoughOut);
 FCustomOutput NormalOut; NormalOut.OutputName=TEXT("Normal"); NormalOut.OutputType=CMOT_Float3; Detail->AdditionalOutputs.Add(NormalOut); Detail->RebuildOutputs();
 auto* Data=M->GetEditorOnlyData(); Data->BaseColor.Connect(0,Detail); Data->Roughness.Connect(1,Detail);
 Data->Normal.Connect(2,Detail); Data->Metallic.Connect(0,Metallic);
 if(Index==8) {
  // A lens must leave the scene visible through the actual reflex frame.
  M->BlendMode=BLEND_Translucent; M->TwoSided=true;
  auto* Opacity=Expr<UMaterialExpressionConstant>(M); Opacity->R=.045f;
  Data->Opacity.Connect(0,Opacity); Metallic->R=0;
 }
 M->PreEditChange(nullptr); M->PostEditChange(); M->ForceRecompileForRendering();
 return Save(M)?M:nullptr;
}

bool BuildMesh(const FString& Weapon,const FString& Part,const TArray<UMaterial*>& Mats,TArray<FString>& Report)
{
 const FString Source=FPaths::ProjectDir()/TEXT("SourceArt/Weapons")/(Weapon+TEXT("_")+Part+TEXT(".vxm"));
 TArray<uint8> Bytes; if(!FFileHelper::LoadFileToArray(Bytes,*Source)) return false;
 FMemoryReader Reader(Bytes,true); uint32 Magic=0,Version=0,TriangleCount=0,MatCount=0;
 Reader<<Magic<<Version<<TriangleCount<<MatCount;
 if(Magic!=0x314d5856||Version!=1||MatCount!=Mats.Num()||TriangleCount==0||TriangleCount>50000||Bytes.Num()!=16+int64(TriangleCount)*100) return false;
 const FString Name=TEXT("SM_")+Weapon+TEXT("_")+Part;
 auto* Mesh=Fresh<UStaticMesh>(TEXT("/Game/Vanthex/Weapons/Meshes"),Name);
 Mesh->SetNumSourceModels(2);
 auto* Description=Mesh->CreateMeshDescription(0);
 FStaticMeshAttributes Attributes(*Description); Attributes.Register();
 auto Normals=Attributes.GetVertexInstanceNormals(); auto UVs=Attributes.GetVertexInstanceUVs(); UVs.SetNumChannels(1);
 auto Tangents=Attributes.GetVertexInstanceTangents(); auto Signs=Attributes.GetVertexInstanceBinormalSigns();
 auto Colors=Attributes.GetVertexInstanceColors();
 TArray<FPolygonGroupID> Groups;
 TArray<FStaticMaterial> Slots;
 for(int32 I=0;I<Mats.Num();++I) {
  Groups.Add(Description->CreatePolygonGroup());
  Attributes.GetPolygonGroupMaterialSlotNames()[Groups.Last()]=FName(Materials[I]);
  FStaticMaterial Slot(Mats[I],FName(Materials[I])); Slot.UVChannelData.bInitialized=true; Slots.Add(Slot);
 }
 FBox Bounds(ForceInit); bool Valid=true;
 for(uint32 Triangle=0;Triangle<TriangleCount;++Triangle) {
  uint32 Mat=0; Reader<<Mat;
  if(Mat>=uint32(Mats.Num())) { Valid=false; break; }
  TArray<FVertexInstanceID> Instances;
  for(int32 Corner=0;Corner<3;++Corner) {
   FVector3f P,N; FVector2f UV;
   Reader<<P.X<<P.Y<<P.Z<<N.X<<N.Y<<N.Z<<UV.X<<UV.Y;
   if(P.ContainsNaN()||N.ContainsNaN()||!FMath::IsNearlyEqual(N.SizeSquared(),1.f,.01f)) Valid=false;
   const FVertexID V=Description->CreateVertex(); Description->GetVertexPositions()[V]=P;
   const auto VI=Description->CreateVertexInstance(V); Instances.Add(VI); Normals[VI]=N; UVs.Set(VI,0,UV);
   Colors[VI]=FVector4f(1,1,1,1); Signs[VI]=1;
   const FVector3f Ref=FMath::Abs(N.Z)>.9f?FVector3f(1,0,0):FVector3f(0,0,1);
   Tangents[VI]=FVector3f::CrossProduct(Ref,N).GetSafeNormal(); Bounds+=FVector(P);
  }
  Description->CreateTriangle(Groups[Mat],Instances);
 }
 if(!Valid||Reader.IsError()) return false;
 Mesh->SetStaticMaterials(Slots);
 Mesh->CommitMeshDescription(0);
 auto& Settings=Mesh->GetSourceModel(0).BuildSettings;
 Settings.bRecomputeNormals=false; Settings.bRecomputeTangents=true; Settings.bGenerateLightmapUVs=false; Settings.bRemoveDegenerates=true;
 Settings.bUseFullPrecisionUVs=true;
 Mesh->GetSourceModel(1).ReductionSettings.PercentTriangles=.45f;
 Mesh->GetSourceModel(1).ReductionSettings.BaseLODModel=0;
 Mesh->GetSourceModel(1).ScreenSize.Default=.18f;
 Mesh->CreateBodySetup(); Mesh->GetBodySetup()->DefaultInstance.SetCollisionProfileName(TEXT("NoCollision"));
 auto Nanite=Mesh->GetNaniteSettings(); Nanite.bEnabled=false; Mesh->SetNaniteSettings(Nanite);
 Mesh->Build(false); Mesh->PostEditChange();
 const bool Saved=Save(Mesh);
 Report.Add(FString::Printf(TEXT("%s %s triangles=%u lods=%d size=%s"),Saved?TEXT("PASS"):TEXT("FAIL"),*Name,TriangleCount,Mesh->GetNumLODs(),*Bounds.GetSize().ToString()));
 UE_LOG(LogVanthex,Display,TEXT("Weapon model %s triangles %u saved %d"),*Name,TriangleCount,Saved);
 return Saved;
}
}
#endif

UVanthexBuildWeaponModelsCommandlet::UVanthexBuildWeaponModelsCommandlet()
{ IsClient=false; IsEditor=true; IsServer=false; LogToConsole=true; }

int32 UVanthexBuildWeaponModelsCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
 bool OK=true; TArray<UMaterial*> Mats; TArray<FString> Report;
 for(int32 I=0;I<UE_ARRAY_COUNT(Materials);++I) { auto* Mat=Material(I); Mats.Add(Mat); OK&=Mat!=nullptr; }
 if(!OK) return 1;
 int32 Count=0;
 for(const TCHAR* Weapon:Weapons) for(const TCHAR* Part:{TEXT("Body"),TEXT("Magazine"),TEXT("Bolt")}) {
  if(FCString::Strcmp(Weapon,TEXT("Knife"))==0&&FCString::Strcmp(Part,TEXT("Body"))!=0) continue;
  const bool Built=BuildMesh(Weapon,Part,Mats,Report); OK&=Built; ++Count;
 }
 Report.Insert(FString::Printf(TEXT("OVERALL %s\nMESHES %d\nMATERIALS %d"),OK?TEXT("PASS"):TEXT("FAIL"),Count,Mats.Num()),0);
 FFileHelper::SaveStringArrayToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("WeaponModels-build.txt")));
 return OK?0:1;
#else
 return 1;
#endif
}
