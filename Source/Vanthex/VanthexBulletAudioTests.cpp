#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "VanthexBulletAudioComponent.h"
#include "VanthexBulletAudioProfile.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include <limits>

namespace
{
constexpr EAutomationTestFlags BulletAudioTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexBulletAudioDistances, "Vanthex.BulletAudio.DistanceBands", BulletAudioTestFlags)
bool FVanthexBulletAudioDistances::RunTest(const FString& Parameters)
{
	const UVanthexBulletAudioProfile* Profile = GetDefault<UVanthexBulletAudioProfile>();
	using namespace VanthexBulletAudio;
	const FVector Start(0., 0., 0.);
	const FVector End(1000., 0., 0.);
	FNearMiss Result;
	TestTrue(TEXT("A: 30 cm near miss exists"), EvaluateNearMiss(Start, End, FVector(500., 30., 0.), *Profile, Result));
	TestTrue(TEXT("A: close band"), Result.Band == EBand::Close);
	TestTrue(TEXT("A: source is closest point on actual segment"), Result.Point.Equals(FVector(500., 0., 0.), 0.001));
	TestTrue(TEXT("A: perpendicular distance is 30 cm"), FMath::IsNearlyEqual(Result.Distance, 30.f));
	const float CloseGain = Result.Gain;
	TestTrue(TEXT("B: 100 cm near miss exists"), EvaluateNearMiss(Start, End, FVector(500., 100., 0.), *Profile, Result));
	TestTrue(TEXT("B: medium band"), Result.Band == EBand::Medium);
	const float MediumGain = Result.Gain;
	TestTrue(TEXT("C: 250 cm flyby exists"), EvaluateNearMiss(Start, End, FVector(500., 250., 0.), *Profile, Result));
	TestTrue(TEXT("C: far band"), Result.Band == EBand::Far);
	TestTrue(TEXT("A/B/C: gain decreases smoothly with distance"), CloseGain > MediumGain && MediumGain > Result.Gain && Result.Gain > 0.f);
	TestFalse(TEXT("D: 500 cm produces no flyby"), EvaluateNearMiss(Start, End, FVector(500., 500., 0.), *Profile, Result));
	TestFalse(TEXT("Beyond max radius produces no event"), EvaluateNearMiss(Start, End, FVector(500., 300.01, 0.), *Profile, Result));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexBulletAudioFairGeometry, "Vanthex.BulletAudio.ClippedSegmentAndContinuity", BulletAudioTestFlags)
bool FVanthexBulletAudioFairGeometry::RunTest(const FString& Parameters)
{
	const UVanthexBulletAudioProfile* Profile = GetDefault<UVanthexBulletAudioProfile>();
	using namespace VanthexBulletAudio;
	FNearMiss Result;
	const FVector Start(0., 0., 0.);
	const FVector End(1000., 0., 0.);
	TestFalse(TEXT("E: listener past wall-clipped endpoint receives no flyby even within radius"), EvaluateNearMiss(Start, End, FVector(1020., 30., 0.), *Profile, Result));
	TestFalse(TEXT("Listener behind muzzle receives no flyby"), EvaluateNearMiss(Start, End, FVector(-20., 30., 0.), *Profile, Result));
	TestFalse(TEXT("Listener at segment end plane does not hear a passing bullet"), EvaluateNearMiss(Start, End, FVector(1000., 30., 0.), *Profile, Result));
	TestFalse(TEXT("Zero-length segment is rejected"), EvaluateNearMiss(Start, Start, FVector(0., 30., 0.), *Profile, Result));
	TestFalse(TEXT("Non-finite listener is rejected"), EvaluateNearMiss(Start, End, FVector(std::numeric_limits<double>::infinity(), 30., 0.), *Profile, Result));
	for (const double Boundary : {50., 150., 300.})
	{
		FNearMiss Below, Above;
		EvaluateNearMiss(Start, End, FVector(500., Boundary - 0.01, 0.), *Profile, Below);
		EvaluateNearMiss(Start, End, FVector(500., Boundary + 0.01, 0.), *Profile, Above);
		TestTrue(FString::Printf(TEXT("Continuous gain around %.0f cm"), Boundary), FMath::Abs(Below.Gain - Above.Gain) < 0.001f);
	}
	const FVector DiagonalStart(300., -200., 100.);
	const FVector Direction = FVector(1., 1., 1.).GetSafeNormal();
	const FVector Perpendicular = FVector(1., -1., 0.).GetSafeNormal();
	const FVector DiagonalEnd = DiagonalStart + Direction * 1000.;
	const FVector Closest = DiagonalStart + Direction * 400.;
	TestTrue(TEXT("Geometry works for translated 3D diagonal shots"), EvaluateNearMiss(DiagonalStart, DiagonalEnd, Closest + Perpendicular * 100., *Profile, Result));
	TestTrue(TEXT("3D source remains on the valid trajectory"), Result.Point.Equals(Closest, 0.001));
	TestTrue(TEXT("3D perpendicular distance remains 100 cm"), FMath::IsNearlyEqual(Result.Distance, 100.f, 0.001f));
	UVanthexBulletAudioProfile* Custom = NewObject<UVanthexBulletAudioProfile>();
	Custom->CloseDistance = 20.f;
	Custom->MediumDistance = 60.f;
	Custom->MaxFlybyDistance = 100.f;
	TestTrue(TEXT("Data profile adjusts distance thresholds"), EvaluateNearMiss(Start, End, FVector(500., 30., 0.), *Custom, Result));
	TestTrue(TEXT("30 cm uses customized medium band"), Result.Band == EBand::Medium);
	TestFalse(TEXT("Data profile adjusts maximum radius"), EvaluateNearMiss(Start, End, FVector(500., 101., 0.), *Custom, Result));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexBulletAudioOcclusion, "Vanthex.BulletAudio.RealWallOcclusion", BulletAudioTestFlags)
bool FVanthexBulletAudioOcclusion::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues InitValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &InitValues);
	if (!TestNotNull(TEXT("Collision test world exists"), World)) return false;
	const FVector Listener(500., 100., 100.);
	const FVector Source(500., 0., 100.);
	TestTrue(TEXT("Open path is audible"), UVanthexBulletAudioComponent::HasClearPath(World, Listener, Source, nullptr, nullptr));
	AActor* Wall = World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(100., 5., 100.));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Box->RegisterComponent();
	Wall->SetActorLocation(FVector(500., 50., 100.));
	TestFalse(TEXT("E: side wall blocks a nearby flyby despite valid segment projection"), UVanthexBulletAudioComponent::HasClearPath(World, Listener, Source, nullptr, nullptr));
	Wall->SetActorLocation(FVector(1000., 50., 100.));
	TestTrue(TEXT("Moving occluder away restores clear path"), UVanthexBulletAudioComponent::HasClearPath(World, Listener, Source, nullptr, nullptr));
	World->DestroyWorld(false);
	return true;
}

#endif
