#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VanthexCharacter.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexCombatCorpseCollisionTest,
 "Vanthex.Combat.SequentialKillsAndCorpseCollision",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FVanthexCombatCorpseCollisionTest::RunTest(const FString&)
{
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
  .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false)
  .ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("CombatCollisionRegression"),nullptr,true,ERHIFeatureLevel::Num,&Init);
 if(!TestNotNull(TEXT("collision world"),World)) return false;
 FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Shooter=World->SpawnActor<ADefaultPawn>(FVector(0,0,1000),FRotator::ZeroRotator,Spawn);
 auto* Controller=World->SpawnActor<APlayerController>(); Controller->Possess(Shooter);
 Spawn.Owner=Shooter; Spawn.Instigator=Shooter;
 auto* Weapon=World->SpawnActor<AHitscanWeapon>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
 Weapon->EquipProfile(TEXT("LegacyRifle"));
 Spawn.Owner=nullptr; Spawn.Instigator=nullptr;
 TArray<AVanthexCharacter*> Targets;
 for(int32 Index=0;Index<3;++Index)
  Targets.Add(World->SpawnActor<AVanthexCharacter>(FVector(500+Index*500,0,1000),FRotator::ZeroRotator,Spawn));
 FCollisionQueryParams Query(SCENE_QUERY_STAT(CorpseRegression),false,Shooter); Query.AddIgnoredActor(Weapon);
 auto Trace=[&](float Z) {
  FHitResult Hit; World->LineTraceSingleByChannel(Hit,FVector(100,0,Z),FVector(2000,0,Z),ECC_Visibility,Query); return Hit;
 };
 auto Shoot=[&](AVanthexCharacter* Target) {
  Controller->SetControlRotation((Target->GetActorLocation()-Shooter->GetPawnViewLocation()).Rotation());
  // Isolate collision/damage from cooldown and ammunition; each shot uses production FireShot.
  Weapon->ResetForRound(); Weapon->FireShot();
  TestEqual(TEXT("real shot consumes ammunition"),Weapon->GetAmmoInMagazine(),Weapon->GetMagazineSize()-1);
 };
 for(int32 Index=0;Index<Targets.Num();++Index) {
  auto* Target=Targets[Index];
  TestTrue(FString::Printf(TEXT("body trace reaches living target %d through earlier corpses"),Index),Trace(1000).GetActor()==Target);
  TestTrue(FString::Printf(TEXT("head trace reaches living target %d through earlier corpses"),Index),Trace(1070).GetActor()==Target);
  Shoot(Target);
  TestEqual(TEXT("first body hit deals 40 HP"),Target->GetHealthComponent()->GetHealth(),60.f);
  Shoot(Target); Shoot(Target);
  TestTrue(FString::Printf(TEXT("consecutive target %d dies from three real hitscan shots"),Index),Target->GetHealthComponent()->IsDead());
  TestFalse(TEXT("dead character collision disabled immediately"),Target->GetActorEnableCollision());
 }
 TestFalse(TEXT("no invisible body remains after three kills"),Trace(1000).bBlockingHit);
 TestFalse(TEXT("no invisible head remains after three kills"),Trace(1070).bBlockingHit);
 FHitResult Sweep;
 World->SweepSingleByChannel(Sweep,FVector(100,0,1000),FVector(2000,0,1000),FQuat::Identity,ECC_Pawn,
  FCollisionShape::MakeCapsule(42.f,96.f),Query);
 TestFalse(TEXT("dead movement capsules do not block passage"),Sweep.bBlockingHit);

 auto* Revived=Targets[2]; Revived->GetHealthComponent()->ResetHealth();
 TestTrue(TEXT("health reset restores collision and body hits"),Revived->GetActorEnableCollision()&&Trace(1000).GetActor()==Revived);
 Shoot(Revived); TestEqual(TEXT("revived target takes bullet damage again"),Revived->GetHealthComponent()->GetHealth(),60.f);
 // Ignoring corpses must never allow a bullet through real cover.
 auto* Wall=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall);
 Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(20,200,200));
 Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Block);
 Box->RegisterComponent(); Wall->SetActorLocation(FVector(1250,0,1000));
 Shoot(Revived); TestEqual(TEXT("solid cover still blocks damage behind corpses"),Revived->GetHealthComponent()->GetHealth(),60.f);
 Wall->SetActorEnableCollision(false);
 Targets[0]->PrepareForRound(FTransform(FVector(500,0,1000)));
 TestTrue(TEXT("normal round reset restores nearest target body and head"),Trace(1000).GetActor()==Targets[0]&&Trace(1070).GetActor()==Targets[0]);
 World->DestroyWorld(false);
 return true;
}
#endif
