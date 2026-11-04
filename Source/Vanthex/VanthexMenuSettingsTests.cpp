#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
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
#endif
