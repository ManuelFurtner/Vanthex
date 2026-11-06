#include "VanthexPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "VanthexModerationComponent.h"
#include "VanthexPlayerState.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

void VanthexStartModerationNetworkTest(AVanthexPlayerController* PC)
{
 if (!PC || !PC->IsLocalController() || PC->HasAuthority() || !PC->GetModeration()) return;

 struct FProbe
 {
  FTimerHandle Timer;
  double Deadline = FPlatformTime::Seconds() + 35.0;
  int32 TargetId = INDEX_NONE;
  bool bStored = false;
 };
 auto Probe = MakeShared<FProbe>();
 PC->GetWorldTimerManager().SetTimer(Probe->Timer, FTimerDelegate::CreateWeakLambda(PC, [PC, Probe]()
 {
  auto* Moderation = PC->GetModeration();
  const auto Finish = [PC, Probe](bool bPass, const TCHAR* Detail)
  {
   PC->GetWorldTimerManager().ClearTimer(Probe->Timer);
   const FString Report = FString::Printf(TEXT("%s %s\n"), bPass ? TEXT("PASS") : TEXT("FAIL"), Detail);
   const bool bSaved = FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("ModerationNetworkTest.txt")));
   FPlatformMisc::RequestExitWithStatus(false, bPass && bSaved ? 0 : 1);
  };
  if (FPlatformTime::Seconds() >= Probe->Deadline)
  {
   Finish(false, TEXT("report or cooldown acknowledgement timed out"));
   return;
  }

  if (Probe->TargetId == INDEX_NONE)
  {
   TArray<AVanthexPlayerState*> Targets;
   Moderation->GetReportTargets(Targets);
   if (Targets.Num() != 1) return;
   Probe->TargetId = Targets[0]->GetPlayerId();
   Moderation->SubmitReport(Probe->TargetId, EVanthexReportReason::Harassment);
   return;
  }

  const FString StatusKey = FTextInspector::GetKey(Moderation->GetReportStatus()).Get(FString());
  if (StatusKey.IsEmpty() || StatusKey == TEXT("Sending")) return;
  if (!Probe->bStored && StatusKey == TEXT("Stored"))
  {
   Probe->bStored = true;
   Moderation->SubmitReport(Probe->TargetId, EVanthexReportReason::Harassment);
   return;
  }
  const bool bCooldown = Probe->bStored && StatusKey == TEXT("Cooldown");
  Finish(bCooldown, bCooldown ? TEXT("server stored one report and rejected an immediate repeat")
   : TEXT("unexpected report result"));
 }), .1f, true, .2f);
}
#endif
