#include "VanthexWeaponProfile.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VanthexEconomyTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexWeaponCatalogTest,"Vanthex.Weapons.CatalogAndHandling",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FVanthexWeaponCatalogTest::RunTest(const FString&)
{
 int32 Firearms=0;
 for(const auto& Item:VanthexEconomy::GetCatalog()) {
  if(Item.Type!=EVanthexEconomyItemType::PrimaryWeapon&&Item.Type!=EVanthexEconomyItemType::Sidearm) continue;
  ++Firearms; const auto* P=VanthexWeapons::Find(Item.Id);
  TestNotNull(FString::Printf(TEXT("Purchasable %s has combat profile"),*Item.Id.ToString()),P);
  if(!P) continue;
  TestTrue(TEXT("Profile matches its inventory slot"),P->bSidearm==(Item.Type==EVanthexEconomyItemType::Sidearm));
  TestTrue(TEXT("Usable finite ammunition and timing"),P->MagazineSize>0&&P->ReserveAmmo>=P->MagazineSize&&P->FireInterval>0&&P->ReloadDuration>0);
  TestTrue(TEXT("Ordered reload cues precede completion"),P->MagazineOut>0&&P->MagazineOut<P->MagazineIn&&P->MagazineIn<P->Bolt&&P->Bolt<1);
  TestEqual(FString::Printf(TEXT("%s close damage clamp"),*Item.Id.ToString()),P->DamageAtDistance(0),P->BodyDamage);
  TestEqual(FString::Printf(TEXT("%s falloff-start endpoint"),*Item.Id.ToString()),P->DamageAtDistance(P->FalloffStart),P->BodyDamage);
  TestEqual(FString::Printf(TEXT("%s falloff-end endpoint"),*Item.Id.ToString()),P->DamageAtDistance(P->FalloffEnd),P->BodyDamage*P->MinimumDamageScale);
  TestEqual(FString::Printf(TEXT("%s far damage clamp"),*Item.Id.ToString()),P->DamageAtDistance(P->TraceRange*10),P->BodyDamage*P->MinimumDamageScale);
  float Previous=P->BodyDamage;
  for(int32 I=0;I<20;++I) { const float Damage=P->DamageAtDistance(P->TraceRange*I/19.f); TestTrue(TEXT("Damage never increases with distance"),Damage<=Previous+KINDA_SMALL_NUMBER&&Damage>0); Previous=Damage; }
 }
 TestEqual(TEXT("All nine catalog firearms have usable profiles"),Firearms,9);
 TestNull(TEXT("Unknown weapon is rejected"),VanthexWeapons::Find(TEXT("client-invented")));
 const auto& Pistol=*VanthexWeapons::Find(TEXT("Pistol"));
 const auto& AUG=*VanthexWeapons::Find(TEXT("AUG"));
 const auto& MP7=*VanthexWeapons::Find(TEXT("MP7"));
 TestTrue(TEXT("Pistol is semi; AUG and MP7 are automatic"),!Pistol.IsAutomatic()&&AUG.IsAutomatic()&&MP7.IsAutomatic());
 TestTrue(TEXT("MP7 close-range rate differs from accurate long-range AUG"),MP7.FireInterval<AUG.FireInterval&&MP7.SpreadDegrees>AUG.SpreadDegrees&&MP7.FalloffEnd<AUG.FalloffEnd);
 TestTrue(TEXT("Shotgun has several projectiles but one magazine shell"),VanthexWeapons::Find(TEXT("Shotgun"))->Pellets==8&&VanthexWeapons::Find(TEXT("Shotgun"))->MagazineSize==8);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexWeaponDeterminismTest,"Vanthex.Weapons.DeterministicSpreadAndRecoil",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FVanthexWeaponDeterminismTest::RunTest(const FString&)
{
 const auto& P=*VanthexWeapons::Find(TEXT("Shotgun")); const FRotator Aim(12,43,0);
 TArray<FVector> Rays;
 for(int32 I=0;I<P.Pellets;++I) {
  const FVector Ray=P.ShotDirection(Aim,123,I,600);
  TestTrue(TEXT("Same server sequence recreates same ray"),Ray.Equals(P.ShotDirection(Aim,123,I,600),1.e-9));
  TestTrue(TEXT("Ray is normalized and inside spread cone"),Ray.IsNormalized()&&FVector::DotProduct(Ray,Aim.Vector())>=FMath::Cos(FMath::DegreesToRadians(P.SpreadDegrees+P.MovingSpreadDegrees))-.00001);
  for(const FVector& Earlier:Rays) TestFalse(TEXT("Pellets do not share one ray"),Ray.Equals(Earlier,.000001));
  Rays.Add(Ray);
 }
 TestFalse(TEXT("Different shot sequence changes pattern"),Rays[0].Equals(P.ShotDirection(Aim,124,0,600),.000001));
 const auto& Rifle=*VanthexWeapons::Find(TEXT("AUG"));
 TestTrue(TEXT("Standing spread narrower than moving spread for same random sample"),FVector::DotProduct(Rifle.ShotDirection(Aim,42,0,0),Aim.Vector())>FVector::DotProduct(Rifle.ShotDirection(Aim,42,0,600),Aim.Vector()));
 TestTrue(TEXT("Burst recoil rises and has repeatable lateral control"),Rifle.RecoilForShot(6).X>Rifle.RecoilForShot(0).X&&Rifle.RecoilForShot(2).Y<0&&Rifle.RecoilForShot(4).Y>0&&Rifle.RecoilForShot(4)==Rifle.RecoilForShot(4));
 return true;
}
#endif

#include "VanthexGameMode.h"
#if !UE_BUILD_SHIPPING
#include "VanthexCharacter.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexTestTarget.h"
#include "HitscanWeapon.h"
#include "HealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"
#endif

// Real game actors/timers, invoked repeatedly only by -WeaponProfileTest.
void VanthexRunWeaponProfileTest(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode||!Mode->HasAuthority()) return;
 auto* Human=Mode->FindCombatant(EVanthexTeam::Alpha,0);
 if(!Human||!Human->GetController()||!Human->GetWeapon()||!Human->GetEquipment()||Mode->Combatants.Num()!=6) return;
 static TWeakObjectPtr<UWorld> FixtureWorld;
 static int32 Step=0,Failures=0; static float Deadline=0,Started=0;
 static TArray<FString> Lines; static TWeakObjectPtr<AVanthexTestTarget> Target;
 if(FixtureWorld.Get()!=Mode->GetWorld()) { FixtureWorld=Mode->GetWorld(); Step=Failures=0; Deadline=0; Started=Mode->GetWorld()->GetTimeSeconds(); Lines.Reset(); Target.Reset(); }
 const float Now=Mode->GetWorld()->GetTimeSeconds();
 auto Check=[&](bool Value,const TCHAR* Name) { Lines.Add(FString::Printf(TEXT("%s %s"),Value?TEXT("PASS"):TEXT("FAIL"),Name)); if(!Value) ++Failures; };
 auto Finish=[&]() {
  Lines.Insert(FString::Printf(TEXT("OVERALL %s\nCHECKS %d\nFAILURES %d"),Failures?TEXT("FAIL"):TEXT("PASS"),Lines.Num(),Failures),0);
  FFileHelper::SaveStringToFile(FString::Join(Lines,TEXT("\n"))+TEXT("\n"),*(FPaths::ProjectSavedDir()/TEXT("WeaponProfiles.txt")));
  Step=100; FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
 };
 if(Step==100) return;
 if(Now-Started>35) { Check(false,TEXT("weapon fixture timeout")); Finish(); return; }
 if(Now<Deadline) return;
 auto Next=[&](float Wait) { ++Step; Deadline=Now+Wait; };
 auto* W=Human->GetWeapon(); auto* E=Human->GetEquipment();
 switch(Step) {
 case 0:
  if(!Mode->IsBuyPhase()) return;
  Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
  Check(W->GetWeaponId()==TEXT("Pistol")&&E->GetSelectedSlot()==EVanthexWeaponSlot::Pistol&&!E->HasSlot(EVanthexWeaponSlot::Rifle),TEXT("free pistol and knife without phantom primary"));
  W->StartFire(); W->StartReload();
  Check(W->GetAmmoInMagazine()==12&&!W->IsReloading(),TEXT("buy phase blocks firing and reload; weapon selection is tested by ControlsTest"));
  Mode->ActivateRound(); Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
  for(int32 I=0;I<Mode->Combatants.Num();++I) { auto* Pawn=Mode->Combatants[I].Get(); Pawn->GetCharacterMovement()->StopMovementImmediately(); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying); Pawn->SetActorLocation(FVector(I*500,0,10000),false,nullptr,ETeleportType::TeleportPhysics); }
  Human->GetController()->SetControlRotation(FRotator(60,0,0));
  E->SelectRifle(); Check(E->GetSelectedSlot()==EVanthexWeaponSlot::Pistol,TEXT("unowned primary key cannot grant weapon"));
  W->StartFire(); Next(.6f); return;
 case 1:
  Check(W->GetAmmoInMagazine()==11&&W->IsTriggerHeld(),TEXT("held semi trigger fires exactly once"));
  W->StartFire(); Check(W->GetAmmoInMagazine()==11,TEXT("duplicate press without release cannot repeat semi shot"));
  W->StopFire(); W->StartFire(); W->StopFire(); Check(W->GetAmmoInMagazine()==10,TEXT("released semi trigger fires next shot"));
  W->SetMagazineAmmo(2); W->SetReserveAmmo(13); W->StartReload(); Check(W->IsReloading(),TEXT("finite reload begins")); Next(W->GetReloadDuration()+.15f); return;
 case 2:
  Check(W->GetAmmoInMagazine()==12&&W->GetReserveAmmo()==3&&!W->IsReloading(),TEXT("reload moves only missing magazine rounds from reserve"));
  W->SetMagazineAmmo(1); W->SetReserveAmmo(8); W->StartReload(); W->CancelReload(); Next(W->GetReloadDuration()+.15f); return;
 case 3:
  Check(W->GetAmmoInMagazine()==1&&W->GetReserveAmmo()==8&&!W->IsReloading(),TEXT("cancel clears timer without minting or losing ammunition"));
  W->SetReserveAmmo(2); W->StartReload(); Next(W->GetReloadDuration()+.15f); return;
 case 4:
  Check(W->GetAmmoInMagazine()==3&&W->GetReserveAmmo()==0,TEXT("partial reserve loads only available rounds"));
  W->StartReload(); Check(!W->IsReloading(),TEXT("empty reserve rejects reload"));
  E->SyncLoadout(TEXT("AUG"),TEXT("P250"),true); E->SelectRoundWeapon(); W->SetMagazineAmmo(7); W->SetReserveAmmo(31);
  E->SelectPistol(); W->StartFire(); Check(W->GetAmmoInMagazine()==7&&!W->IsTriggerHeld(),TEXT("holster blocks shots before profile swap")); Next(E->SwitchDuration+.1f); return;
 case 5:
  Check(W->GetWeaponId()==TEXT("P250")&&W->GetAmmoInMagazine()==13&&W->GetReserveAmmo()==52,TEXT("sidearm selection applies independent profile"));
  W->SetMagazineAmmo(3); W->SetReserveAmmo(11); E->SelectRifle(); Next(E->SwitchDuration+.1f); return;
 case 6:
  Check(W->GetWeaponId()==TEXT("AUG")&&W->GetAmmoInMagazine()==7&&W->GetReserveAmmo()==31,TEXT("primary ammunition survives sidearm switch"));
  E->SelectPistol(); Next(E->SwitchDuration+.1f); return;
 case 7:
  Check(W->GetWeaponId()==TEXT("P250")&&W->GetAmmoInMagazine()==3&&W->GetReserveAmmo()==11,TEXT("sidearm ammunition survives primary switch"));
  E->SelectKnife(); Next(E->SwitchDuration+.1f); return;
 case 8:
  Check(W->GetWeaponId()==TEXT("Knife")&&!E->CanUseRifle(),TEXT("knife public ID does not expose a holstered inventory weapon"));
  E->SelectPistol(); Next(E->SwitchDuration+.1f); return;
 case 9:
  Check(W->GetAmmoInMagazine()==3&&W->GetReserveAmmo()==11,TEXT("knife switch preserves sidearm ammunition"));
  E->SyncLoadout(TEXT("MP7"),TEXT("Pistol"),true); E->SelectRoundWeapon(); W->StartFire(); Next(.3f); return;
 case 10:
  W->StopFire(); Check(W->GetAmmoInMagazine()<=36&&W->GetAmmoInMagazine()>=32,TEXT("held automatic trigger repeatedly fires at profile rate"));
  W->SetMagazineAmmo(0); Next(.2f); return;
 case 11:
  W->StartFire(); Check(!W->IsReloading()&&!W->IsTriggerHeld()&&W->GetAmmoInMagazine()==0,TEXT("empty real weapon dry-fires without automatic reload"));
  E->SyncLoadout(TEXT("Shotgun"),TEXT("Pistol"),true); E->SelectRoundWeapon();
  Human->GetController()->SetControlRotation(FRotator::ZeroRotator);
  Target=Mode->GetWorld()->SpawnActor<AVanthexTestTarget>(Human->GetPawnViewLocation()+FVector(200,0,-80),FRotator::ZeroRotator);
  Next(1.f); return;
 case 12:
  W->StartFire(); W->StopFire();
  Check(W->GetAmmoInMagazine()==7,TEXT("shotgun consumes one shell per trigger"));
  Check(Target.IsValid()&&FMath::IsNearlyEqual(Target->GetHealthComponent()->GetHealth(),12.f),TEXT("eight close shotgun pellets each apply eleven damage"));
  Check(!W->EquipProfile(TEXT("invalid"))&&W->GetWeaponId()==TEXT("Shotgun"),TEXT("unknown profile cannot mutate active weapon"));
  { int32 Count=0; for(TActorIterator<AHitscanWeapon> It(Mode->GetWorld());It;++It) if(It->GetOwner()==Human) ++Count; Check(Count==1,TEXT("inventory uses exactly one hitscan actor per player")); }
  E->SyncLoadout(TEXT("AUG"),TEXT("P250"),true); E->SelectRoundWeapon();
  Check(W->GetAmmoInMagazine()==30&&W->GetReserveAmmo()==90,TEXT("round refill restores purchased weapon ammunition"));
  Finish(); return;
 }
#endif
}
