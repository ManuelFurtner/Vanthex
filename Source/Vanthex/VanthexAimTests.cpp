#include "VanthexWeaponPresentation.h"
#include "VanthexWeaponProfile.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Materials/MaterialInterface.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexAimGeometryTest,"Vanthex.Aim.SightAlignmentAndSpread",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FVanthexAimGeometryTest::RunTest(const FString&)
{
 for(const auto& Weapon:VanthexWeapons::All()) {
  const auto Sight=VanthexWeaponPresentation::Aim(Weapon.Id);
  const FVector Axis=Sight.Rotation.RotateVector(Sight.SightPoint)+Sight.CameraRelativeOrigin;
  TestTrue(Weapon.Id.ToString()+TEXT(" sight projects onto view centre"),Axis.X>10&&FMath::Abs(Axis.Y)<.001&&FMath::Abs(Axis.Z)<.001);
  TestTrue(Weapon.Id.ToString()+TEXT(" optical FOV and movement limits"),Sight.FieldOfView>=25&&Sight.FieldOfView<85&&Sight.MovementScale>.3&&Sight.MovementScale<.8);
  const FRotator Rotation(17,63,0);
  double HipVariance=0,AimVariance=0; bool WithinAimCone=true;
  const float AimCone=Weapon.SpreadDegrees+Weapon.MovingSpreadDegrees*(500.f/600.f)*Sight.MovingSpreadScale;
  for(uint32 Sequence=1;Sequence<=2048;++Sequence) {
   const double Hip=FVector::DotProduct(Rotation.Vector(),Weapon.ShotDirection(Rotation,Sequence,0,500));
   const double Aim=FVector::DotProduct(Rotation.Vector(),Weapon.ShotDirection(Rotation,Sequence,0,500*Sight.MovingSpreadScale));
   WithinAimCone &= Aim>=FMath::Cos(FMath::DegreesToRadians(AimCone))-1.e-7;
   HipVariance+=FMath::Max(0.,1.-Hip); AimVariance+=FMath::Max(0.,1.-Aim);
  }
  TestTrue(Weapon.Id.ToString()+TEXT(" every ADS ray lies inside reduced authoritative spread cone"),WithinAimCone);
  // UE VRandCone folds a random angle modulo cone size: compare distributions, not paired rays.
  if(Weapon.MovingSpreadDegrees>0)
   TestTrue(Weapon.Id.ToString()+TEXT(" ADS reduces angular variance across 2048 repeatable moving shots"),AimVariance<HipVariance*.85);
  else TestTrue(TEXT("zero-spread diagnostic remains unchanged"),FMath::IsNearlyEqual(AimVariance,HipVariance,1.e-8));
 }
 TestTrue(TEXT("AUG and DMR use distinct scope magnification"),VanthexWeaponPresentation::Aim(TEXT("AUG")).Sight==EVanthexSight::Scope&&VanthexWeaponPresentation::Aim(TEXT("DMR")).FieldOfView<VanthexWeaponPresentation::Aim(TEXT("AUG")).FieldOfView);
 TestTrue(TEXT("MP7 uses physical reflex; pistol uses physical irons"),VanthexWeaponPresentation::Aim(TEXT("MP7")).Sight==EVanthexSight::Reflex&&VanthexWeaponPresentation::Aim(TEXT("Pistol")).Sight==EVanthexSight::Iron);
 const auto* Lens=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Vanthex/Weapons/Materials/M_WPN_Optic.M_WPN_Optic"));
 if(TestNotNull(TEXT("Optic lens material imported"),Lens)) TestEqual(TEXT("Reflex lens does not block world"),Lens->GetBlendMode(),BLEND_Translucent);
 return true;
}
#endif

#include "VanthexGameMode.h"
#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexBuyMenuComponent.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexPlayerController.h"
#include "HitscanWeapon.h"
#include "HealthComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#endif

void VanthexRunAimTest(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode||!Mode->HasAuthority()) return;
 auto* Pawn=Mode->FindCombatant(EVanthexTeam::Alpha,0);
 auto* PC=Pawn?Cast<AVanthexPlayerController>(Pawn->GetController()):nullptr;
 if(!Pawn||!PC||!Pawn->GetWeapon()||!Pawn->GetEquipment()) return;
 static TWeakObjectPtr<UWorld> Fixture; static int32 Step=0,Failures=0;
 static double Started=0,NextAt=0; static TArray<FString> Lines;
 const double Now=FPlatformTime::Seconds();
 if(Fixture.Get()!=Mode->GetWorld()) { Fixture=Mode->GetWorld(); Step=Failures=0; Started=Now; NextAt=0; Lines.Reset(); }
 if(Step==100) return;
 auto Check=[&](bool Pass,const TCHAR* Name) { Lines.Add(FString::Printf(TEXT("%s %s"),Pass?TEXT("PASS"):TEXT("FAIL"),Name)); if(!Pass) ++Failures; };
 auto Finish=[&]() { Step=100; const FString Report=FString::Printf(TEXT("OVERALL %s CHECKS %d FAILURES %d\n"),Failures?TEXT("FAIL"):TEXT("PASS"),Lines.Num(),Failures)+FString::Join(Lines,TEXT("\n"))+TEXT("\n"); FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("AimTest.txt"))); UE_LOG(LogVanthex,Display,TEXT("AIM_TEST %s"),*Report); FPlatformMisc::RequestExitWithStatus(false,Failures?1:0); };
 if(Now-Started>35) { Check(false,TEXT("bounded runtime timeout")); Finish(); return; }
 if(Now<NextAt) return;
 auto Next=[&](float Delay=.65f) { ++Step; NextAt=Now+Delay; };
 auto* W=Pawn->GetWeapon(); auto* E=Pawn->GetEquipment();
 switch(Step) {
 case 0:
  if(!Mode->IsBuyPhase()) return;
  Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
  Pawn->SetAiming(true); Check(!Pawn->IsAiming(),TEXT("buy phase rejects ADS"));
  Mode->ActivateRound(); Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
  PC->GetBuyMenu()->Close(); PC->SetViewTarget(Pawn);
  Pawn->GetCharacterMovement()->StopMovementImmediately();
  Pawn->SetAiming(true); Check(Pawn->IsAuthoritativelyAiming(),TEXT("active owner enters server ADS")); Next(); return;
 case 1:
  Check(Pawn->GetAimBlend()>.99f&&FMath::IsNearlyEqual(Pawn->GetFirstPersonCamera()->FieldOfView,72.f,.2f),TEXT("iron sight smoothly reaches its actual camera FOV"));
  Check(Pawn->GetCharacterMovement()->MaxWalkSpeed<400.f,TEXT("server ADS limits run speed"));
  Pawn->SetCrouching(true); Next(.2f); return;
 case 2:
  Check(Pawn->bIsCrouched&&Pawn->IsAiming()&&Pawn->GetCharacterMovement()->MaxWalkSpeedCrouched<280,TEXT("crouch remains compatible with ADS and reduced speed"));
  Pawn->SetCrouching(false); Pawn->SetAiming(false); Next(); return;
 case 3:
  Check(!Pawn->IsAiming()&&Pawn->GetAimBlend()<.001f&&FMath::IsNearlyEqual(Pawn->GetFirstPersonCamera()->FieldOfView,90.f,.1f)&&FMath::IsNearlyEqual(Pawn->GetCharacterMovement()->MaxWalkSpeed,600.f),TEXT("release restores camera and movement"));
  Pawn->SetAiming(true); Pawn->SetSprinting(true); Check(!Pawn->IsAiming(),TEXT("sprint cancels ADS"));
  Pawn->SetAiming(true); Check(!Pawn->IsAiming(),TEXT("ADS cannot bypass active sprint")); Pawn->SetSprinting(false);
  Pawn->SetAiming(true); W->SetMagazineAmmo(2); W->StartReload(); Check(W->IsReloading()&&!Pawn->IsAiming(),TEXT("actual weapon reload cancels ADS"));
  Pawn->SetAiming(true); Check(!Pawn->IsAiming(),TEXT("reload rejects ADS request")); W->CancelReload();
  Pawn->SetAiming(true); E->SelectKnife(); Next(.8f); return;
 case 4:
  Pawn->SetAiming(true); Check(!Pawn->IsAiming()&&W->GetWeaponId()==TEXT("Knife"),TEXT("knife switch cancels and blocks ADS"));
  E->SyncLoadout(TEXT("AUG"),TEXT("Pistol"),true); E->SelectRoundWeapon(); Pawn->SetAiming(true); Next(); return;
 case 5:
  Check(Pawn->IsAiming()&&FMath::IsNearlyEqual(Pawn->GetFirstPersonCamera()->FieldOfView,45.f,.2f),TEXT("AUG enters its magnified scope"));
  Mode->Phase=EVanthexRoundPhase::BuyPhase; Mode->PublishMatch(); Next(); return;
 case 6:
  Check(!Pawn->IsAiming()&&Pawn->GetAimBlend()<.001f,TEXT("phase transition cancels ADS without release input"));
  Mode->ActivateRound(); Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer); PC->GetBuyMenu()->Close();
  Pawn->SetAiming(true);
  { FVanthexDamageRequest Damage; Damage.InstigatorActor=Mode->FindCombatant(EVanthexTeam::Bravo,0); Damage.Damage=1000; Pawn->GetHealthComponent()->ApplyDamage(Damage); }
  Check(Pawn->GetHealthComponent()->IsDead()&&!Pawn->IsAiming(),TEXT("death cancels ADS"));
  Pawn->SetAiming(true); Check(!Pawn->IsAiming(),TEXT("dead owner cannot re-enter ADS")); Finish(); return;
 }
#endif
}
