#include "VanthexBuildKit.h"

#include "Vanthex.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "UObject/SavePackage.h"

namespace
{
FMeshDescription& PrepareKitMesh(FMeshDescription& Mesh)
{
	FStaticMeshAttributes Attributes(Mesh);
	Attributes.Register();
	return Mesh;
}

struct FKitBuilder
{
	FMeshDescription& Mesh;
	FPolygonGroupID GroupA;
	FPolygonGroupID GroupB;
	TVertexInstanceAttributesRef<FVector3f> Normals;
	TVertexInstanceAttributesRef<FVector2f> UVs;

	explicit FKitBuilder(FMeshDescription& InMesh)
		: Mesh(PrepareKitMesh(InMesh))
		, GroupA(Mesh.CreatePolygonGroup())
		, GroupB(Mesh.CreatePolygonGroup())
		, Normals(FStaticMeshAttributes(Mesh).GetVertexInstanceNormals())
		, UVs(FStaticMeshAttributes(Mesh).GetVertexInstanceUVs())
	{
	}

	void Quad(FPolygonGroupID Group, const FVector3f& A, const FVector3f& B, const FVector3f& C, const FVector3f& D, const FVector3f& Normal)
	{
		const FVertexID Vertices[4] = {Mesh.CreateVertex(), Mesh.CreateVertex(), Mesh.CreateVertex(), Mesh.CreateVertex()};
		const FVector3f Points[4] = {A, B, C, D};
		const FVector2f Tex[4] = {FVector2f(0.f, 0.f), FVector2f(1.f, 0.f), FVector2f(1.f, 1.f), FVector2f(0.f, 1.f)};
		FVertexInstanceID Instances[4];
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Mesh.GetVertexPositions()[Vertices[Index]] = Points[Index];
			Instances[Index] = Mesh.CreateVertexInstance(Vertices[Index]);
			Normals.Set(Instances[Index], Normal);
			UVs.Set(Instances[Index], 0, Tex[Index]);
		}
		Mesh.CreateTriangle(Group, {Instances[0], Instances[1], Instances[2]});
		Mesh.CreateTriangle(Group, {Instances[0], Instances[2], Instances[3]});
	}

	void Box(const FVector& Center, const FVector& Extent, int32 Slot, const FRotator& Rotation)
	{
		const FPolygonGroupID Group = Slot == 0 ? GroupA : GroupB;
		const FVector Corners[8] = {
			FVector(-1.f, -1.f, -1.f), FVector(1.f, -1.f, -1.f), FVector(1.f, 1.f, -1.f), FVector(-1.f, 1.f, -1.f),
			FVector(-1.f, -1.f, 1.f), FVector(1.f, -1.f, 1.f), FVector(1.f, 1.f, 1.f), FVector(-1.f, 1.f, 1.f)
		};
		FVector3f World[8];
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const FVector Point = Center + Rotation.RotateVector(Corners[Index] * Extent);
			World[Index] = FVector3f(Point);
		}

		auto Face = [&](int32 I0, int32 I1, int32 I2, int32 I3, const FVector& Normal)
		{
			Quad(Group, World[I0], World[I1], World[I2], World[I3], FVector3f(Rotation.RotateVector(Normal)));
		};

		Face(1, 2, 6, 5, FVector(1.f, 0.f, 0.f));
		Face(3, 0, 4, 7, FVector(-1.f, 0.f, 0.f));
		Face(2, 3, 7, 6, FVector(0.f, 1.f, 0.f));
		Face(0, 1, 5, 4, FVector(0.f, -1.f, 0.f));
		Face(4, 5, 6, 7, FVector(0.f, 0.f, 1.f));
		Face(3, 2, 1, 0, FVector(0.f, 0.f, -1.f));
	}

	void Box(const FVector& Center, const FVector& Extent, int32 Slot = 0)
	{
		Box(Center, Extent, Slot, FRotator::ZeroRotator);
	}
};

bool SaveKit(const TCHAR* Name, TFunctionRef<void(FKitBuilder&)> Fill)
{
	const FString PackageName = FString::Printf(TEXT("/Game/Vanthex/Meshes/%s"), Name);
	const FString ExistingFile = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	IFileManager::Get().Delete(*ExistingFile, false, true, true);
	UPackage* Package = CreatePackage(*PackageName);
	if (UObject* Existing = FindObject<UObject>(Package, Name))
	{
		Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		Existing->ClearFlags(RF_Standalone | RF_Public);
	}
	UStaticMesh* Mesh = NewObject<UStaticMesh>(Package, FName(Name), RF_Public | RF_Standalone);
	if (!Mesh)
	{
		return false;
	}

	Mesh->SetNumSourceModels(1);
	FMeshDescription* Description = Mesh->CreateMeshDescription(0);
	if (!Description)
	{
		return false;
	}

	{
		FKitBuilder Builder(*Description);
		Fill(Builder);
	}

	UStaticMesh::FCommitMeshDescriptionParams CommitParams;
	Mesh->CommitMeshDescription(0, CommitParams);

	FStaticMaterial Structure;
	Structure.MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
	Structure.MaterialSlotName = TEXT("Structure");
	Structure.UVChannelData.bInitialized = true;
	FStaticMaterial Emissive = Structure;
	Emissive.MaterialSlotName = TEXT("Emissive");
	Mesh->SetStaticMaterials({Structure, Emissive});

	FStaticMeshSourceModel& Model = Mesh->GetSourceModel(0);
	Model.BuildSettings.bRecomputeNormals = false;
	Model.BuildSettings.bRecomputeTangents = true;
	Model.BuildSettings.bGenerateLightmapUVs = false;
	Model.BuildSettings.bRemoveDegenerates = true;

	FMeshNaniteSettings Nanite = Mesh->GetNaniteSettings();
	Nanite.bEnabled = true;
	Mesh->SetNaniteSettings(Nanite);

	Mesh->CreateBodySetup();
	if (UBodySetup* Body = Mesh->GetBodySetup())
	{
		Body->DefaultInstance.SetCollisionProfileName(TEXT("NoCollision"));
		Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	}

	Mesh->Build(false);
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	const bool bSaved = UPackage::SavePackage(Package, Mesh, *Filename, Args);
	if (bSaved)
	{
		FAssetRegistryModule::AssetCreated(Mesh);
	}
	UE_LOG(LogVanthex, Display, TEXT("Kit mesh %s saved %d"), Name, bSaved ? 1 : 0);
	return bSaved;
}
}

bool VanthexBuildKitMeshes()
{
	bool bOk = true;

	bOk &= SaveKit(TEXT("SM_WallPanel"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(1.5f, 0.f, 0.f), FVector(1.5f, 100.f, 230.f), 0);
		Kit.Box(FVector(6.f, -94.f, 0.f), FVector(4.f, 6.f, 230.f), 0);
		Kit.Box(FVector(6.f, 94.f, 0.f), FVector(4.f, 6.f, 230.f), 0);
		Kit.Box(FVector(6.f, 0.f, 222.f), FVector(4.f, 88.f, 8.f), 0);
		Kit.Box(FVector(6.f, 0.f, -222.f), FVector(4.f, 88.f, 8.f), 0);
		Kit.Box(FVector(5.2f, 0.f, 40.f), FVector(2.4f, 88.f, 5.f), 0);
		Kit.Box(FVector(5.2f, 0.f, -70.f), FVector(2.4f, 88.f, 5.f), 0);
		Kit.Box(FVector(3.2f, 0.f, 120.f), FVector(1.2f, 80.f, 90.f), 0);
		Kit.Box(FVector(3.2f, 0.f, -40.f), FVector(1.2f, 80.f, 70.f), 0);
		Kit.Box(FVector(8.f, 78.f, 0.f), FVector(1.6f, 2.2f, 200.f), 0);
		for (int32 Slat = 0; Slat < 5; ++Slat)
		{
			Kit.Box(FVector(7.2f, 0.f, 70.f + Slat * 14.f), FVector(1.1f, 72.f, 2.2f), 0);
		}
		const FVector Bolts[4] = {FVector(9.f, -86.f, 210.f), FVector(9.f, 86.f, 210.f), FVector(9.f, -86.f, -210.f), FVector(9.f, 86.f, -210.f)};
		for (const FVector& Bolt : Bolts)
		{
			Kit.Box(Bolt, FVector(0.7f, 1.3f, 1.3f), 1);
		}
	});

	bOk &= SaveKit(TEXT("SM_WallPanelB"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(1.5f, 0.f, 0.f), FVector(1.5f, 100.f, 230.f), 0);
		Kit.Box(FVector(6.f, -94.f, 0.f), FVector(4.f, 6.f, 230.f), 0);
		Kit.Box(FVector(6.f, 94.f, 0.f), FVector(4.f, 6.f, 230.f), 0);
		Kit.Box(FVector(6.f, 0.f, 222.f), FVector(4.f, 88.f, 8.f), 0);
		Kit.Box(FVector(6.f, 0.f, -222.f), FVector(4.f, 88.f, 8.f), 0);
		Kit.Box(FVector(7.4f, 0.f, 20.f), FVector(2.f, 36.f, 150.f), 0);
		Kit.Box(FVector(4.f, -40.f, -20.f), FVector(1.4f, 40.f, 160.f), 0);
		for (int32 Rib = 0; Rib < 6; ++Rib)
		{
			Kit.Box(FVector(8.2f, -20.f, -150.f + Rib * 52.f), FVector(1.f, 28.f, 2.f), 0);
		}
		Kit.Box(FVector(9.f, 70.f, 160.f), FVector(1.2f, 8.f, 8.f), 1);
	});

	bOk &= SaveKit(TEXT("SM_Column"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 150.f), FVector(14.f, 14.f, 150.f), 0);
		Kit.Box(FVector(16.f, 0.f, 150.f), FVector(2.f, 18.f, 150.f), 0);
		Kit.Box(FVector(-16.f, 0.f, 150.f), FVector(2.f, 18.f, 150.f), 0);
		Kit.Box(FVector(0.f, 0.f, 292.f), FVector(20.f, 20.f, 6.f), 0);
		Kit.Box(FVector(0.f, 0.f, 8.f), FVector(20.f, 20.f, 8.f), 0);
	});

	bOk &= SaveKit(TEXT("SM_Beam"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 7.f), FVector(50.f, 8.f, 1.4f), 0);
		Kit.Box(FVector(0.f, 0.f, -7.f), FVector(50.f, 8.f, 1.4f), 0);
		Kit.Box(FVector(0.f, 0.f, 0.f), FVector(50.f, 1.5f, 7.f), 0);
	});

	bOk &= SaveKit(TEXT("SM_Light"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 0.f), FVector(80.f, 16.f, 6.f), 0);
		Kit.Box(FVector(0.f, 0.f, -5.f), FVector(68.f, 8.f, 1.5f), 1);
		Kit.Box(FVector(-70.f, 0.f, 8.f), FVector(2.f, 2.f, 10.f), 0);
		Kit.Box(FVector(70.f, 0.f, 8.f), FVector(2.f, 2.f, 10.f), 0);
	});

	bOk &= SaveKit(TEXT("SM_Rack"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 100.f), FVector(28.f, 18.f, 100.f), 0);
		for (int32 Shelf = 0; Shelf < 5; ++Shelf)
		{
			const float Z = 24.f + Shelf * 36.f;
			Kit.Box(FVector(0.f, 0.f, Z), FVector(26.f, 16.f, 1.2f), 0);
		}
		Kit.Box(FVector(0.f, 19.f, 100.f), FVector(2.f, 0.6f, 92.f), 1);
	});

	bOk &= SaveKit(TEXT("SM_Cabinet"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 70.f), FVector(32.f, 16.f, 70.f), 0);
		Kit.Box(FVector(0.f, 14.f, 70.f), FVector(26.f, 1.5f, 58.f), 0);
		Kit.Box(FVector(22.f, 16.f, 110.f), FVector(4.f, 0.8f, 1.2f), 1);
		Kit.Box(FVector(-18.f, 16.f, 24.f), FVector(8.f, 1.f, 10.f), 0);
	});

	bOk &= SaveKit(TEXT("SM_Vent"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 0.f), FVector(46.f, 6.f, 28.f), 0);
		for (int32 Slat = 0; Slat < 4; ++Slat)
		{
			Kit.Box(FVector(0.f, 2.f, -16.f + Slat * 11.f), FVector(40.f, 2.f, 1.4f), 0);
		}
	});

	bOk &= SaveKit(TEXT("SM_Door"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(-62.f, 0.f, 110.f), FVector(8.f, 6.f, 110.f), 0);
		Kit.Box(FVector(62.f, 0.f, 110.f), FVector(8.f, 6.f, 110.f), 0);
		Kit.Box(FVector(0.f, 0.f, 214.f), FVector(70.f, 6.f, 8.f), 0);
		Kit.Box(FVector(0.f, 1.f, 105.f), FVector(50.f, 3.f, 100.f), 0);
		Kit.Box(FVector(0.f, 5.f, 168.f), FVector(18.f, 1.f, 10.f), 1);
	});

	bOk &= SaveKit(TEXT("SM_Railing"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 50.f), FVector(2.f, 2.f, 50.f), 0);
		Kit.Box(FVector(0.f, 200.f, 50.f), FVector(2.f, 2.f, 50.f), 0);
		Kit.Box(FVector(0.f, 100.f, 96.f), FVector(1.6f, 100.f, 1.6f), 0);
		Kit.Box(FVector(0.f, 100.f, 52.f), FVector(1.2f, 100.f, 1.2f), 0);
		Kit.Box(FVector(0.f, 100.f, 8.f), FVector(1.f, 100.f, 6.f), 0);
	});

	bOk &= SaveKit(TEXT("SM_Crate"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 22.f), FVector(28.f, 18.f, 22.f), 0);
		Kit.Box(FVector(0.f, 0.f, 44.f), FVector(30.f, 20.f, 2.f), 0);
		Kit.Box(FVector(0.f, 0.f, 2.f), FVector(30.f, 20.f, 2.f), 0);
	});

	bOk &= SaveKit(TEXT("SM_Tray"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 0.f), FVector(100.f, 14.f, 1.f), 0);
		Kit.Box(FVector(0.f, 13.f, 3.f), FVector(100.f, 1.f, 3.f), 0);
		Kit.Box(FVector(0.f, -13.f, 3.f), FVector(100.f, 1.f, 3.f), 0);
	});

	bOk &= SaveKit(TEXT("SM_Pylon"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 4.f), FVector(16.f, 16.f, 4.f), 0);
		Kit.Box(FVector(0.f, 0.f, 78.f), FVector(7.f, 7.f, 70.f), 0);
		Kit.Box(FVector(0.f, 0.f, 154.f), FVector(12.f, 12.f, 8.f), 0);
		Kit.Box(FVector(0.f, 0.f, 166.f), FVector(6.f, 6.f, 4.f), 1);
	});

	bOk &= SaveKit(TEXT("SM_TransferBase"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 6.f), FVector(210.f, 210.f, 6.f), 0);
		Kit.Box(FVector(0.f, 0.f, 14.f), FVector(150.f, 150.f, 3.f), 0);
		Kit.Box(FVector(0.f, 0.f, 18.f), FVector(48.f, 48.f, 2.f), 1);
		Kit.Box(FVector(0.f, -120.f, 28.f), FVector(70.f, 16.f, 16.f), 0);
		Kit.Box(FVector(-90.f, -70.f, 70.f), FVector(10.f, 10.f, 48.f), 0);
		Kit.Box(FVector(90.f, -70.f, 70.f), FVector(10.f, 10.f, 48.f), 0);
		Kit.Box(FVector(-48.f, -36.f, 108.f), FVector(28.f, 8.f, 6.f), 0, FRotator(28.f, 24.f, 0.f));
		Kit.Box(FVector(48.f, -36.f, 108.f), FVector(28.f, 8.f, 6.f), 0, FRotator(28.f, -24.f, 0.f));
		Kit.Box(FVector(0.f, 20.f, 92.f), FVector(22.f, 8.f, 6.f), 0);
		Kit.Box(FVector(0.f, 130.f, 22.f), FVector(36.f, 14.f, 12.f), 0);
		Kit.Box(FVector(24.f, 146.f, 36.f), FVector(6.f, 2.f, 3.f), 1);
	});

	bOk &= SaveKit(TEXT("SM_CoreDevice"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, -24.f), FVector(20.f, 20.f, 3.5f), 0);
		Kit.Box(FVector(0.f, 0.f, 24.f), FVector(20.f, 20.f, 3.5f), 0);
		Kit.Box(FVector(17.f, 17.f, 0.f), FVector(2.4f, 2.4f, 24.f), 0);
		Kit.Box(FVector(17.f, -17.f, 0.f), FVector(2.4f, 2.4f, 24.f), 0);
		Kit.Box(FVector(-17.f, 17.f, 0.f), FVector(2.4f, 2.4f, 24.f), 0);
		Kit.Box(FVector(-17.f, -17.f, 0.f), FVector(2.4f, 2.4f, 24.f), 0);
		Kit.Box(FVector(0.f, 18.f, 0.f), FVector(12.f, 1.6f, 4.f), 0);
		Kit.Box(FVector(0.f, -18.f, 0.f), FVector(12.f, 1.6f, 4.f), 0);
		Kit.Box(FVector(18.f, 0.f, 0.f), FVector(1.6f, 12.f, 4.f), 0);
		Kit.Box(FVector(-18.f, 0.f, 0.f), FVector(1.6f, 12.f, 4.f), 0);
		Kit.Box(FVector(0.f, 24.f, -6.f), FVector(3.f, 4.f, 6.f), 0);
		Kit.Box(FVector(0.f, -24.f, -6.f), FVector(3.f, 4.f, 6.f), 0);
		Kit.Box(FVector(0.f, 0.f, 28.f), FVector(8.f, 8.f, 1.4f), 0);
		Kit.Box(FVector(14.f, 0.f, 26.f), FVector(1.4f, 1.4f, 1.4f), 1);
		Kit.Box(FVector(-14.f, 0.f, 26.f), FVector(1.4f, 1.4f, 1.4f), 1);
		Kit.Box(FVector(0.f, 14.f, -26.f), FVector(1.4f, 1.4f, 1.4f), 1);
		Kit.Box(FVector(0.f, -14.f, 26.f), FVector(1.4f, 1.4f, 1.4f), 1);
	});

	bOk &= SaveKit(TEXT("SM_Ring"), [](FKitBuilder& Kit)
	{
		constexpr int32 Segments = 16;
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const float Angle = Index * (360.f / Segments);
			const FVector Center(FMath::Cos(FMath::DegreesToRadians(Angle)) * 100.f, FMath::Sin(FMath::DegreesToRadians(Angle)) * 100.f, 0.f);
			Kit.Box(Center, FVector(22.f, 5.f, 4.f), 0, FRotator(0.f, Angle + 90.f, 0.f));
		}
	});

	bOk &= SaveKit(TEXT("SM_SignPlate"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 0.f), FVector(1.2f, 50.f, 16.f), 0);
		Kit.Box(FVector(1.4f, 0.f, 14.f), FVector(1.4f, 50.f, 2.f), 0);
		Kit.Box(FVector(1.4f, 0.f, -14.f), FVector(1.4f, 50.f, 2.f), 0);
		Kit.Box(FVector(1.4f, -48.f, 0.f), FVector(1.4f, 2.f, 14.f), 0);
		Kit.Box(FVector(1.4f, 48.f, 0.f), FVector(1.4f, 2.f, 14.f), 0);
	});

	bOk &= SaveKit(TEXT("SM_Fan"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 0.f), FVector(4.f, 4.f, 2.f), 0);
		for (int32 Blade = 0; Blade < 4; ++Blade)
		{
			const float Angle = Blade * 90.f;
			const FVector Center = FRotator(0.f, Angle, 0.f).RotateVector(FVector(16.f, 0.f, 0.f));
			Kit.Box(Center, FVector(14.f, 3.f, 0.6f), 0, FRotator(0.f, Angle, 0.f));
		}
	});

	bOk &= SaveKit(TEXT("SM_FloorPlate"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 1.f), FVector(86.f, 86.f, 1.f), 0);
		Kit.Box(FVector(0.f, 84.f, 2.2f), FVector(88.f, 3.f, 1.2f), 0);
		Kit.Box(FVector(0.f, -84.f, 2.2f), FVector(88.f, 3.f, 1.2f), 0);
		Kit.Box(FVector(84.f, 0.f, 2.2f), FVector(3.f, 84.f, 1.2f), 0);
		Kit.Box(FVector(-84.f, 0.f, 2.2f), FVector(3.f, 84.f, 1.2f), 0);
		Kit.Box(FVector(0.f, 0.f, 2.3f), FVector(70.f, 1.f, 0.35f), 0);
		Kit.Box(FVector(0.f, 0.f, 2.3f), FVector(1.f, 70.f, 0.35f), 0);
		Kit.Box(FVector(70.f, 70.f, 2.6f), FVector(1.2f, 1.2f, 0.5f), 1);
		Kit.Box(FVector(-70.f, -70.f, 2.6f), FVector(1.2f, 1.2f, 0.5f), 1);
	});

	bOk &= SaveKit(TEXT("SM_Grate"), [](FKitBuilder& Kit)
	{
		for (int32 Bar = 0; Bar < 8; ++Bar)
		{
			Kit.Box(FVector(-31.5f + Bar * 9.f, 0.f, 1.2f), FVector(1.1f, 98.f, 0.8f), 0);
		}
		Kit.Box(FVector(0.f, -96.f, 0.6f), FVector(38.f, 2.f, 1.4f), 0);
		Kit.Box(FVector(0.f, 96.f, 0.6f), FVector(38.f, 2.f, 1.4f), 0);
		Kit.Box(FVector(0.f, 0.f, 0.4f), FVector(36.f, 2.f, 1.f), 0);
	});

	bOk &= SaveKit(TEXT("SM_Duct"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 0.f), FVector(50.f, 18.f, 12.f), 0);
		Kit.Box(FVector(0.f, 0.f, -10.f), FVector(50.f, 16.f, 1.4f), 0);
		Kit.Box(FVector(-48.f, 0.f, 0.f), FVector(2.f, 20.f, 14.f), 0);
		Kit.Box(FVector(48.f, 0.f, 0.f), FVector(2.f, 20.f, 14.f), 0);
	});

	bOk &= SaveKit(TEXT("SM_Pipe"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, 0.f, 0.f), FVector(50.f, 5.5f, 5.5f), 0);
		Kit.Box(FVector(-46.f, 0.f, 0.f), FVector(4.f, 8.f, 8.f), 0);
		Kit.Box(FVector(46.f, 0.f, 0.f), FVector(4.f, 8.f, 8.f), 0);
	});

	bOk &= SaveKit(TEXT("SM_Gate"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(0.f, -130.f, 130.f), FVector(12.f, 14.f, 130.f), 0);
		Kit.Box(FVector(0.f, 130.f, 130.f), FVector(12.f, 14.f, 130.f), 0);
		Kit.Box(FVector(0.f, 0.f, 252.f), FVector(14.f, 144.f, 12.f), 0);
		Kit.Box(FVector(0.f, 0.f, 8.f), FVector(18.f, 156.f, 8.f), 0);
		Kit.Box(FVector(10.f, -108.f, 128.f), FVector(2.2f, 2.2f, 108.f), 1);
		Kit.Box(FVector(10.f, 108.f, 128.f), FVector(2.2f, 2.2f, 108.f), 1);
		Kit.Box(FVector(10.f, 0.f, 236.f), FVector(2.2f, 108.f, 3.f), 1);
		Kit.Box(FVector(-6.f, -130.f, 36.f), FVector(16.f, 18.f, 20.f), 0);
		Kit.Box(FVector(-6.f, 130.f, 36.f), FVector(16.f, 18.f, 20.f), 0);
		Kit.Box(FVector(8.f, 0.f, 150.f), FVector(4.f, 70.f, 2.f), 0);
	});

	bOk &= SaveKit(TEXT("SM_Rifle"), [](FKitBuilder& Kit)
	{
		Kit.Box(FVector(-10.f, 0.f, 1.2f), FVector(6.f, 1.7f, 2.2f), 0);
		Kit.Box(FVector(-2.f, 0.f, 0.4f), FVector(6.5f, 1.9f, 2.6f), 0);
		Kit.Box(FVector(-1.f, 0.f, -3.2f), FVector(1.3f, 1.1f, 2.8f), 0);
		Kit.Box(FVector(1.4f, 0.f, -2.6f), FVector(1.5f, 1.15f, 3.4f), 0);
		Kit.Box(FVector(8.f, 0.f, 0.6f), FVector(6.f, 1.55f, 1.8f), 0);
		for (int32 Rib = 0; Rib < 4; ++Rib)
		{
			Kit.Box(FVector(5.f + Rib * 2.4f, 0.f, 2.2f), FVector(0.45f, 1.7f, 0.35f), 0);
		}
		Kit.Box(FVector(16.f, 0.f, 1.1f), FVector(4.2f, 0.7f, 0.7f), 0);
		Kit.Box(FVector(20.5f, 0.f, 1.1f), FVector(1.3f, 1.15f, 1.15f), 0);
		Kit.Box(FVector(2.f, 0.f, 3.4f), FVector(2.4f, 0.9f, 0.7f), 0);
		Kit.Box(FVector(4.2f, 0.f, 4.3f), FVector(0.7f, 0.45f, 0.35f), 1);
	});

	return bOk;
}
#else
bool VanthexBuildKitMeshes()
{
	return false;
}
#endif
