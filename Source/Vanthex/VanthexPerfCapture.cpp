#include "VanthexGameMode.h"

#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexBotController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Scalability.h"
#include "TimerManager.h"

namespace
{
#if CSV_PROFILER
constexpr int32 CaptureFrames = 600;
constexpr float SettleSeconds = 8.f;

void SetCaptureCVar(const TCHAR* Name, float Value)
{
 if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
  Variable->Set(Value, ECVF_SetByCode);
}
#endif
}

void VanthexSchedulePerfCapture(AVanthexGameMode* Mode)
{
 if (!Mode || !FParse::Param(FCommandLine::Get(), TEXT("VanthexPerfCapture"))) return;
#if CSV_PROFILER
 UWorld* World = Mode->GetWorld();
 if (!World || World->WorldType == EWorldType::PIE || World->GetNetMode() != NM_Standalone || !FApp::CanEverRender())
 {
  UE_LOG(LogVanthex, Error, TEXT("VanthexPerfCapture requires a rendered standalone game."));
  FPlatformMisc::RequestExitWithStatus(false, 1);
  return;
 }

 static TWeakObjectPtr<UWorld> ScheduledWorld;
 if (ScheduledWorld.Get() == World) return;
 ScheduledWorld = World;

 // Profile a stationary spawn view. Suspending bot decisions keeps the view and
 // combat effects repeatable; characters still render in their normal poses.
 if (auto* PC = World->GetFirstPlayerController())
 {
  PC->SetIgnoreMoveInput(true);
  PC->SetIgnoreLookInput(true);
 }
 for (TActorIterator<AVanthexBotController> It(World); It; ++It)
  It->SetActorTickEnabled(false);

 Scalability::FQualityLevels Quality;
 Quality.SetFromSingleQualityLevel(2);
 Quality.ResolutionQuality = 100.f;
 Scalability::SetQualityLevels(Quality);
 SetCaptureCVar(TEXT("r.VSync"), 0.f);
 SetCaptureCVar(TEXT("t.MaxFPS"), 0.f);
 SetCaptureCVar(TEXT("r.ScreenPercentage"), 100.f);
 SetCaptureCVar(TEXT("r.DynamicRes.OperationMode"), 0.f);
 SetCaptureCVar(TEXT("csv.CompressionMode"), 0.f);

 FTimerHandle SettleTimer;
 World->GetTimerManager().SetTimer(SettleTimer, FTimerDelegate::CreateWeakLambda(Mode, [Mode]()
 {
  if (!Mode->IsRoundActive() || FCsvProfiler::IsCapturing())
  {
   UE_LOG(LogVanthex, Error, TEXT("VanthexPerfCapture could not start after settling."));
   FPlatformMisc::RequestExitWithStatus(false, 1);
   return;
  }
  const FString MapName = Mode->GetWorld()->GetMapName();
  const FString FileName = FString::Printf(TEXT("VanthexPerf_%s_High"), *MapName);
  FCsvProfiler::SetMetadata(TEXT("VanthexMap"), *MapName);
  FCsvProfiler::SetMetadata(TEXT("VanthexView"), TEXT("StationarySpawn"));
  FCsvProfiler::SetMetadata(TEXT("VanthexQuality"), TEXT("High100PercentUncapped"));
  UE_LOG(LogVanthex, Display, TEXT("VANTHEX_PERF %s after %.0fs settling, %d frames."),
   *FileName, SettleSeconds, CaptureFrames);
  FCsvProfiler::Get()->BeginCapture(CaptureFrames, FString(), FileName);
 }), SettleSeconds, false);
#else
 UE_LOG(LogVanthex, Error, TEXT("VanthexPerfCapture requires CSV profiler support in this build."));
 FPlatformMisc::RequestExitWithStatus(false, 1);
#endif
}
#endif
