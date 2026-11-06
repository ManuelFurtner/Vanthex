#include "VanthexTestArena.h"

#include "Vanthex.h"
#include "VanthexMapLayout.h"
#include "VanthexShadows.h"
#include "VanthexArt.h"
#include "VanthexCore.h"
#include "VanthexShiftDevice.h"
#include "EngineUtils.h"
#include "VanthexExtractionZone.h"
#include "VanthexTestTarget.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/SpotLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"

AVanthexTestArena::AVanthexTestArena()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
 SetNetAddressable();
}

void AVanthexTestArena::Build()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

if(VanthexMap::IsCTF(World)){
 if(!World->GetMapName().EndsWith(TEXT("CrossroadsCTF"))||FParse::Param(FCommandLine::Get(),TEXT("BakeCTF"))) BuildCTF();
 BuildCTFGameplay();return;
}
if(VanthexMap::IsGridV4(World)){
  const bool Baked=World->GetMapName().EndsWith(TEXT("GridV4"))&&!FParse::Param(FCommandLine::Get(),TEXT("BakeGridV4"));
  if(!Baked)BuildGridV4();BuildGridV4Gameplay();
  VanthexShadows::ApplyGridV4(World);
  if(FParse::Param(FCommandLine::Get(),TEXT("GridV4Test"))){FTimerHandle Handle;World->GetTimerManager().SetTimer(Handle,this,&AVanthexTestArena::ValidateGridV4,2.f,false);}
  return;
 }
if (VanthexMap::IsGridV3(World)) {
  const bool bBaked = World->GetMapName().EndsWith(TEXT("GridV3")) && !FParse::Param(FCommandLine::Get(),TEXT("BakeGridV3"));
  if (!bBaked) BuildGridV3();
  BuildGridV3Gameplay();
  if(FParse::Param(FCommandLine::Get(),TEXT("GridV3Test"))) {
   FTimerHandle Handle; World->GetTimerManager().SetTimer(Handle,this,&AVanthexTestArena::ValidateGridV3,2.f,false);
  }
  return;
 }
if (!VanthexMap::IsLegacyTest()) {
  if(FParse::Param(FCommandLine::Get(),TEXT("VanthexLegacyGrid"))) BuildGridMap(); else BuildFoundryMap();
  if(FParse::Param(FCommandLine::Get(),TEXT("VanthexMapTest"))) World->GetTimerManager().SetTimerForNextTick(this,&AVanthexTestArena::ValidateGridMap);
  if(FParse::Param(FCommandLine::Get(),TEXT("VanthexEnergyTest"))) { FTimerHandle Handle; World->GetTimerManager().SetTimer(Handle,this,&AVanthexTestArena::ValidateEnergySystems,2.f,false); }
  return;
 }
 CubeMesh = VanthexArt::LoadCube();

	SpawnBlock(FVector(0.f, 0.f, -10.f), FVector(40.f, 40.f, 0.2f), FLinearColor(0.055f, 0.058f, 0.062f), 0.22f, 0.08f, EVanthexSurface::Floor);
	SpawnBlock(FVector(0.f, 2000.f, 150.f), FVector(40.f, 0.4f, 3.f), FLinearColor(0.40f, 0.41f, 0.42f), 0.88f, 0.02f, EVanthexSurface::Concrete);
	SpawnBlock(FVector(0.f, -2000.f, 150.f), FVector(40.f, 0.4f, 3.f), FLinearColor(0.40f, 0.41f, 0.42f), 0.88f, 0.02f, EVanthexSurface::Concrete);
	SpawnBlock(FVector(2000.f, 0.f, 150.f), FVector(0.4f, 40.f, 3.f), FLinearColor(0.40f, 0.41f, 0.42f), 0.88f, 0.02f, EVanthexSurface::Concrete);
	SpawnBlock(FVector(-2000.f, 0.f, 150.f), FVector(0.4f, 40.f, 3.f), FLinearColor(0.40f, 0.41f, 0.42f), 0.88f, 0.02f, EVanthexSurface::Concrete);
	SpawnBlock(FVector(-500.f, 200.f, 60.f), FVector(1.4f, 0.45f, 1.2f), FLinearColor(0.16f, 0.17f, 0.18f), 0.46f, 0.62f, EVanthexSurface::PaintedSteel);
	SpawnBlock(FVector(520.f, -180.f, 50.f), FVector(0.9f, 0.9f, 1.f), FLinearColor(0.16f, 0.17f, 0.18f), 0.46f, 0.62f, EVanthexSurface::PaintedSteel);
	SpawnBlock(FVector(760.f, 420.f, 50.f), FVector(0.8f, 1.2f, 1.f), FLinearColor(0.16f, 0.17f, 0.18f), 0.46f, 0.62f, EVanthexSurface::PaintedSteel);

	const FLinearColor RouteColor(0.2f, 0.21f, 0.22f);
	const FLinearColor CatwalkColor(0.3f, 0.31f, 0.32f);
	const FLinearColor AlphaColor(0.07f, 0.1f, 0.16f);
	const FLinearColor BravoColor(0.16f, 0.09f, 0.06f);

	SpawnBlock(FVector(-900.f, -750.f, 140.f), FVector(0.28f, 5.f, 2.8f), RouteColor, 0.5f, 0.35f, EVanthexSurface::DarkMetal);
	SpawnBlock(FVector(-900.f, 750.f, 140.f), FVector(0.28f, 5.f, 2.8f), RouteColor, 0.5f, 0.35f, EVanthexSurface::DarkMetal);
	SpawnBlock(FVector(900.f, -750.f, 140.f), FVector(0.28f, 5.f, 2.8f), RouteColor, 0.5f, 0.35f, EVanthexSurface::DarkMetal);
	SpawnBlock(FVector(900.f, 750.f, 140.f), FVector(0.28f, 5.f, 2.8f), RouteColor, 0.5f, 0.35f, EVanthexSurface::DarkMetal);
	SpawnBlock(FVector(-1300.f, 0.f, 230.f), FVector(2.4f, 10.f, 0.16f), CatwalkColor, 0.38f, 0.78f, EVanthexSurface::BareSteel);
	SpawnBlock(FVector(-1300.f, -980.f, 120.f), FVector(2.2f, 3.2f, 0.16f), CatwalkColor, 0.38f, 0.78f, EVanthexSurface::BareSteel);
	SpawnBlock(FVector(0.f, -1500.f, 4.f), FVector(8.f, 3.2f, 0.06f), AlphaColor, 0.55f, 0.22f, EVanthexSurface::PaintedSteel);
	SpawnBlock(FVector(0.f, 1620.f, 4.f), FVector(8.f, 3.2f, 0.06f), BravoColor, 0.55f, 0.22f, EVanthexSurface::PaintedSteel);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (GetNetMode() != NM_Client) Target = World->SpawnActor<AVanthexTestTarget>(FVector(0.f, 1200.f, 0.f), FRotator::ZeroRotator, Params);

	auto AddSpawn = [&](EVanthexTeam Team, const FVector& Location, float Yaw)
	{
		APlayerStart* Start = World->SpawnActor<APlayerStart>(Location, FRotator(0.f, Yaw, 0.f), Params);
		if (!Start)
		{
			return;
		}
		if (Team == EVanthexTeam::Alpha)
		{
			AlphaSpawns.Add(Start);
		}
		else
		{
			BravoSpawns.Add(Start);
		}
	};

	AddSpawn(EVanthexTeam::Alpha, FVector(0.f, -1400.f, 200.f), 90.f);
	AddSpawn(EVanthexTeam::Alpha, FVector(-280.f, -1580.f, 200.f), 90.f);
	AddSpawn(EVanthexTeam::Alpha, FVector(280.f, -1580.f, 200.f), 90.f);
	AddSpawn(EVanthexTeam::Bravo, FVector(0.f, 1700.f, 200.f), -90.f);
	AddSpawn(EVanthexTeam::Bravo, FVector(-320.f, 1580.f, 200.f), -90.f);
	AddSpawn(EVanthexTeam::Bravo, FVector(320.f, 1580.f, 200.f), -90.f);

	if (GetNetMode() != NM_Client) Core = World->SpawnActor<AVanthexCore>(FVector(220.f, 0.f, 50.f), FRotator::ZeroRotator, Params);
	if (Core)
	{
		Core->SetHome(FVector(220.f, 0.f, 50.f));
	}

	ExtractionA = World->SpawnActor<AVanthexExtractionZone>(FVector(-850.f, 850.f, 20.f), FRotator::ZeroRotator, Params);
	ExtractionB = World->SpawnActor<AVanthexExtractionZone>(FVector(850.f, -750.f, 20.f), FRotator::ZeroRotator, Params);
	if (ExtractionA)
	{
		ExtractionA->Setup(EVanthexExtractionId::A, FVector(250.f, 250.f, 220.f));
	}
	if (ExtractionB)
	{
		ExtractionB->Setup(EVanthexExtractionId::B, FVector(250.f, 250.f, 220.f));
	}

	if (!FParse::Param(FCommandLine::Get(), TEXT("VanthexSelfTest"))) {
  // Cover breaks direct spawn-to-spawn fire while preserving three routes.
  for (float Side : {-1.f, 1.f}) {
   SpawnBlock(FVector(0,Side*1050,110),FVector(4,0.5f,2.2f),RouteColor,0.6f,0.3f,EVanthexSurface::Concrete);
   SpawnBlock(FVector(Side*1400,Side*650,65),FVector(2,1,1.3f),RouteColor,0.6f,0.3f,EVanthexSurface::Concrete);
  }
  if (GetNetMode() != NM_Client) {
   const FVector Locations[]={FVector(900,0,140),FVector(-600,400,50),FVector(-1300,-650,10)};
   for (uint8 Kind=0;Kind<3;++Kind) { auto* Device=World->SpawnActor<AVanthexShiftDevice>(Locations[Kind],FRotator::ZeroRotator,Params); if(Device) Device->Configure(Kind); }
  }
 }
 DressFacility();
	BuildAtmosphere();
	UE_LOG(LogVanthex, Display, TEXT("GRID facility built"));
}

void AVanthexTestArena::ResetRoundState()
{
 for(TActorIterator<AVanthexShiftDevice> It(GetWorld());It;++It) It->ResetDevice();
	if (Target)
	{
		Target->ResetForRound();
	}
	if (Core)
	{
		Core->ResetToSpawn();
	}
}

APlayerStart* AVanthexTestArena::GetPlayerStart() const
{
	return GetTeamSpawn(EVanthexTeam::Alpha, 0);
}

APlayerStart* AVanthexTestArena::GetTeamSpawn(EVanthexTeam Team, int32 Slot) const
{
	const TArray<TObjectPtr<APlayerStart>>& Spawns = Team == EVanthexTeam::Bravo ? BravoSpawns : AlphaSpawns;
	return Spawns.IsValidIndex(Slot) ? Spawns[Slot].Get() : nullptr;
}

AVanthexExtractionZone* AVanthexTestArena::GetExtractionZone(EVanthexExtractionId Id) const
{
	if (Id == EVanthexExtractionId::A)
	{
		return ExtractionA;
	}
	if (Id == EVanthexExtractionId::B)
	{
		return ExtractionB;
	}
	return nullptr;
}

void AVanthexTestArena::BeginPlay()
{
	Super::BeginPlay();

	if (SkyLight)
	{
		if (USkyLightComponent* SkyComponent = SkyLight->GetLightComponent())
		{
			SkyComponent->RecaptureSky();
		}
	}
}

UStaticMeshComponent* AVanthexTestArena::SpawnBlock(const FVector& Location, const FVector& Scale, const FLinearColor& Color, float Roughness, float Metallic, EVanthexSurface Style)
{
	UStaticMeshComponent* Block = NewObject<UStaticMeshComponent>(this, FName(*FString::Printf(TEXT("GridBlock_%d"),BlockIndex++)));
 Block->SetNetAddressable();
	if (!Block)
	{
		return nullptr;
	}

	Block->SetStaticMesh(CubeMesh);
	Block->SetupAttachment(SceneRoot);
	Block->SetWorldLocation(Location);
	Block->SetWorldScale3D(Scale);
	Block->SetCollisionProfileName(TEXT("BlockAll"));
	Block->SetCastShadow(true);
	Block->ComponentTags.Add(Style == EVanthexSurface::Concrete || Style == EVanthexSurface::Floor ? FName(TEXT("Concrete")) : FName(TEXT("Metal")));
	Block->RegisterComponent();
	if (UMaterialInstanceDynamic* Instance = VanthexArt::MakeSurface(this, Color, Roughness, Metallic, FLinearColor::Black, 0.f, Style))
	{
		Block->SetMaterial(0, Instance);
	}
	return Block;
}

void AVanthexTestArena::DressFacility()
{
	UMaterialInstanceDynamic* Ceiling = VanthexArt::MakeSurface(this, FLinearColor(0.15f, 0.16f, 0.17f), 0.92f, 0.02f, FLinearColor(0.05f, 0.055f, 0.06f), 0.12f, EVanthexSurface::Concrete);
	VanthexArt::AddMesh(this, SceneRoot, CubeMesh, FVector(0.f, 0.f, 520.f), FRotator::ZeroRotator, FVector(40.f, 40.f, 0.25f), Ceiling, false);
	DressPass02();
}

void AVanthexTestArena::Tick(float DeltaSeconds)
{
	for (USceneComponent* Part : SpinParts)
	{
		if (Part)
		{
			Part->AddLocalRotation(FRotator(0.f, DeltaSeconds * 120.f, 0.f));
		}
	}
}
void AVanthexTestArena::BuildAtmosphere()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FTransform SunTransform(FRotator(-58.f, -30.f, 0.f));
	ADirectionalLight* Sun = World->SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(), SunTransform);
	if (Sun)
	{
		if (ULightComponent* Light = Sun->GetLightComponent())
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(6.5f);
			Light->SetLightColor(FLinearColor(0.78f, 0.84f, 0.95f));
			Light->SetCastShadows(true);
			Light->ContactShadowLength = 0.06f;
		}
		Sun->FinishSpawning(SunTransform);
	}

	const FTransform SkyTransform = FTransform::Identity;
	SkyLight = World->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), SkyTransform);
	if (SkyLight)
	{
		if (USkyLightComponent* SkyComponent = SkyLight->GetLightComponent())
		{
			SkyComponent->SetMobility(EComponentMobility::Movable);
			SkyComponent->SetIntensity(0.9f);
			SkyComponent->SetLightColor(FLinearColor(0.55f, 0.62f, 0.7f));
			SkyComponent->bLowerHemisphereIsBlack = false;
			SkyComponent->LowerHemisphereColor = FLinearColor(0.22f, 0.23f, 0.25f);
		}
		SkyLight->FinishSpawning(SkyTransform);
	}
	World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, Params);

	auto AddSpot = [&](const FVector& Location, const FRotator& Rotation, const FLinearColor& Color, float Intensity, float Radius, bool bShadow)
	{
		const FTransform Transform(Rotation, Location);
		ASpotLight* Spot = World->SpawnActorDeferred<ASpotLight>(ASpotLight::StaticClass(), Transform);
		if (!Spot)
		{
			return;
		}
		if (USpotLightComponent* Light = Cast<USpotLightComponent>(Spot->GetLightComponent()))
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(Intensity);
			Light->SetLightColor(Color);
			Light->SetAttenuationRadius(Radius);
			Light->SetInnerConeAngle(18.f);
			Light->SetOuterConeAngle(42.f);
			Light->SetCastShadows(bShadow);
		}
		Spot->FinishSpawning(Transform);
	};

	const FRotator Down(-90.f, 0.f, 0.f);
	const FLinearColor Fixture(1.f, 0.9f, 0.75f);
	const FLinearColor Cyan(0.45f, 0.85f, 1.f);
	const FLinearColor Amber(1.f, 0.62f, 0.28f);
	AddSpot(FVector(220.f, 0.f, 470.f), Down, Fixture, 9000.f, 1800.f, true);
	AddSpot(FVector(220.f, 0.f, 240.f), FRotator(-15.f, 90.f, 0.f), Cyan, 2500.f, 800.f, false);
	AddSpot(FVector(0.f, -1500.f, 450.f), Down, FLinearColor(0.8f, 0.88f, 1.f), 8000.f, 1400.f, true);
	AddSpot(FVector(0.f, 1620.f, 450.f), Down, FLinearColor(1.f, 0.84f, 0.7f), 8000.f, 1400.f, true);
	AddSpot(FVector(-850.f, 850.f, 450.f), Down, Cyan, 4500.f, 1000.f, true);
	AddSpot(FVector(850.f, -750.f, 450.f), Down, Amber, 4500.f, 1000.f, true);
	AddSpot(FVector(-1300.f, 0.f, 450.f), Down, Fixture, 5500.f, 1100.f, false);
	AddSpot(FVector(1200.f, 200.f, 460.f), Down, Fixture, 4500.f, 1400.f, false);
	AddSpot(FVector(-200.f, 200.f, 460.f), Down, Fixture, 4500.f, 1400.f, false);

	AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Fog)
	{
		if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			FogComponent->SetFogDensity(0.008f);
			FogComponent->SetFogHeightFalloff(0.2f);
			FogComponent->SetFogInscatteringColor(FLinearColor(0.32f, 0.36f, 0.4f));
			FogComponent->SetStartDistance(600.f);
			FogComponent->SetFogMaxOpacity(0.28f);
			FogComponent->SetVolumetricFog(false);
		}
	}

	APostProcessVolume* Post = World->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Post)
	{
		Post->bUnbound = true;
		FPostProcessSettings& Settings = Post->Settings;
		Settings.bOverride_AutoExposureMethod = true;
		Settings.AutoExposureMethod = AEM_Histogram;
		Settings.bOverride_AutoExposureMinBrightness = true;
		Settings.AutoExposureMinBrightness = 0.45f;
		Settings.bOverride_AutoExposureMaxBrightness = true;
		Settings.AutoExposureMaxBrightness = 0.72f;
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = 0.f;
		Settings.bOverride_BloomIntensity = true;
		Settings.BloomIntensity = 0.22f;
		Settings.bOverride_BloomThreshold = true;
		Settings.BloomThreshold = 1.1f;
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = 0.2f;
		Settings.bOverride_MotionBlurAmount = true;
		Settings.MotionBlurAmount = 0.f;
		Settings.bOverride_SceneFringeIntensity = true;
		Settings.SceneFringeIntensity = 0.f;
		Settings.bOverride_ColorSaturation = true;
		Settings.ColorSaturation = FVector4(0.9f, 0.92f, 0.95f, 1.f);
		Settings.bOverride_ColorContrast = true;
		Settings.ColorContrast = FVector4(1.04f, 1.04f, 1.05f, 1.f);
	}
}
