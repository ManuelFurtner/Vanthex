#include "VanthexGameInstance.h"
#include "SVanthexLoadingScreen.h"
#include "MoviePlayer.h"
#include "UObject/UObjectGlobals.h"

void UVanthexGameInstance::Init()
{
 Super::Init();
 // MoviePlayer is not used in PIE, headless checks or dedicated servers.
 if(IsRunningDedicatedServer() || IsRunningCommandlet() || GIsEditor || !IsMoviePlayerEnabled()) return;
 PreLoadHandle=FCoreUObjectDelegates::PreLoadMap.AddUObject(this,&UVanthexGameInstance::PrepareLoadingScreen);
 // Arm before the initial default map loads, then refresh for subsequent travel.
 PrepareLoadingScreen(FString());
}

void UVanthexGameInstance::PrepareLoadingScreen(const FString& MapName)
{
 if(IsRunningDedicatedServer() || IsRunningCommandlet() || GIsEditor || !IsMoviePlayerEnabled()) return;
 auto* Player=GetMoviePlayer();
 if(!Player || Player->IsMovieCurrentlyPlaying()) return;
 FLoadingScreenAttributes Screen;
 Screen.bAutoCompleteWhenLoadingCompletes=true;
 Screen.bWaitForManualStop=false;
 Screen.bMoviesAreSkippable=false;
 Screen.bAllowEngineTick=false;
 Screen.MinimumLoadingScreenDisplayTime=0.f;
 Screen.WidgetLoadingScreen=SNew(SVanthexLoadingScreen);
 Player->SetupLoadingScreen(Screen);
 // The engine starts/stops MoviePlayer around LoadMap. No manual wait/stop loop.
}

void UVanthexGameInstance::Shutdown()
{
 if(PreLoadHandle.IsValid()) FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadHandle);
 PreLoadHandle.Reset();
 Super::Shutdown();
}
