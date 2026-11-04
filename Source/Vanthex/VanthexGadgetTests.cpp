#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "VanthexGadgetRules.h"
#include "VanthexEconomyTypes.h"
#include <limits>

namespace
{
constexpr EAutomationTestFlags GadgetFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexGadgetCatalogTest, "Vanthex.Gadgets.Catalog", GadgetFlags)
bool FVanthexGadgetCatalogTest::RunTest(const FString& Parameters)
{
 const FName Ids[]={TEXT("Flash"),TEXT("Smoke"),TEXT("Frag"),TEXT("EMP"),TEXT("Sensor")};
 for(FName Id:Ids)
 {
  const auto* Profile=VanthexGadgetRules::Find(Id);
  TestNotNull(FString::Printf(TEXT("Effect profile for purchasable %s"),*Id.ToString()),Profile);
  if(!Profile) continue;
  TestTrue(TEXT("Positive finite fuse and radius"),FMath::IsFinite(Profile->Fuse)&&Profile->Fuse>0.f&&FMath::IsFinite(Profile->Radius)&&Profile->Radius>0.f);
  TestTrue(TEXT("Finite nonnegative duration and damage"),FMath::IsFinite(Profile->Duration)&&Profile->Duration>=0.f&&FMath::IsFinite(Profile->MaxDamage)&&Profile->MaxDamage>=0.f);
  TestTrue(TEXT("Effect has its exact catalog ID"),Profile->Id==Id);
  const auto* Item=VanthexEconomy::FindItem(Id);
  TestTrue(TEXT("Effect resolves to a purchasable gadget"),Item&&Item->Type==EVanthexEconomyItemType::Gadget);
 }
 TestNull(TEXT("Unknown ID does not fall back to a live explosive"),VanthexGadgetRules::Find(TEXT("UnknownGadget")));
 TestNull(TEXT("Weapon is not a gadget"),VanthexGadgetRules::Find(TEXT("Pistol")));
 if(const auto* P=VanthexGadgetRules::Find(TEXT("Flash"))) TestTrue(TEXT("Flash starts at 1800 cm / four seconds"),P->Radius==1800.f&&P->Duration==4.f);
 if(const auto* P=VanthexGadgetRules::Find(TEXT("Smoke"))) TestTrue(TEXT("Smoke starts at 450 cm / eighteen seconds"),P->Radius==450.f&&P->Duration==18.f);
 if(const auto* P=VanthexGadgetRules::Find(TEXT("Frag"))) TestTrue(TEXT("Frag starts at 650 cm / two-second fuse / 110 damage"),P->Radius==650.f&&P->Fuse==2.f&&P->MaxDamage==110.f);
 if(const auto* P=VanthexGadgetRules::Find(TEXT("EMP"))) TestTrue(TEXT("EMP starts at 1000 cm / eight seconds"),P->Radius==1000.f&&P->Duration==8.f);
 if(const auto* P=VanthexGadgetRules::Find(TEXT("Sensor"))) TestTrue(TEXT("Sensor starts at 1000 cm / fifteen seconds"),P->Radius==1000.f&&P->Duration==15.f);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexFlashRulesTest, "Vanthex.Gadgets.FlashFalloff", GadgetFlags)
bool FVanthexFlashRulesTest::RunTest(const FString& Parameters)
{
 using namespace VanthexGadgetRules;
 TestEqual(TEXT("Flash at origin facing detonation is full strength"),FlashStrength(0.f,1.f,1800.f),1.f);
 TestEqual(TEXT("Flash at maximum range is zero"),FlashStrength(1800.f,1.f,1800.f),0.f);
 TestEqual(TEXT("Flash outside range is zero"),FlashStrength(2000.f,1.f,1800.f),0.f);
 TestEqual(TEXT("Zero radius cannot blind"),FlashStrength(0.f,1.f,0.f),0.f);
 TestTrue(TEXT("Turning away reduces exposure"),FlashStrength(300.f,-1.f,1800.f)<FlashStrength(300.f,1.f,1800.f));
 TestTrue(TEXT("Distance reduces exposure"),FlashStrength(1200.f,1.f,1800.f)<FlashStrength(300.f,1.f,1800.f));
 for(float Distance:{0.f,300.f,900.f,1799.f,1800.f}) for(float Facing:{-1.f,0.f,1.f})
 {
  const float Strength=FlashStrength(Distance,Facing,1800.f);
  TestTrue(TEXT("Every valid flash result remains normalized"),FMath::IsFinite(Strength)&&Strength>=0.f&&Strength<=1.f);
 }
 TestEqual(TEXT("Nonfinite distance is rejected"),FlashStrength(std::numeric_limits<float>::quiet_NaN(),1.f,1800.f),0.f);
 TestEqual(TEXT("Nonfinite facing is rejected"),FlashStrength(0.f,std::numeric_limits<float>::infinity(),1800.f),0.f);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexBlastRulesTest, "Vanthex.Gadgets.BlastFalloff", GadgetFlags)
bool FVanthexBlastRulesTest::RunTest(const FString& Parameters)
{
 using namespace VanthexGadgetRules;
 TestEqual(TEXT("Center receives maximum damage"),BlastDamage(0.f,650.f,110.f),110.f);
 TestEqual(TEXT("Half radius receives half damage"),BlastDamage(325.f,650.f,110.f),55.f);
 TestEqual(TEXT("Radius boundary has no damage"),BlastDamage(650.f,650.f,110.f),0.f);
 TestEqual(TEXT("Outside radius has no damage"),BlastDamage(651.f,650.f,110.f),0.f);
 TestEqual(TEXT("Zero radius is harmless"),BlastDamage(0.f,0.f,110.f),0.f);
 TestEqual(TEXT("Zero maximum is harmless"),BlastDamage(0.f,650.f,0.f),0.f);
 TestEqual(TEXT("Negative maximum is harmless"),BlastDamage(0.f,650.f,-110.f),0.f);
 TestEqual(TEXT("Nonfinite distance cannot deal damage"),BlastDamage(std::numeric_limits<float>::quiet_NaN(),650.f,110.f),0.f);
 TestEqual(TEXT("Nonfinite maximum cannot deal damage"),BlastDamage(0.f,650.f,std::numeric_limits<float>::infinity()),0.f);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexSmokeRulesTest, "Vanthex.Gadgets.SmokeIntersection", GadgetFlags)
bool FVanthexSmokeRulesTest::RunTest(const FString& Parameters)
{
 using namespace VanthexGadgetRules;
 const FVector Center(100,200,300);
 TestTrue(TEXT("Segment passing through smoke is occluded"),SegmentIntersectsSmoke(Center-FVector(900,0,0),Center+FVector(900,0,0),Center,450.f));
 TestFalse(TEXT("Segment ending before smoke is visible"),SegmentIntersectsSmoke(Center-FVector(900,0,0),Center-FVector(451,0,0),Center,450.f));
 TestFalse(TEXT("Smoke beyond the ray endpoint cannot occlude"),SegmentIntersectsSmoke(Center+FVector(451,0,0),Center+FVector(900,0,0),Center,450.f));
 TestFalse(TEXT("Parallel segment outside radius is visible"),SegmentIntersectsSmoke(Center+FVector(-900,451,0),Center+FVector(900,451,0),Center,450.f));
 TestTrue(TEXT("Observer inside smoke is occluded"),SegmentIntersectsSmoke(Center,Center+FVector(900,0,0),Center,450.f));
 TestTrue(TEXT("Target inside smoke is occluded"),SegmentIntersectsSmoke(Center+FVector(900,0,0),Center,Center,450.f));
 TestTrue(TEXT("Zero-length point inside smoke is occluded"),SegmentIntersectsSmoke(Center,Center,Center,450.f));
 TestFalse(TEXT("Zero-length point outside smoke is visible"),SegmentIntersectsSmoke(Center+FVector(451,0,0),Center+FVector(451,0,0),Center,450.f));
 TestFalse(TEXT("Empty smoke radius is not an occluder"),SegmentIntersectsSmoke(Center-FVector(1,0,0),Center+FVector(1,0,0),Center,0.f));
 const FVector A=Center+FVector(-600,120,0),B=Center+FVector(700,120,0);
 TestEqual(TEXT("Occlusion is independent of ray direction"),SegmentIntersectsSmoke(A,B,Center,450.f),SegmentIntersectsSmoke(B,A,Center,450.f));
 return true;
}
#endif
