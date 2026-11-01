#include "VanthexTestArena.h"

#include "VanthexArt.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

void AVanthexTestArena::DressPass02()
{
	UStaticMesh* Panel = VanthexArt::LoadKit(TEXT("SM_WallPanel"));
	UStaticMesh* PanelB = VanthexArt::LoadKit(TEXT("SM_WallPanelB"));
	UStaticMesh* Column = VanthexArt::LoadKit(TEXT("SM_Column"));
	UStaticMesh* Beam = VanthexArt::LoadKit(TEXT("SM_Beam"));
	UStaticMesh* Light = VanthexArt::LoadKit(TEXT("SM_Light"));
	UStaticMesh* Rack = VanthexArt::LoadKit(TEXT("SM_Rack"));
	UStaticMesh* Cabinet = VanthexArt::LoadKit(TEXT("SM_Cabinet"));
	UStaticMesh* Vent = VanthexArt::LoadKit(TEXT("SM_Vent"));
	UStaticMesh* Door = VanthexArt::LoadKit(TEXT("SM_Door"));
	UStaticMesh* Rail = VanthexArt::LoadKit(TEXT("SM_Railing"));
	UStaticMesh* Crate = VanthexArt::LoadKit(TEXT("SM_Crate"));
	UStaticMesh* Tray = VanthexArt::LoadKit(TEXT("SM_Tray"));
	UStaticMesh* Ring = VanthexArt::LoadKit(TEXT("SM_Ring"));
	UStaticMesh* Fan = VanthexArt::LoadKit(TEXT("SM_Fan"));
	UStaticMesh* Plate = VanthexArt::LoadKit(TEXT("SM_FloorPlate"));
	UStaticMesh* Grate = VanthexArt::LoadKit(TEXT("SM_Grate"));
	UStaticMesh* Duct = VanthexArt::LoadKit(TEXT("SM_Duct"));
	UStaticMesh* Pipe = VanthexArt::LoadKit(TEXT("SM_Pipe"));
	UStaticMesh* Gate = VanthexArt::LoadKit(TEXT("SM_Gate"));
	UStaticMesh* Cylinder = VanthexArt::LoadCylinder();

	UMaterialInstanceDynamic* Concrete = VanthexArt::MakeSurface(this, FLinearColor(0.42f, 0.43f, 0.44f), 0.88f, 0.02f, FLinearColor::Black, 0.f, EVanthexSurface::Concrete);
	UMaterialInstanceDynamic* Painted = VanthexArt::MakeSurface(this, FLinearColor(0.2f, 0.22f, 0.24f), 0.5f, 0.45f, FLinearColor::Black, 0.f, EVanthexSurface::PaintedSteel);
	UMaterialInstanceDynamic* Bare = VanthexArt::MakeSurface(this, FLinearColor(0.28f, 0.29f, 0.3f), 0.34f, 0.86f, FLinearColor::Black, 0.f, EVanthexSurface::BareSteel);
	UMaterialInstanceDynamic* Dark = VanthexArt::MakeSurface(this, FLinearColor(0.07f, 0.075f, 0.08f), 0.48f, 0.4f, FLinearColor::Black, 0.f, EVanthexSurface::DarkMetal);
	UMaterialInstanceDynamic* Warm = VanthexArt::MakeEnergy(this, FLinearColor(1.f, 0.9f, 0.72f), 1.15f);
	UMaterialInstanceDynamic* Cyan = VanthexArt::MakeEnergy(this, FLinearColor(0.35f, 0.9f, 1.f), 0.9f);
	UMaterialInterface* Glass = VanthexArt::GetGlassMaterial();

	auto Instances = [&](UStaticMesh* Mesh, UMaterialInterface* Primary, UMaterialInterface* Secondary, bool bShadow)
	{
		return VanthexArt::MakeInstances(this, SceneRoot, Mesh, Primary, Secondary, bShadow);
	};
	auto Place = [](UInstancedStaticMeshComponent* Mesh, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
	{
		if (Mesh)
		{
			Mesh->AddInstance(FTransform(Rotation, Location, Scale), true);
		}
	};

	if (UInstancedStaticMeshComponent* Walls = Instances(Panel, Concrete, Cyan, true))
	{
		UInstancedStaticMeshComponent* WallsB = Instances(PanelB, Painted, Cyan, true);
		for (int32 Index = -8; Index <= 8; ++Index)
		{
			const float Coord = Index * 200.f;
			const bool bDoor = Index == 0;
			const bool bAlternate = (FMath::Abs(Index) % 3) == 0;
			UInstancedStaticMeshComponent* Row = (bAlternate && WallsB) ? WallsB : Walls;
			if (!bDoor)
			{
				Place(Row, FVector(Coord, -1980.f, 260.f), FRotator(0.f, 90.f, 0.f), FVector(1.f, 1.f, 1.16f));
				Place(Row, FVector(Coord, 1980.f, 260.f), FRotator(0.f, -90.f, 0.f), FVector(1.f, 1.f, 1.16f));
			}
			Place(Row, FVector(1980.f, Coord, 260.f), FRotator(0.f, 180.f, 0.f), FVector(1.f, 1.f, 1.16f));
			Place(Row, FVector(-1980.f, Coord, 260.f), FRotator(0.f, 0.f, 0.f), FVector(1.f, 1.f, 1.16f));
		}
	}

	if (UInstancedStaticMeshComponent* Columns = Instances(Column, Painted, nullptr, true))
	{
		const FVector Corners[4] = {FVector(1760.f, 1760.f, 0.f), FVector(1760.f, -1760.f, 0.f), FVector(-1760.f, 1760.f, 0.f), FVector(-1760.f, -1760.f, 0.f)};
		for (const FVector& Corner : Corners)
		{
			Place(Columns, Corner, FRotator::ZeroRotator, FVector(1.f, 1.f, 1.55f));
		}
		Place(Columns, FVector(-1360.f, -600.f, 0.f), FRotator::ZeroRotator, FVector(0.7f, 0.7f, 0.76f));
		Place(Columns, FVector(-1360.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(0.7f, 0.7f, 0.76f));
		Place(Columns, FVector(-1360.f, 600.f, 0.f), FRotator::ZeroRotator, FVector(0.7f, 0.7f, 0.76f));
		Place(Columns, FVector(20.f, 240.f, 0.f), FRotator::ZeroRotator, FVector(0.48f, 0.48f, 1.25f));
		Place(Columns, FVector(420.f, 240.f, 0.f), FRotator::ZeroRotator, FVector(0.48f, 0.48f, 1.25f));
		Place(Columns, FVector(20.f, -240.f, 0.f), FRotator::ZeroRotator, FVector(0.48f, 0.48f, 1.25f));
		Place(Columns, FVector(420.f, -240.f, 0.f), FRotator::ZeroRotator, FVector(0.48f, 0.48f, 1.25f));
	}

	if (UInstancedStaticMeshComponent* Beams = Instances(Beam, Dark, nullptr, true))
	{
		for (const float Y : {-1200.f, -400.f, 400.f, 1200.f})
		{
			Place(Beams, FVector(0.f, Y, 498.f), FRotator::ZeroRotator, FVector(36.f, 1.f, 1.f));
		}
		for (const float X : {-1200.f, 1200.f})
		{
			Place(Beams, FVector(X, 0.f, 486.f), FRotator(0.f, 90.f, 0.f), FVector(32.f, 1.f, 1.f));
		}
	}

	if (UInstancedStaticMeshComponent* Fixtures = Instances(Light, Painted, Warm, false))
	{
		const FVector Lamps[] = {
			FVector(0.f, -1400.f, 488.f), FVector(0.f, 1500.f, 488.f), FVector(220.f, 0.f, 470.f),
			FVector(-850.f, 850.f, 488.f), FVector(850.f, -750.f, 488.f), FVector(-1200.f, 0.f, 488.f),
			FVector(800.f, 400.f, 488.f), FVector(-400.f, -400.f, 488.f)
		};
		for (const FVector& Lamp : Lamps)
		{
			Place(Fixtures, Lamp, FRotator::ZeroRotator, FVector(1.f));
		}
	}

	if (UInstancedStaticMeshComponent* Racks = Instances(Rack, Dark, Cyan, true))
	{
		for (const float Y : {-200.f, 180.f, 560.f})
		{
			Place(Racks, FVector(-1760.f, Y, 0.f), FRotator(0.f, 90.f, 0.f), FVector(1.f));
		}
	}
	if (UInstancedStaticMeshComponent* Cabinets = Instances(Cabinet, Painted, Warm, true))
	{
		for (const float Y : {-520.f, -80.f, 360.f})
		{
			Place(Cabinets, FVector(1760.f, Y, 0.f), FRotator(0.f, -90.f, 0.f), FVector(1.f));
		}
	}
	if (UInstancedStaticMeshComponent* Vents = Instances(Vent, Dark, nullptr, false))
	{
		Place(Vents, FVector(0.f, -1972.f, 390.f), FRotator(0.f, 90.f, 0.f), FVector(1.4f, 1.f, 1.f));
		Place(Vents, FVector(0.f, 1972.f, 390.f), FRotator(0.f, -90.f, 0.f), FVector(1.4f, 1.f, 1.f));
		Place(Vents, FVector(-1972.f, 900.f, 390.f), FRotator::ZeroRotator, FVector(1.2f));
		Place(Vents, FVector(1972.f, -900.f, 390.f), FRotator(0.f, 180.f, 0.f), FVector(1.2f));
	}
	if (UInstancedStaticMeshComponent* Doors = Instances(Door, Painted, Glass, true))
	{
		Place(Doors, FVector(0.f, -1974.f, 0.f), FRotator(0.f, 90.f, 0.f), FVector(1.f));
		Place(Doors, FVector(0.f, 1974.f, 0.f), FRotator(0.f, -90.f, 0.f), FVector(1.f));
	}
	if (UInstancedStaticMeshComponent* Rails = Instances(Rail, Bare, nullptr, false))
	{
		for (int32 Step = -4; Step <= 3; ++Step)
		{
			const float Y = Step * 200.f;
			Place(Rails, FVector(-1172.f, Y, 238.f), FRotator::ZeroRotator, FVector(1.f));
			Place(Rails, FVector(-1432.f, Y, 238.f), FRotator::ZeroRotator, FVector(1.f));
		}
	}
	if (UInstancedStaticMeshComponent* Crates = Instances(Crate, Dark, nullptr, true))
	{
		Place(Crates, FVector(460.f, -1680.f, 0.f), FRotator(0.f, 15.f, 0.f), FVector(1.f));
		Place(Crates, FVector(-420.f, 1560.f, 0.f), FRotator(0.f, -20.f, 0.f), FVector(1.2f, 1.f, 1.f));
		Place(Crates, FVector(1480.f, 980.f, 0.f), FRotator(0.f, 8.f, 0.f), FVector(1.f));
	}
	if (UInstancedStaticMeshComponent* Trays = Instances(Tray, Dark, nullptr, false))
	{
		Place(Trays, FVector(0.f, 1500.f, 455.f), FRotator::ZeroRotator, FVector(16.f, 1.f, 1.f));
		Place(Trays, FVector(0.f, -1500.f, 455.f), FRotator::ZeroRotator, FVector(16.f, 1.f, 1.f));
		Place(Trays, FVector(1500.f, 0.f, 448.f), FRotator(0.f, 90.f, 0.f), FVector(14.f, 1.f, 1.f));
		Place(Trays, FVector(-1500.f, 0.f, 448.f), FRotator(0.f, 90.f, 0.f), FVector(14.f, 1.f, 1.f));
	}

	if (Cylinder && Bare)
	{
		if (UInstancedStaticMeshComponent* Pipes = Instances(Pipe ? Pipe : Cylinder, Bare, nullptr, false))
		{
			for (int32 Step = 0; Step < 14; ++Step)
			{
				Place(Pipes, FVector(-650.f + Step * 100.f, 1620.f, 430.f), FRotator::ZeroRotator, FVector(1.f));
			}
			for (int32 Step = 0; Step < 10; ++Step)
			{
				Place(Pipes, FVector(-1680.f, -400.f + Step * 100.f, 400.f), FRotator(0.f, 90.f, 0.f), FVector(1.f));
			}
		}
	}

	if (Ring)
	{
		VanthexArt::AddMesh(this, SceneRoot, Ring, FVector(220.f, 0.f, 430.f), FRotator::ZeroRotator, FVector(2.4f, 2.4f, 1.f), Bare, false);
		VanthexArt::AddMesh(this, SceneRoot, Cylinder, FVector(220.f, 0.f, 360.f), FRotator::ZeroRotator, FVector(0.08f, 0.08f, 1.4f), Dark, false);
	}
	if (Gate)
	{
		if (UStaticMeshComponent* CoreGate = VanthexArt::AddMesh(this, SceneRoot, Gate, FVector(220.f, -20.f, 0.f), FRotator(0.f, -90.f, 0.f), FVector(1.15f), Dark, true))
		{
			CoreGate->SetMaterial(1, Cyan);
		}
	}

	const FRotator Floor( -90.f, 0.f, 0.f);
	VanthexArt::AddDecal(this, SceneRoot, FVector(0.f, 0.f, 2.f), FRotator(-90.f, 90.f, 0.f), FVector(4.f, 3.f, 900.f), FLinearColor(0.75f, 0.62f, 0.18f), 0.55f, 0.f);
	VanthexArt::AddDecal(this, SceneRoot, FVector(220.f, 0.f, 3.f), Floor, FVector(4.f, 160.f, 160.f), FLinearColor(0.15f, 0.55f, 0.7f), 0.22f, 0.f);
	VanthexArt::AddDecal(this, SceneRoot, FVector(-850.f, 500.f, 3.f), Floor, FVector(4.f, 70.f, 28.f), FLinearColor(0.85f, 0.62f, 0.12f), 0.8f, 1.f);
	VanthexArt::AddDecal(this, SceneRoot, FVector(850.f, -420.f, 3.f), Floor, FVector(4.f, 70.f, 28.f), FLinearColor(0.85f, 0.45f, 0.1f), 0.8f, 1.f);

	const FLinearColor Cool(0.72f, 0.9f, 1.f);
	const FLinearColor WarmText(1.f, 0.78f, 0.5f);
	VanthexArt::SpawnSign(this, SceneRoot, TEXT("GRID / ENERGY"), FVector(0.f, -620.f, 340.f), FRotator(0.f, -90.f, 0.f), 420.f, 28.f, Cool);
	VanthexArt::SpawnSign(this, SceneRoot, TEXT("CORE CONTAINMENT"), FVector(720.f, 0.f, 250.f), FRotator(0.f, 180.f, 0.f), 360.f, 24.f, Cool);
	VanthexArt::SpawnSign(this, SceneRoot, TEXT("SECTOR A"), FVector(-1680.f, -700.f, 250.f), FRotator(0.f, 0.f, 0.f), 180.f, 20.f, Cool);
	VanthexArt::SpawnSign(this, SceneRoot, TEXT("SECTOR B"), FVector(1680.f, 500.f, 250.f), FRotator(0.f, 180.f, 0.f), 180.f, 20.f, WarmText);
	VanthexArt::SpawnSign(this, SceneRoot, TEXT("POWER"), FVector(1680.f, -200.f, 220.f), FRotator(0.f, 180.f, 0.f), 140.f, 18.f, WarmText);
	VanthexArt::SpawnSign(this, SceneRoot, TEXT("SECURITY"), FVector(0.f, -1900.f, 250.f), FRotator(0.f, 90.f, 0.f), 200.f, 18.f, Cool);
	VanthexArt::SpawnSign(this, SceneRoot, TEXT("ALPHA"), FVector(-1860.f, -200.f, 300.f), FRotator(0.f, 0.f, 0.f), 320.f, 48.f, Cool);
	VanthexArt::SpawnSign(this, SceneRoot, TEXT("BRAVO"), FVector(1860.f, 200.f, 300.f), FRotator(0.f, 180.f, 0.f), 320.f, 48.f, WarmText);
	VanthexArt::AddDecal(this, SceneRoot, FVector(-500.f, 230.f, 40.f), FRotator(0.f, 90.f, 0.f), FVector(8.f, 18.f, 55.f), FLinearColor(0.85f, 0.62f, 0.1f), 0.9f, 1.f);
	VanthexArt::AddDecal(this, SceneRoot, FVector(520.f, -140.f, 36.f), FRotator(0.f, 90.f, 0.f), FVector(8.f, 28.f, 40.f), FLinearColor(0.85f, 0.62f, 0.1f), 0.9f, 1.f);
	VanthexArt::AddDecal(this, SceneRoot, FVector(760.f, 490.f, 36.f), FRotator(0.f, 0.f, 0.f), FVector(8.f, 22.f, 48.f), FLinearColor(0.85f, 0.62f, 0.1f), 0.9f, 1.f);
	VanthexArt::SpawnSign(this, SceneRoot, TEXT("VANTHEX"), FVector(0.f, 1880.f, 310.f), FRotator(0.f, -90.f, 0.f), 240.f, 24.f, Cool);
	VanthexArt::SpawnSign(this, SceneRoot, TEXT("MAINTENANCE"), FVector(1880.f, -980.f, 210.f), FRotator(0.f, 180.f, 0.f), 220.f, 16.f, WarmText);

	if (UInstancedStaticMeshComponent* Plates = Instances(Plate, Bare, nullptr, false))
	{
		for (int32 X = -4; X <= 4; ++X)
		{
			for (int32 Y = -4; Y <= 4; ++Y)
			{
				const FVector Point(X * 400.f, Y * 400.f, 0.f);
				const bool bCore = FVector::Dist2D(Point, FVector(220.f, 0.f, 0.f)) < 240.f;
				const bool bExtractA = FVector::Dist2D(Point, FVector(-850.f, 850.f, 0.f)) < 300.f;
				const bool bExtractB = FVector::Dist2D(Point, FVector(850.f, -750.f, 0.f)) < 300.f;
				if (!bCore && !bExtractA && !bExtractB)
				{
					Place(Plates, Point, FRotator::ZeroRotator, FVector(1.f));
				}
			}
		}
	}
	if (UInstancedStaticMeshComponent* Grates = Instances(Grate, Bare, nullptr, false))
	{
		for (int32 Step = -4; Step <= 3; ++Step)
		{
			const float Y = Step * 200.f;
			Place(Grates, FVector(-1340.f, Y, 239.f), FRotator::ZeroRotator, FVector(1.f));
			Place(Grates, FVector(-1240.f, Y, 239.f), FRotator::ZeroRotator, FVector(1.f));
		}
	}
	if (UInstancedStaticMeshComponent* Ducts = Instances(Duct, Dark, nullptr, false))
	{
		for (int32 Step = 0; Step < 10; ++Step)
		{
			Place(Ducts, FVector(-100.f + Step * 100.f, 1180.f, 452.f), FRotator::ZeroRotator, FVector(1.f));
		}
		for (int32 Step = 0; Step < 8; ++Step)
		{
			Place(Ducts, FVector(-700.f + Step * 100.f, -520.f, 458.f), FRotator::ZeroRotator, FVector(1.f));
			Place(Ducts, FVector(-200.f + Step * 100.f, 280.f, 468.f), FRotator(0.f, 90.f, 0.f), FVector(1.f));
		}
	}

	if (Fan && Bare)
	{
		const FVector Fans[] = {FVector(-400.f, -400.f, 500.f), FVector(700.f, 700.f, 500.f), FVector(-700.f, 900.f, 500.f)};
		for (const FVector& FanLocation : Fans)
		{
			if (UStaticMeshComponent* Blade = VanthexArt::AddMesh(this, SceneRoot, Fan, FanLocation, FRotator::ZeroRotator, FVector(1.f), Bare, false))
			{
				SpinParts.Add(Blade);
			}
		}
	}
}
