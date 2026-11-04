#include "VanthexGameMode.h"
#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexBotController.h"
#include "VanthexPlayerState.h"
#include "VanthexOperatorComponent.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexWeaponPresentation.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"
#endif

void VanthexRunPoseHitTest(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode||!Mode->HasAuthority()||Mode->Combatants.Num()!=6)return;
 static TWeakObjectPtr<UWorld> Fixture;static int32 Step=0,Pose=0,Failures=0,Checks=0;static double Start=0,Next=0;static TArray<FString> Lines;
 auto* World=Mode->GetWorld();const double Now=FPlatformTime::Seconds();
 if(Fixture.Get()!=World){Fixture=World;Step=Pose=Failures=Checks=0;Start=Now;Next=0;Lines.Reset();}
 if(Step==100)return;
 auto Check=[&](bool OK,FString Name){++Checks;Lines.Add(FString(OK?TEXT("PASS "):TEXT("FAIL "))+Name);if(!OK)++Failures;};
 auto Finish=[&](){Step=100;const FString Report=FString::Printf(TEXT("OVERALL %s CHECKS %d FAILURES %d\n"),Failures?TEXT("FAIL"):TEXT("PASS"),Checks,Failures)+FString::Join(Lines,TEXT("\n"));FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("PoseHitTest.txt")));UE_LOG(LogVanthex,Display,TEXT("POSE_HIT_TEST %s"),*Report);FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);};
 if(Now-Start>60){Check(false,TEXT("bounded pose test"));Finish();return;}if(Now<Next)return;
 auto* Shooter=Mode->FindCombatant(EVanthexTeam::Alpha,0);auto* Target=Mode->FindCombatant(EVanthexTeam::Bravo,0);
 if(!Shooter||!Target||!Shooter->GetWeapon()||!Target->GetController())return;
 auto* Mesh=Target->GetMesh();auto* W=Shooter->GetWeapon();
 const FVector Base(50000,0,5096);
 auto Trace=[&](FVector Point,FVector Direction=FVector(1,0,0)){
  FCollisionQueryParams Q(SCENE_QUERY_STAT(PoseHitTest),false,Shooter);Q.AddIgnoredActor(W);
  FHitResult Hit;World->LineTraceSingleByChannel(Hit,Point+Direction*250,Point-Direction*250,ECC_Visibility,Q);return Hit;
 };
 auto Center=[&](FName Bone){auto* Body=Mesh->GetBodyInstance(Bone);return Body?Body->GetCOMPosition():Mesh->GetBoneLocation(Bone);};
 auto Advance=[&](int32 Value,double Delay=.1){Step=Value;Next=Now+Delay;};
 switch(Step){
 case 0:
  Mode->BeginRound();Mode->ActivateRound();Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
  for(int32 I=0;I<Mode->Combatants.Num();++I){auto* C=Mode->Combatants[I].Get();if(auto* Bot=Cast<AVanthexBotController>(C->GetController()))Bot->SetActorTickEnabled(false);C->GetWeapon()->StopFire();C->GetCharacterMovement()->DisableMovement();C->SetActorLocation(FVector(60000,I*600,6000));C->GetPlayerState<AVanthexPlayerState>()->ResetEconomyForMatch();}
  {auto* Floor=World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Floor);Floor->SetRootComponent(Box);Box->SetBoxExtent(FVector(2000,2000,10));Box->SetCollisionProfileName(TEXT("BlockAll"));Box->RegisterComponent();Floor->SetActorLocation(Base-FVector(0,0,106));}
  Target->GetEquipment()->SyncLoadout(TEXT("M4"),TEXT("Pistol"),true);Target->GetEquipment()->SelectRifle();
  Target->SetActorLocationAndRotation(Base,FRotator::ZeroRotator);Target->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  Shooter->SetActorLocation(Base+FVector(600,0,0));W->EquipProfile(TEXT("LegacyRifle"));
  Check(Mesh->GetPhysicsAsset()!=nullptr&&Mesh->GetCollisionEnabled()==ECollisionEnabled::QueryOnly,TEXT("animated physics asset supplies query hit zones"));
  Check(Target->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Visibility)==ECR_Ignore,TEXT("movement capsule cannot intercept bullet traces"));
  if(auto* Asset=Mesh->GetPhysicsAsset())for(const auto& Body:Asset->SkeletalBodySetups)Lines.Add(TEXT("BODY ")+Body->BoneName.ToString());
  Advance(1,.8);return;
 case 1:
  Target->SetHoldForward(false);Target->GetCharacterMovement()->StopMovementImmediately();Target->SetActorLocation(Base);
  Target->GetController()->SetControlRotation(FRotator(Pose==2?50:Pose==3?-40:0,0,0));
  Target->SetCrouching(Pose==1);if(Pose==4)Target->SetHoldForward(true);
  Advance(2,.5);return;
 case 2:
  for(const FName Bone:{FName(TEXT("head")),FName(TEXT("spine_02")),FName(TEXT("pelvis")),FName(TEXT("upperarm_l")),FName(TEXT("lowerarm_r")),FName(TEXT("thigh_l")),FName(TEXT("calf_r"))}){
   // The head's mass centre is close to the jaw/neck overlap. Sample the cranium
   // from behind so a raised forearm correctly remains a body hit from the front.
   const FVector Point=Center(Bone)+(Bone==TEXT("head")?FVector(0,0,8):FVector::ZeroVector);const auto Hit=Trace(Point,Bone==TEXT("head")?FVector(-1,0,0):Bone.ToString().Contains(TEXT("arm"))?FVector(0,1,0):FVector(1,0,0));
   Check(Hit.GetActor()==Target&&Hit.GetComponent()==Mesh&&!Hit.BoneName.IsNone(),FString::Printf(TEXT("pose %d bone %s hits animated mesh (%s)"),Pose,*Bone.ToString(),*Hit.BoneName.ToString()));
   if(Bone==TEXT("head"))Check(Hit.BoneName==TEXT("head"),TEXT("head classified by skeletal bone"));
  }
  {const FVector Head=Center(TEXT("head"));Check(Trace(Head+FVector(0,0,45)).GetActor()!=Target,TEXT("empty space above posed head misses"));}
  if(++Pose<5){Advance(1);return;}Target->SetHoldForward(false);Target->GetCharacterMovement()->StopMovementImmediately();Target->SetCrouching(false);Advance(3,.4);return;
 case 3:
  {const FVector Point=Center(TEXT("head"));Shooter->SetActorLocation(FVector(Point.X+600,Point.Y,Point.Z-64));Shooter->GetController()->SetControlRotation((Point-Shooter->GetPawnViewLocation()).Rotation());Target->GetHealthComponent()->ResetHealth();W->ResetForRound();W->FireShot();Check(FMath::IsNearlyEqual(Target->GetHealthComponent()->GetHealth(),20.f),TEXT("real skeletal headshot applies 80 damage"));}
  Advance(4,.3);return;
 case 4:
  {const FVector Point=Center(TEXT("pelvis"));Shooter->SetActorLocation(FVector(Point.X+600,Point.Y,Point.Z-64));Shooter->GetController()->SetControlRotation((Point-Shooter->GetPawnViewLocation()).Rotation());Target->GetHealthComponent()->ResetHealth();W->ResetForRound();W->FireShot();Check(FMath::IsNearlyEqual(Target->GetHealthComponent()->GetHealth(),60.f),TEXT("real skeletal body shot applies 40 damage"));
   FVanthexDamageRequest Fatal;Fatal.Damage=1000;Target->GetHealthComponent()->ApplyDamage(Fatal);Check(Trace(Point).GetActor()!=Target,TEXT("death disables articulated hit zones"));}
  Advance(10,.7);return;
 case 10:
  Check(Mesh->GetSocketTransform(TEXT("root")).GetRotation().GetAxisZ().Z<.1,TEXT("fixture reaches collapsed death pose"));
  Target->GetHealthComponent()->ResetHealth();
  Check(Mesh->GetSocketTransform(TEXT("root")).GetRotation().GetAxisZ().Z>.99&&Trace(Center(TEXT("pelvis"))).GetActor()==Target,TEXT("revive synchronously restores upright pose and same-frame hits"));
  Advance(5,.3);return;
 case 5:
  Check(Trace(Center(TEXT("pelvis"))).GetActor()==Target,TEXT("revive restores posed hit zones"));
  {auto* Operator=Shooter->FindComponentByClass<UVanthexOperatorComponent>();auto* Arms=Operator?Operator->GetFirstPersonArms():nullptr;
   Check(Arms&&Arms->IsVisible()&&Arms->bOnlyOwnerSee&&Arms->GetCollisionEnabled()==ECollisionEnabled::NoCollision,TEXT("owner-only first person arms exist without gameplay collision"));
   Check(Arms&&!Arms->IsBoneHiddenByName(TEXT("hand_l"))&&!Arms->IsBoneHiddenByName(TEXT("hand_r")),TEXT("both first person hands remain visible"));
   const auto* ArmAsset=Arms?Arms->GetSkeletalMeshAsset():nullptr;
   Check(ArmAsset&&ArmAsset!=Shooter->GetMesh()->GetSkeletalMeshAsset()&&ArmAsset->GetSkeleton()==Shooter->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton()&&!ArmAsset->GetPhysicsAsset(),TEXT("dedicated arms geometry shares the original skeleton without duplicate torso or physics"));}
  Pose=0;Advance(6);return;
 case 6:
  {
   const FName Ids[]={TEXT("Pistol"),TEXT("P250"),TEXT("MP5"),TEXT("MP7"),TEXT("M4"),TEXT("AK"),TEXT("AUG"),TEXT("DMR"),TEXT("Shotgun")};
   const FName Id=Ids[Pose/2];const bool Pistol=Pose/2<2;
   Shooter->SetAiming(false);W->CancelReload();
   Shooter->GetEquipment()->SyncLoadout(Pistol?NAME_None:Id,Pistol?Id:FName(TEXT("Pistol")),true);
   Shooter->GetEquipment()->SelectRoundWeapon();
   Shooter->GetController()->SetControlRotation(FRotator::ZeroRotator);
   Shooter->SetActorRotation(FRotator::ZeroRotator);
   Shooter->SetAiming(Pose%2==1);Advance(7,.7);return;
  }
 case 7:
 case 8:
  {
   auto* Operator=Shooter->FindComponentByClass<UVanthexOperatorComponent>();
   const FName Id=W->GetWeaponId();const bool Pistol=Id==TEXT("Pistol")||Id==TEXT("P250");
   for(const bool Ego:{true,false}) {
    auto* Arms=Ego?Operator->GetFirstPersonArms():Shooter->GetMesh();
    const auto* Mount=Ego?W->GetFirstPersonPivot():Operator->GetWorldFirearmRoot();
    const FTransform T=Mount->GetComponentTransform();
    const FVector Right=T.TransformPosition(VanthexWeaponPresentation::RightPalm(Id))-T.GetRotation().GetAxisX()*11.;
    const FVector Left=T.TransformPosition(VanthexWeaponPresentation::LeftPalm(Id,W->GetReloadProgress()))-(Pistol?T.GetRotation().GetAxisX():T.GetRotation().GetAxisY())*10.-(Pistol?FVector::ZeroVector:T.GetRotation().GetAxisZ()*2.);
    for(const bool IsRight:{true,false}) {
     const double Error=FVector::Dist(Arms->GetBoneLocation(IsRight?TEXT("hand_r"):TEXT("hand_l")),IsRight?Right:Left);
     Check(Error<2.5,FString::Printf(TEXT("%s %s %s %s wrist tracks equipment: %.2f cm"),*Id.ToString(),Ego?TEXT("ego"):TEXT("world"),Step==8?TEXT("reload"):Pose%2?TEXT("ADS"):TEXT("hip"),IsRight?TEXT("right"):TEXT("left"),Error));
    }
   }
   if(Step==8){Finish();return;}
   if(++Pose<18){Advance(6);return;}
   Shooter->SetAiming(false);Shooter->GetEquipment()->SyncLoadout(TEXT("M4"),TEXT("Pistol"),true);Shooter->GetEquipment()->SelectRoundWeapon();
   W->SetMagazineAmmo(2);W->StartReload();Advance(8,.7);return;
  }
 }
#endif
}
