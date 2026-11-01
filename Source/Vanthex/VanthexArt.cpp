#include "VanthexArt.h"

#include "Vanthex.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RotationMatrix.h"

namespace
{
UMaterialInterface* LoadFirst(const TArray<const TCHAR*>& Paths)
{
	for (const TCHAR* Path : Paths)
	{
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Path))
		{
			return Material;
		}
	}
	return nullptr;
}

uint8 GlyphRow(TCHAR Character, int32 Row)
{
	// 5-bit rows, top to bottom. Missing glyphs stay blank.
	static const uint8 Space[7] = {0, 0, 0, 0, 0, 0, 0};
	static const uint8 A[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
	static const uint8 B[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
	static const uint8 C[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
	static const uint8 D[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
	static const uint8 E[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
	static const uint8 F[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
	static const uint8 G[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
	static const uint8 H[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
	static const uint8 I[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
	static const uint8 L[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
	static const uint8 N[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
	static const uint8 O[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
	static const uint8 P[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
	static const uint8 R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
	static const uint8 S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
	static const uint8 T[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
	static const uint8 V[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
	static const uint8 Y[7] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
	static const uint8 K[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
	static const uint8 M[7] = {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11};
	static const uint8 U[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
	static const uint8 W[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11};
	static const uint8 X[7] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
	static const uint8 Z[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
	static const uint8 Slash[7] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00};
	static const uint8 Dash[7] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
	static const uint8 D0[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
	static const uint8 D1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
	const uint8* Rows = Space;
	switch (Character)
	{
	case 'A': Rows = A; break;
	case 'B': Rows = B; break;
	case 'C': Rows = C; break;
	case 'D': Rows = D; break;
	case 'E': Rows = E; break;
	case 'F': Rows = F; break;
	case 'G': Rows = G; break;
	case 'H': Rows = H; break;
	case 'I': Rows = I; break;
	case 'L': Rows = L; break;
	case 'N': Rows = N; break;
	case 'O': Rows = O; break;
	case 'P': Rows = P; break;
	case 'R': Rows = R; break;
	case 'S': Rows = S; break;
	case 'T': Rows = T; break;
	case 'V': Rows = V; break;
	case 'Y': Rows = Y; break;
	case 'K': Rows = K; break;
	case 'M': Rows = M; break;
	case 'U': Rows = U; break;
	case 'W': Rows = W; break;
	case 'X': Rows = X; break;
	case 'Z': Rows = Z; break;
	case '/': Rows = Slash; break;
	case '-': Rows = Dash; break;
	case '0': Rows = D0; break;
	case '1': Rows = D1; break;
	default: break;
	}
	return Rows[FMath::Clamp(Row, 0, 6)];
}
}

UMaterialInterface* VanthexArt::GetSurfaceMaterial()
{
	return LoadFirst({
		TEXT("/Game/Vanthex/Materials/M_VanthexSurface.M_VanthexSurface"),
		TEXT("/Engine/TemplateResources/M_Template_Master.M_Template_Master"),
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
	});
}

UMaterialInterface* VanthexArt::GetEnergyMaterial()
{
	return LoadFirst({
		TEXT("/Game/Vanthex/Materials/M_VanthexEnergy.M_VanthexEnergy"),
		TEXT("/Engine/ArtTools/RenderToTexture/Materials/Debug/M_Emissive_Color.M_Emissive_Color"),
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
	});
}

UMaterialInterface* VanthexArt::GetSignMaterial()
{
	return LoadFirst({
		TEXT("/Game/Vanthex/Materials/M_VanthexSign.M_VanthexSign"),
		TEXT("/Game/Vanthex/Materials/M_VanthexEnergy.M_VanthexEnergy"),
		TEXT("/Engine/ArtTools/RenderToTexture/Materials/Debug/M_Emissive_Color.M_Emissive_Color")
	});
}

UMaterialInterface* VanthexArt::GetDecalMaterial()
{
	return LoadFirst({
		TEXT("/Game/Vanthex/Materials/M_VanthexDecal.M_VanthexDecal")
	});
}

UMaterialInterface* VanthexArt::GetGlassMaterial()
{
	return LoadFirst({
		TEXT("/Game/Vanthex/Materials/M_VanthexGlass.M_VanthexGlass"),
		TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent")
	});
}

UMaterialInstanceDynamic* VanthexArt::MakeSurface(
	UObject* Outer,
	const FLinearColor& Color,
	float Roughness,
	float Metallic,
	const FLinearColor& EmissiveColor,
	float EmissiveStrength,
	EVanthexSurface Style)
{
	UMaterialInterface* Base = GetSurfaceMaterial();
	if (!Base || !Outer)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, Outer);
	if (!Instance)
	{
		return nullptr;
	}

	Instance->SetVectorParameterValue(TEXT("BaseColor"), Color);
	Instance->SetVectorParameterValue(TEXT("DiffuseColor"), Color);
	Instance->SetVectorParameterValue(TEXT("Color"), Color);
	Instance->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	Instance->SetScalarParameterValue(TEXT("Metallic"), Metallic);
	Instance->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor);
	Instance->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrength);
	float Panel = 180.f;
	float Relief = 1.f;
	float Wear = 0.4f;
	float StyleId = 0.f;
	switch (Style)
	{
	case EVanthexSurface::PaintedSteel: Panel = 90.f; Relief = 0.75f; Wear = 0.28f; StyleId = 1.f; break;
	case EVanthexSurface::BareSteel: Panel = 14.f; Relief = 0.4f; Wear = 0.15f; StyleId = 2.f; break;
	case EVanthexSurface::DarkMetal: Panel = 140.f; Relief = 0.7f; Wear = 0.45f; StyleId = 1.f; break;
	case EVanthexSurface::Floor: Panel = 150.f; Relief = 0.08f; Wear = 0.22f; StyleId = 0.f; break;
	case EVanthexSurface::Rubber: Panel = 30.f; Relief = 0.15f; Wear = 0.2f; StyleId = 4.f; break;
	default: break;
	}
	Instance->SetScalarParameterValue(TEXT("Panel"), Panel);
	Instance->SetScalarParameterValue(TEXT("Relief"), Relief);
	Instance->SetScalarParameterValue(TEXT("Wear"), Wear);
	Instance->SetScalarParameterValue(TEXT("Style"), StyleId);
	return Instance;
}

UMaterialInstanceDynamic* VanthexArt::MakeEnergy(UObject* Outer, const FLinearColor& Color, float Strength)
{
	UMaterialInterface* Base = GetEnergyMaterial();
	if (!Base || !Outer)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, Outer);
	if (!Instance)
	{
		return nullptr;
	}

	const FLinearColor Bright(Color.R * Strength, Color.G * Strength, Color.B * Strength, 1.f);
	Instance->SetVectorParameterValue(TEXT("Color"), Color);
	Instance->SetVectorParameterValue(TEXT("BaseColor"), Color);
	Instance->SetScalarParameterValue(TEXT("Strength"), Strength);
	Instance->SetVectorParameterValue(TEXT("EmissiveColor"), Bright);
	return Instance;
}

UStaticMesh* VanthexArt::LoadCube()
{
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
}

UStaticMesh* VanthexArt::LoadCylinder()
{
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
}

UStaticMesh* VanthexArt::LoadSphere()
{
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
}

UStaticMesh* VanthexArt::LoadPlane()
{
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
}

UStaticMesh* VanthexArt::LoadKit(const TCHAR* Name)
{
	const FString Path = FString::Printf(TEXT("/Game/Vanthex/Meshes/%s.%s"), Name, Name);
	return LoadObject<UStaticMesh>(nullptr, *Path);
}

UStaticMeshComponent* VanthexArt::AddMesh(
	AActor* Owner,
	USceneComponent* Parent,
	UStaticMesh* Mesh,
	const FVector& Location,
	const FRotator& Rotation,
	const FVector& Scale,
	UMaterialInterface* Material,
	bool bCastShadow)
{
	if (!Owner || !Parent || !Mesh)
	{
		return nullptr;
	}

	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(Parent);
	Component->SetWorldLocationAndRotation(Location, Rotation);
	Component->SetWorldScale3D(Scale);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(bCastShadow);
	Component->RegisterComponent();
	if (Material)
	{
		Component->SetMaterial(0, Material);
	}
	return Component;
}

UInstancedStaticMeshComponent* VanthexArt::MakeInstances(
	AActor* Owner,
	USceneComponent* Parent,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	UMaterialInterface* Secondary,
	bool bCastShadow)
{
	if (!Owner || !Parent || !Mesh)
	{
		return nullptr;
	}

	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(Owner);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(Parent);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(bCastShadow);
	Component->SetMobility(EComponentMobility::Movable);
	Component->RegisterComponent();
	if (Material)
	{
		Component->SetMaterial(0, Material);
	}
	if (Secondary)
	{
		Component->SetMaterial(1, Secondary);
	}
	return Component;
}

UDecalComponent* VanthexArt::AddDecal(
	AActor* Owner,
	USceneComponent* Parent,
	const FVector& Location,
	const FRotator& Rotation,
	const FVector& Size,
	const FLinearColor& Color,
	float Opacity,
	float Stripe)
{
	UMaterialInterface* DecalMaterial = GetDecalMaterial();
	if (!Owner || !Parent || !DecalMaterial)
	{
		return nullptr;
	}

	UDecalComponent* Decal = NewObject<UDecalComponent>(Owner);
	Decal->SetupAttachment(Parent);
	Decal->SetWorldLocationAndRotation(Location, Rotation);
	Decal->DecalSize = Size;
	Decal->SetFadeScreenSize(0.001f);
	Decal->RegisterComponent();
	if (UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(DecalMaterial, Owner))
	{
		Instance->SetVectorParameterValue(TEXT("Color"), Color);
		Instance->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		Instance->SetScalarParameterValue(TEXT("Stripe"), Stripe);
		Decal->SetDecalMaterial(Instance);
	}
	return Decal;
}

void VanthexArt::SpawnSign(
	AActor* Owner,
	USceneComponent* Parent,
	const FString& Text,
	const FVector& Location,
	const FRotator& Rotation,
	float WidthCm,
	float HeightCm,
	const FLinearColor& Color)
{
	const FString Clean = Text.ToUpper();
	const int32 Glyph = 6;
	const int32 Scale = 8;
	const int32 Pad = 8;
	const int32 Width = FMath::Max(4, Clean.Len() * Glyph * Scale + Pad * 2);
	const int32 Height = 7 * Scale + Pad * 2;
	TArray<uint8> Pixels;
	Pixels.SetNumZeroed(Width * Height * 4);
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			uint8* Pixel = &Pixels[(Y * Width + X) * 4];
			Pixel[0] = 18;
			Pixel[1] = 20;
			Pixel[2] = 22;
			Pixel[3] = 255;
		}
	}

	for (int32 CharIndex = 0; CharIndex < Clean.Len(); ++CharIndex)
	{
		const TCHAR Character = Clean[CharIndex];
		for (int32 Row = 0; Row < 7; ++Row)
		{
			const uint8 Bits = GlyphRow(Character, Row);
			for (int32 Col = 0; Col < 5; ++Col)
			{
				if ((Bits & (1 << (4 - Col))) == 0)
				{
					continue;
				}
				for (int32 Sy = 0; Sy < Scale; ++Sy)
				{
					for (int32 Sx = 0; Sx < Scale; ++Sx)
					{
						const int32 X = Pad + CharIndex * Glyph * Scale + Col * Scale + Sx;
						const int32 Y = Pad + Row * Scale + Sy;
						if (X < 0 || Y < 0 || X >= Width || Y >= Height)
						{
							continue;
						}
						uint8* Pixel = &Pixels[(Y * Width + X) * 4];
						Pixel[0] = 245;
						Pixel[1] = 248;
						Pixel[2] = 250;
						Pixel[3] = 255;
					}
				}
			}
		}
	}

	UMaterialInstanceDynamic* Instance = nullptr;
	if (UMaterialInterface* Sign = GetSignMaterial())
	{
		Instance = UMaterialInstanceDynamic::Create(Sign, Owner);
	}
	if (Instance)
	{
		static int32 SignSerial = 0;
		const FName TextureName(*FString::Printf(TEXT("VanthexSign_%d"), ++SignSerial));
		if (UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8, TextureName, Pixels))
		{
			Texture->SRGB = true;
			Texture->Filter = TF_Bilinear;
			Texture->AddressX = TA_Clamp;
			Texture->AddressY = TA_Clamp;
			Texture->UpdateResource();
			Instance->SetTextureParameterValue(TEXT("Mark"), Texture);
		}
		Instance->SetVectorParameterValue(TEXT("Color"), Color);
		Instance->SetScalarParameterValue(TEXT("Strength"), 1.4f);
	}

	const float BoardHeight = HeightCm;
	const float BoardWidth = BoardHeight * static_cast<float>(Width) / static_cast<float>(Height);
	UStaticMesh* Plane = LoadPlane();
	const FVector Facing = Rotation.Vector().GetSafeNormal();
	FVector Right = FVector::CrossProduct(Facing, FVector::UpVector);
	if (Right.SizeSquared() < 0.01f)
	{
		Right = FVector::RightVector;
	}
	Right.Normalize();
	const FRotator BoardRotation = FRotationMatrix::MakeFromXZ(Right, Facing).Rotator();
	if (UStaticMesh* Plate = LoadKit(TEXT("SM_SignPlate")))
	{
		const float PlateScaleY = BoardWidth / 100.f;
		const float PlateScaleZ = BoardHeight / 32.f;
		AddMesh(Owner, Parent, Plate, Location - Facing * 2.f, FRotationMatrix::MakeFromX(Facing).Rotator(), FVector(1.f, PlateScaleY, PlateScaleZ), MakeSurface(Owner, FLinearColor(0.08f, 0.085f, 0.09f), 0.45f, 0.55f, FLinearColor::Black, 0.f, EVanthexSurface::DarkMetal), false);
	}
	VanthexArt::AddMesh(
		Owner,
		Parent,
		Plane,
		Location,
		BoardRotation,
		FVector(BoardWidth / 100.f, BoardHeight / 100.f, 1.f),
		Instance ? static_cast<UMaterialInterface*>(Instance) : MakeSurface(Owner, Color, 0.45f, 0.2f),
		false);
}
