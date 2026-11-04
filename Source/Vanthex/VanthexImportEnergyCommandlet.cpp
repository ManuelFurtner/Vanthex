#include "VanthexImportEnergyCommandlet.h"
#include "Vanthex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/MemoryReader.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"
namespace {
const TCHAR* EnergyAssetRoot=TEXT("/Game/Vanthex/Imported/Core");
bool Save(UObject* Asset){
 auto* Package=Asset->GetPackage();Package->MarkPackageDirty();
 FString Filename=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 bool OK=UPackage::SavePackage(Package,Asset,*Filename,Args);if(OK)FAssetRegistryModule::AssetCreated(Asset);return OK;
}
template<typename T>T* Fresh(const FString& Folder,const FString& Name){
 FString Path=Folder/Name;
 auto* Package=FPackageName::DoesPackageExist(Path)?LoadPackage(nullptr,*Path,LOAD_None):CreatePackage(*Path);
 if(!Package)return nullptr;Package->FullyLoad();
 if(auto* Old=FindObject<UObject>(Package,*Name)){Old->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);Old->ClearFlags(RF_Public|RF_Standalone);}
 return NewObject<T>(Package,FName(Name),RF_Public|RF_Standalone);
}
template<typename T>T* Expr(UMaterial* M){auto* E=NewObject<T>(M);M->GetExpressionCollection().AddExpression(E);return E;}
FLinearColor Color(const TSharedPtr<FJsonObject>& Data,const TCHAR* Key){
 const auto& A=Data->GetArrayField(Key);return FLinearColor(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());
}
bool LoadMaterials(TArray<UMaterial*>& Materials){
 FString Json;if(!FFileHelper::LoadFileToString(Json,*(FPaths::ProjectSavedDir()/TEXT("EnergyImport/materials.json"))))return false;
 TSharedPtr<FJsonObject> Root;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root)||!Root)return false;
 const FString TextureDir=Root->GetStringField(TEXT("textures_dir"));
 for(const auto& Value:Root->GetArrayField(TEXT("materials"))){
  const auto Data=Value->AsObject();if(!Data)return false;
  auto* M=Fresh<UMaterial>(EnergyAssetRoot,TEXT("M_Energy_")+Data->GetStringField(TEXT("name")));if(!M)return false;
  auto* D=M->GetEditorOnlyData();M->SetShadingModel(MSM_DefaultLit);M->TwoSided=true;
  auto* Base=Expr<UMaterialExpressionConstant3Vector>(M);Base->Constant=Color(Data,TEXT("color"));D->BaseColor.Connect(0,Base);
  auto* Emission=Expr<UMaterialExpressionConstant3Vector>(M);Emission->Constant=Color(Data,TEXT("emission"));D->EmissiveColor.Connect(0,Emission);
  auto* Rough=Expr<UMaterialExpressionConstant>(M);Rough->R=Data->GetNumberField(TEXT("roughness"));D->Roughness.Connect(0,Rough);
  auto* Metal=Expr<UMaterialExpressionConstant>(M);Metal->R=Data->GetNumberField(TEXT("metallic"));D->Metallic.Connect(0,Metal);
  if(Data->GetBoolField(TEXT("glass"))){
   M->BlendMode=BLEND_Translucent;M->TranslucencyLightingMode=TLM_Surface;
   auto* Opacity=Expr<UMaterialExpressionConstant>(M);Opacity->R=.065f;D->Opacity.Connect(0,Opacity);
  }
  for(const auto& Pair:Data->GetObjectField(TEXT("textures"))->Values){
   const FString Source=TextureDir/Pair.Value->AsString();if(!IFileManager::Get().FileExists(*Source))return false;
   const FString Name=TEXT("T_Energy_")+FPaths::GetBaseFilename(Source);
   const FString PackageName=FString(EnergyAssetRoot)/Name;
   auto* Package=CreatePackage(*PackageName);Package->FullyLoad();
   if(auto* Old=FindObject<UObject>(Package,*Name)){Old->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);Old->ClearFlags(RF_Public|RF_Standalone);}
   auto* Factory=NewObject<UTextureFactory>();Factory->SuppressImportOverwriteDialog();bool Cancelled=false;
   auto* Texture=Cast<UTexture2D>(Factory->ImportObject(UTexture2D::StaticClass(),Package,FName(Name),RF_Public|RF_Standalone,Source,nullptr,Cancelled));
   if(!Texture||Cancelled)return false;
   bool Normal=Pair.Key==TEXT("normal"),ColorMap=Pair.Key==TEXT("color");
   Texture->SRGB=ColorMap;Texture->CompressionSettings=Normal?TC_Normalmap:ColorMap?TC_Default:TC_Masks;
   Texture->bFlipGreenChannel=Normal;Texture->MaxTextureSize=2048;Texture->PostEditChange();if(!Save(Texture))return false;
   auto* Sample=Expr<UMaterialExpressionTextureSample>(M);Sample->Texture=Texture;Sample->SamplerType=Normal?SAMPLERTYPE_Normal:ColorMap?SAMPLERTYPE_Color:SAMPLERTYPE_Masks;
   if(Normal)D->Normal.Connect(0,Sample);
   else if(ColorMap)D->BaseColor.Connect(0,Sample);
   else if(Pair.Key==TEXT("alpha")){M->BlendMode=BLEND_Masked;M->OpacityMaskClipValue=.35f;D->OpacityMask.Connect(1,Sample);}
   else if(Pair.Key==TEXT("ao"))D->AmbientOcclusion.Connect(1,Sample);
  }
  M->PreEditChange(nullptr);M->PostEditChange();M->ForceRecompileForRendering();if(!Save(M))return false;Materials.Add(M);
 }
 return !Materials.IsEmpty();
}
bool BuildMesh(const TArray<UMaterial*>& Mats,TArray<FString>& Report)
{
 const FString Source=FPaths::ProjectSavedDir()/TEXT("EnergyImport/energy.vxm");
 TArray<uint8> Bytes; if(!FFileHelper::LoadFileToArray(Bytes,*Source)) return false;
 FMemoryReader Reader(Bytes,true); uint32 Magic=0,Version=0,TriangleCount=0,MatCount=0;
 Reader<<Magic<<Version<<TriangleCount<<MatCount;
 if(Magic!=0x314d5856||Version!=1||MatCount!=Mats.Num()||TriangleCount==0||TriangleCount>1000000||Bytes.Num()!=16+int64(TriangleCount)*100) return false;
 const FString Name=TEXT("SM_EnergyCore");
 auto* Mesh=Fresh<UStaticMesh>(TEXT("/Game/Vanthex/Imported/Core"),Name);
 Mesh->SetNumSourceModels(3);
 auto* Description=Mesh->CreateMeshDescription(0);
 FStaticMeshAttributes Attributes(*Description); Attributes.Register();
 auto Normals=Attributes.GetVertexInstanceNormals(); auto UVs=Attributes.GetVertexInstanceUVs(); UVs.SetNumChannels(1);
 auto Tangents=Attributes.GetVertexInstanceTangents(); auto Signs=Attributes.GetVertexInstanceBinormalSigns();
 auto Colors=Attributes.GetVertexInstanceColors();
 TArray<FPolygonGroupID> Groups;
 TArray<FStaticMaterial> Slots;
 for(int32 I=0;I<Mats.Num();++I) {
  Groups.Add(Description->CreatePolygonGroup());
  Attributes.GetPolygonGroupMaterialSlotNames()[Groups.Last()]=FName(*Mats[I]->GetName());
  FStaticMaterial Slot(Mats[I],FName(*Mats[I]->GetName())); Slot.UVChannelData.bInitialized=true; Slots.Add(Slot);
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
 Mesh->GetSourceModel(2).ReductionSettings.PercentTriangles=.15f;
 Mesh->GetSourceModel(2).ScreenSize.Default=.06f;
 Mesh->Build(false);
 const bool Saved=Save(Mesh);
 Report.Add(FString::Printf(TEXT("%s %s triangles=%u lods=%d size=%s"),Saved?TEXT("PASS"):TEXT("FAIL"),*Name,TriangleCount,Mesh->GetNumLODs(),*Bounds.GetSize().ToString()));
 UE_LOG(LogVanthex,Display,TEXT("Energy model %s triangles %u saved %d"),*Name,TriangleCount,Saved);
 return Saved;
}
}
#endif
UVanthexImportEnergyCommandlet::UVanthexImportEnergyCommandlet(){IsClient=false;IsEditor=true;IsServer=false;LogToConsole=true;}
int32 UVanthexImportEnergyCommandlet::Main(const FString& Params){
#if WITH_EDITOR
 TArray<UMaterial*> Materials;TArray<FString> Report;
 bool OK=LoadMaterials(Materials)&&BuildMesh(Materials,Report);
 Report.Insert(OK?TEXT("PASS Energy capsule import (local-only licensed derivative)"):TEXT("FAIL Energy import"),0);
 FFileHelper::SaveStringToFile(FString::Join(Report,TEXT("\n")),*(FPaths::ProjectSavedDir()/TEXT("EnergyImport/import.txt")));
 return OK?0:1;
#else
 return 1;
#endif
}
