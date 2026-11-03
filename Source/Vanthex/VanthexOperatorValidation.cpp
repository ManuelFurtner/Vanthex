#include "VanthexGameMode.h"

#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexBotController.h"
#include "VanthexCharacter.h"
#include "VanthexOperatorComponent.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexPlayerState.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
struct FOperatorTest
{
 TWeakObjectPtr<UWorld> World;
 TArray<TWeakObjectPtr<AVanthexCharacter>> Characters;
 TWeakObjectPtr<AVanthexCharacter> Runner, Ally, Enemy;
 TArray<FString> Lines;
 FTransform EnemySpawn;
 FVector RunStart, LeftFoot, RightFoot;
 double Started=0., PhaseStarted=0.;
 float MaxFootMotion=0.f;
 int32 Phase=0, Checks=0, Failures=0;
 bool bFinished=false, bNoEarlyFire=true, bSampledReaction=false;

 void Check(bool Passed,const FString& Detail)
 {
  ++Checks;
  if(!Passed) ++Failures;
  Lines.Add((Passed?TEXT("PASS "):TEXT("FAIL "))+Detail);
  UE_LOG(LogVanthex,Display,TEXT("OPERATOR_TEST %s"),*Lines.Last());
 }
 void Finish()
 {
  if(bFinished) return;
  bFinished=true;
  const FString Report=FString::Printf(TEXT("OVERALL %s\nCHECKS %d\nFAILURES %d\n"),Failures?TEXT("FAIL"):TEXT("PASS"),Checks,Failures)+FString::Join(Lines,TEXT("\n"))+TEXT("\n");
  const bool Saved=FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("OperatorValidation.txt")));
  UE_LOG(LogVanthex,Display,TEXT("OPERATOR_TEST complete checks=%d failures=%d saved=%d"),Checks,Failures,Saved);
  FPlatformMisc::RequestExitWithStatus(false,Failures||!Saved?1:0);
 }
};

void FreezeBot(AVanthexCharacter* Character)
{
 if(!Character) return;
 if(auto* Bot=Cast<AVanthexBotController>(Character->GetController())) Bot->SetActorTickEnabled(false);
 if(Character->GetWeapon()) Character->GetWeapon()->StopFire();
 Character->GetCharacterMovement()->StopMovementImmediately();
 Character->GetCharacterMovement()->DisableMovement();
}

void CheckHitProxy(FOperatorTest& Test,AVanthexCharacter* Target,const FName Tag)
{
 TInlineComponentArray<UStaticMeshComponent*> Components;
 Target->GetComponents(Components);
 UStaticMeshComponent* Proxy=nullptr;
 for(auto* Component:Components) if(Component->ComponentHasTag(Tag)) { Proxy=Component; break; }
 if(!Proxy) { Test.Check(false,FString::Printf(TEXT("%s hit proxy exists"),*Tag.ToString())); return; }
 FVector Point=Proxy->Bounds.Origin;
 // The old body/head proxies overlap at neck height; sample the upper head.
 if(Tag==TEXT("Head")) Point.Z+=Proxy->Bounds.BoxExtent.Z*.6f;
 FHitResult Hit;
 FCollisionQueryParams Params(SCENE_QUERY_STAT(OperatorHitProxy),false);
 const bool Blocked=Target->GetWorld()->LineTraceSingleByChannel(Hit,Point-FVector(300,0,0),Point+FVector(300,0,0),ECC_Visibility,Params);
 Test.Check(!Proxy->IsVisible() && Proxy->GetCollisionEnabled()==ECollisionEnabled::QueryOnly && Blocked && Hit.GetActor()==Target && Hit.GetComponent()==Proxy,
  FString::Printf(TEXT("hidden %s proxy still receives actual visibility hits"),*Tag.ToString()));
}
}
#endif

// Called by a 0.1 s GameMode timer only for the explicit -OperatorTest fixture.
// Bot decisions remain enabled: the test controls other participants individually.
void VanthexRunOperatorTest(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode || !Mode->HasAuthority() || !Mode->GetWorld()) return;
 UWorld* World=Mode->GetWorld();
 static FOperatorTest Test;
 const double Now=World->GetTimeSeconds();
 if(Test.World.Get()!=World) { Test=FOperatorTest(); Test.World=World; Test.Started=Now; }
 if(Test.bFinished) return;
 if(Now-Test.Started>45.) { Test.Check(false,FString::Printf(TEXT("bounded fixture timeout in phase %d"),Test.Phase)); Test.Finish(); return; }

 if(Test.Phase==0)
 {
  if(!Mode->IsRoundActive()) return;
  TArray<AVanthexCharacter*> Characters;
  bool Ready=true;
  for(TActorIterator<AVanthexCharacter> It(World);It;++It)
  {
   Characters.Add(*It);
   const auto* Operator=It->FindComponentByClass<UVanthexOperatorComponent>();
   Ready &= Operator && Operator->IsPresentationReady() && It->GetMesh()->GetAnimInstance() && It->GetWeapon();
  }
  if(Characters.Num()!=6 || !Ready)
  {
   if(Now-Test.Started>15.) { Test.Check(false,FString::Printf(TEXT("six loaded operator presentations ready: characters=%d ready=%d"),Characters.Num(),Ready)); Test.Finish(); }
   return;
  }
  int32 Bots=0, Alpha=0, Bravo=0;
  for(auto* Character:Characters)
  {
   Test.Characters.Add(Character);
   const auto* PS=Character->GetPlayerState<AVanthexPlayerState>();
   const bool IsBot=Cast<AVanthexBotController>(Character->GetController())!=nullptr;
   Bots+=IsBot;
   if(PS && PS->GetTeam()==EVanthexTeam::Alpha) { ++Alpha; if(IsBot && PS->GetSpawnSlot()==1) Test.Runner=Character; if(IsBot && PS->GetSpawnSlot()==2) Test.Ally=Character; }
   if(PS && PS->GetTeam()==EVanthexTeam::Bravo) { ++Bravo; if(PS->GetSpawnSlot()==0) Test.Enemy=Character; }
   const auto* Operator=Character->FindComponentByClass<UVanthexOperatorComponent>();
   const auto* Mesh=Character->GetMesh();
   const auto* Rifle=Operator->GetWorldRifle();
   FLinearColor TeamColor=FLinearColor::Black;
   const bool HasColor=Operator->GetArmorMaterial() && Operator->GetArmorMaterial()->GetVectorParameterValue(FMaterialParameterInfo(TEXT("TeamColor")),TeamColor);
   const bool ColorMatches=PS && HasColor && (PS->GetTeam()==EVanthexTeam::Alpha ? TeamColor.B>TeamColor.R : TeamColor.R>TeamColor.B);
   Test.Check(Operator->GetOperatorId()==TEXT("ARC") && Mesh->GetSkeletalMeshAsset() && Mesh->GetNumBones()>30 && Mesh->IsVisible() && Mesh->bOwnerNoSee && Mesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision && Mesh->GetAnimInstance(),
    FString::Printf(TEXT("%s has animated ARC mesh, owner hiding and separate collision"),*Character->GetName()));
   Test.Check(ColorMatches,FString::Printf(TEXT("%s team material follows replicated roster team (%s)"),*Character->GetName(),*TeamColor.ToString()));
   Test.Check(Rifle && Rifle->GetStaticMesh() && Rifle->IsVisible() && Rifle->bOwnerNoSee && Rifle->GetCollisionEnabled()==ECollisionEnabled::NoCollision && Rifle->GetAttachParent()==Mesh,
    FString::Printf(TEXT("%s has visible third-person rifle attached to skeleton"),*Character->GetName()));
  }
  Test.Check(Bots==5 && Alpha==3 && Bravo==3,TEXT("offline roster is one human and five bots, three operators per team"));
  if(!Test.Runner.IsValid() || !Test.Ally.IsValid() || !Test.Enemy.IsValid()) { Test.Check(false,TEXT("required bot roster slots exist")); Test.Finish(); return; }
  Test.EnemySpawn=Test.Enemy->GetActorTransform();
  // The roster has already been moving before readiness. Freezing a teammate
  // in the same corridor would turn a locomotion check into a crowd blockage.
  // Isolate participants without disabling any static map or runner collision.
  int32 Parked=0;
  for(auto Weak:Test.Characters) if(Weak.Get()!=Test.Runner.Get())
  {
   FreezeBot(Weak.Get());
   Weak->SetActorLocation(FVector(60000,10000+Parked++*500,5000),false,nullptr,ETeleportType::TeleportPhysics);
  }
  Test.RunStart=Test.Runner->GetActorLocation();
  Test.LeftFoot=Test.Runner->GetMesh()->GetSocketTransform(TEXT("foot_l"),RTS_Component).GetLocation();
  Test.RightFoot=Test.Runner->GetMesh()->GetSocketTransform(TEXT("foot_r"),RTS_Component).GetLocation();
  Test.Check(Test.Runner->GetMesh()->GetBoneIndex(TEXT("foot_l"))!=INDEX_NONE && Test.Runner->GetMesh()->GetBoneIndex(TEXT("foot_r"))!=INDEX_NONE,TEXT("locomotion bones available for actual pose measurement"));
  Test.PhaseStarted=Now; Test.Phase=1; return;
 }
 if(!Test.Runner.IsValid() || !Test.Ally.IsValid() || !Test.Enemy.IsValid()) { Test.Check(false,TEXT("fixture participants remain alive as actors")); Test.Finish(); return; }
 auto* Runner=Test.Runner.Get();
 auto* Enemy=Test.Enemy.Get();
 const double Age=Now-Test.PhaseStarted;
 if(Test.Phase==1)
 {
  const auto* Mesh=Runner->GetMesh();
  const float Motion=FMath::Max(FVector::Dist(Test.LeftFoot,Mesh->GetSocketTransform(TEXT("foot_l"),RTS_Component).GetLocation()),FVector::Dist(Test.RightFoot,Mesh->GetSocketTransform(TEXT("foot_r"),RTS_Component).GetLocation()));
  Test.MaxFootMotion=FMath::Max(Test.MaxFootMotion,Motion);
  if(Age<2.4) return;
  const float Distance=FVector::Dist2D(Test.RunStart,Runner->GetActorLocation());
  Test.Check(Distance>150.f,FString::Printf(TEXT("live bot navigation moved %.1f cm toward Core; start=%s end=%s speed=%.1f"),Distance,*Test.RunStart.ToString(),*Runner->GetActorLocation().ToString(),Runner->GetVelocity().Size2D()));
  Test.Check(Test.MaxFootMotion>1.f,FString::Printf(TEXT("locomotion changes foot pose in component space by %.2f cm"),Test.MaxFootMotion));
  int32 Index=0;
  for(auto Weak:Test.Characters) if(auto* Character=Weak.Get()) { FreezeBot(Character); Character->SetActorLocation(FVector(60000,10000+Index++*500,5000),false,nullptr,ETeleportType::TeleportPhysics); }
  Runner->SetActorLocation(FVector(50000,0,5000));
  Test.Ally->SetActorLocation(FVector(51000,350,5000));
  Enemy->SetActorLocation(FVector(55000,0,5000));
  CheckHitProxy(Test,Enemy,TEXT("Body")); CheckHitProxy(Test,Enemy,TEXT("Head"));
  Test.Check(!Mode->CanDamage(Runner,Test.Ally.Get()) && Mode->CanDamage(Runner,Enemy),TEXT("bot target rules distinguish friendly and opposing operators"));
  Runner->GetWeapon()->ResetForRound();
  Runner->GetController()->SetActorTickEnabled(true);
  Test.PhaseStarted=Now; Test.Phase=2; return;
 }
 if(Test.Phase==2)
 {
  if(Age<.9) return;
  Test.Check(Runner->GetWeapon()->GetAmmoInMagazine()==Runner->GetWeapon()->GetMagazineSize() && FMath::IsNearlyEqual(Test.Ally->GetHealthComponent()->GetHealth(),100.f),TEXT("live bot does not shoot visible teammate"));
  Enemy->GetHealthComponent()->ResetHealth();
  Enemy->SetActorLocation(FVector(51000,0,5000));
  Test.PhaseStarted=Now; Test.Phase=3; return;
 }
 if(Test.Phase==3)
 {
  if(Age<.4) { Test.bNoEarlyFire &= Runner->GetWeapon()->GetAmmoInMagazine()==Runner->GetWeapon()->GetMagazineSize(); Test.bSampledReaction |= Age>.15; }
  if(Age<1.2) return;
  Test.Check(Test.bSampledReaction && Test.bNoEarlyFire,TEXT("new visible enemy receives the bot reaction delay"));
  Test.Check(Runner->GetWeapon()->GetAmmoInMagazine()<Runner->GetWeapon()->GetMagazineSize() && Enemy->GetHealthComponent()->GetHealth()<100.f,TEXT("live bot aims, fires and damages opposing operator"));
  Test.Check(FMath::IsNearlyEqual(Test.Ally->GetHealthComponent()->GetHealth(),100.f),TEXT("nearby friendly operator remains undamaged"));
  FreezeBot(Runner);
  Enemy->PrepareForRound(Enemy->GetActorTransform());
  FVanthexDamageRequest Fatal; Fatal.Damage=1000.f; Fatal.InstigatorActor=Runner; Fatal.InstigatorController=Runner->GetController();
  Enemy->GetHealthComponent()->ApplyDamage(Fatal);
  Test.PhaseStarted=Now; Test.Phase=4; return;
 }
 if(Test.Phase==4)
 {
  if(Age<.4) return;
  Test.Check(Enemy->GetHealthComponent()->IsDead() && Enemy->GetCharacterMovement()->MovementMode==MOVE_None && !Enemy->GetWeapon()->IsTriggerHeld(),TEXT("eliminated operator stops movement and weapon fire"));
  const auto* Operator=Enemy->FindComponentByClass<UVanthexOperatorComponent>();
  Test.Check(Operator && Operator->GetWorldRifle() && !Operator->GetWorldRifle()->IsVisible(),TEXT("eliminated operator hides third-person rifle"));
  Enemy->PrepareForRound(Test.EnemySpawn);
  Test.PhaseStarted=Now; Test.Phase=5; return;
 }
 if(Test.Phase==5 && Age>=.5)
 {
  const auto* Operator=Enemy->FindComponentByClass<UVanthexOperatorComponent>();
  Test.Check(!Enemy->GetHealthComponent()->IsDead() && FMath::IsNearlyEqual(Enemy->GetHealthComponent()->GetHealth(),100.f) && Enemy->GetCharacterMovement()->MovementMode==MOVE_Walking && Operator && Operator->IsPresentationReady() && Operator->GetWorldRifle()->IsVisible() && Enemy->GetMesh()->IsVisible(),TEXT("round reset restores healthy, visible, animated operator and rifle"));
  Runner->GetEquipment()->ResetEquipment();
  Runner->GetEquipment()->SelectKnife();
  Test.PhaseStarted=Now; Test.Phase=6; return;
 }
 if(Test.Phase==6 && Age>=.7)
 {
  Test.Check(Runner->GetEquipment()->GetSelectedSlot()==EVanthexWeaponSlot::Knife && !Runner->GetEquipment()->IsSwitching(),TEXT("bot takeover fixture starts with a drawn knife"));
  Runner->GetController()->SetActorTickEnabled(true);
  Test.PhaseStarted=Now; Test.Phase=7; return;
 }
 if(Test.Phase==7 && Age>=.8)
 {
  Test.Check(Runner->GetEquipment()->CanUseRifle(),TEXT("live bot returns inherited knife equipment to a usable rifle"));
  Test.Finish();
 }
#endif
}
