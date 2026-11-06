#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "VanthexLocalSettings.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexMenuAddressTest,"Vanthex.Menu.ConnectionInput",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FVanthexMenuAddressTest::RunTest(const FString& Parameters)
{
	FString Address,Error;
	for(const TCHAR* Valid:{TEXT("127.0.0.1"),TEXT("192.168.1.10:7777"),TEXT("localhost"),TEXT("play.vanthex.test:65535"),TEXT(" 127.0.0.1:1 ")})
		TestTrue(FString::Printf(TEXT("Accepts %s"),Valid),UVanthexLocalSettings::ValidateAddress(Valid,Address,Error));
	TestEqual(TEXT("Trims outer whitespace"),Address,FString(TEXT("127.0.0.1:1")));
	for(const TCHAR* Invalid:{TEXT(""),TEXT(" "),TEXT("host:0"),TEXT("host:65536"),TEXT("host:"),TEXT("host:-1"),TEXT("host:abc"),
		TEXT("https://host"),TEXT("host?listen"),TEXT("host/Map"),TEXT("host name"),TEXT("host;quit"),TEXT("host:7777:7778"),TEXT("::1"),
		TEXT("999.1.1.1"),TEXT("127.0.1"),TEXT("-host"),TEXT("a.-host"),TEXT("host..test")})
	{
		TestFalse(FString::Printf(TEXT("Rejects %s"),Invalid),UVanthexLocalSettings::ValidateAddress(Invalid,Address,Error));
		TestTrue(TEXT("Shows an error and clears destination"),!Error.IsEmpty() && Address.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexMenuVolumeTest,"Vanthex.Menu.AudioBounds",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FVanthexMenuVolumeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Mute stays silent"),UVanthexLocalSettings::SafeVolume(0.f),0.f);
	TestEqual(TEXT("Negative saved gain is clamped"),UVanthexLocalSettings::SafeVolume(-1.f),0.f);
	TestEqual(TEXT("Excess saved gain is clamped"),UVanthexLocalSettings::SafeVolume(2.f),1.f);
	TestEqual(TEXT("A valid gain is retained"),UVanthexLocalSettings::SafeVolume(.6f),.6f);
	TestEqual(TEXT("Non-finite gain is silent"),UVanthexLocalSettings::SafeVolume(std::numeric_limits<float>::quiet_NaN()),0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexMenuControlsSettingsTest, "Vanthex.Menu.ControlsSettings",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FVanthexMenuControlsSettingsTest::RunTest(const FString& Parameters)
{
 using Action = EVanthexGameplayAction;
 auto* Settings = NewObject<UVanthexLocalSettings>();
 Settings->ResetGameplayKeys();
 TestTrue(TEXT("Jump defaults to Space"), Settings->GetGameplayKey(Action::Jump) == EKeys::SpaceBar);
 TestTrue(TEXT("Aim defaults to right mouse"), Settings->GetGameplayKey(Action::Aim) == EKeys::RightMouseButton);

 FText Error;
 TestFalse(TEXT("Existing movement binding is a conflict"), Settings->TrySetGameplayKey(Action::Jump, EKeys::W, Error));
 TestTrue(TEXT("Conflict has a readable error"), !Error.IsEmpty());
 TestFalse(TEXT("Menu key stays reserved"), Settings->TrySetGameplayKey(Action::Jump, EKeys::Escape, Error));
 TestFalse(TEXT("Secondary default Ctrl conflicts with another action"), Settings->TrySetGameplayKey(Action::Jump, EKeys::RightControl, Error));
 TestTrue(TEXT("Rejected assignments retain prior binding"), Settings->GetGameplayKey(Action::Jump) == EKeys::SpaceBar);
 TestTrue(TEXT("Unused keyboard key can bind jump"), Settings->TrySetGameplayKey(Action::Jump, EKeys::J, Error));
 TestTrue(TEXT("Freed Space can bind reload"), Settings->TrySetGameplayKey(Action::Reload, EKeys::SpaceBar, Error));
 TestFalse(TEXT("Remapped keys still conflict"), Settings->TrySetGameplayKey(Action::Fire, EKeys::J, Error));

 Settings->MouseSensitivity = 2.f;
 Settings->ADSMouseSensitivity = .5f;
 TestEqual(TEXT("ADS factor does not change hip sensitivity"), Settings->GetMouseSensitivity(), 2.f);
 TestEqual(TEXT("ADS sensitivity uses half of hip at full aim"), Settings->GetMouseSensitivity(1.f), 1.f);
 TestEqual(TEXT("Aim transition interpolates sensitivity"), Settings->GetMouseSensitivity(.5f), 1.5f);

 // Exercise UObject config serialization in memory without touching the user's settings or CDO.
 FConfigCacheIni TempConfig(EConfigCacheType::Temporary);
 const FString ConfigFile(TEXT("VanthexControlsSettingsTest.ini"));
 // SaveConfig evicts an implicitly created branch after attempting to flush it.
 // Pre-create the branch so this temporary cache retains the saved values.
 TempConfig.AddNewBranch(ConfigFile);
 Settings->SaveConfig(CPF_Config, *ConfigFile, &TempConfig, false);
 auto* Reloaded = NewObject<UVanthexLocalSettings>();
 Reloaded->ResetGameplayKeys();
 Reloaded->MouseSensitivity = 1.f;
 Reloaded->ADSMouseSensitivity = 1.f;
 UObject::FLoadConfigParams Load;
 Load.ConfigSystem = &TempConfig;
 Load.Filename = ConfigFile;
 Reloaded->LoadConfig(Load);
 TestTrue(TEXT("Saved jump binding survives load"), Reloaded->GetGameplayKey(Action::Jump) == EKeys::J);
 TestTrue(TEXT("Saved binding to freed key survives load"), Reloaded->GetGameplayKey(Action::Reload) == EKeys::SpaceBar);
 TestEqual(TEXT("Saved hip sensitivity survives load"), Reloaded->GetMouseSensitivity(), 2.f);
 TestEqual(TEXT("Saved ADS factor survives load"), Reloaded->GetADSMouseSensitivity(), .5f);

 Reloaded->ResetGameplayKeys();
 TestTrue(TEXT("Reset restores jump and reload defaults"), Reloaded->GetGameplayKey(Action::Jump) == EKeys::SpaceBar &&
  Reloaded->GetGameplayKey(Action::Reload) == EKeys::R);
 Reloaded->ADSMouseSensitivity = std::numeric_limits<float>::quiet_NaN();
 TestEqual(TEXT("Invalid ADS factor falls back to normal sensitivity"), Reloaded->GetMouseSensitivity(1.f), 2.f);
 return true;
}
#endif
