#include "VanthexModerationComponent.h"
#include "VanthexCommunicationComponent.h"
#include "VanthexGameState.h"
#include "VanthexPlayerController.h"
#include "VanthexPlayerState.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
const TCHAR* ReportReasonCode(EVanthexReportReason Reason)
{
 switch (Reason)
 {
 case EVanthexReportReason::Harassment: return TEXT("harassment");
 case EVanthexReportReason::Cheating: return TEXT("suspected_cheating");
 case EVanthexReportReason::Disruption: return TEXT("disruption");
 default: return nullptr;
 }
}

bool IsReportParticipant(const AVanthexPlayerState* State)
{
 return State && !State->IsABot() && !State->IsInactive() && !State->IsOnlyASpectator() &&
  State->GetTeam() != EVanthexTeam::None;
}
}

UVanthexModerationComponent::UVanthexModerationComponent()
{
 SetIsReplicatedByDefault(true);
}

void UVanthexModerationComponent::GetReportTargets(TArray<AVanthexPlayerState*>& Targets) const
{
 Targets.Reset();
 const auto* PC = Cast<AVanthexPlayerController>(GetOwner());
 const auto* World = GetWorld();
 const auto* Match = World ? World->GetGameState<AVanthexGameState>() : nullptr;
 const auto* OwnState = PC ? PC->GetPlayerState<AVanthexPlayerState>() : nullptr;
 if (!Match || !IsReportParticipant(OwnState) || World->GetNetMode() == NM_Standalone) return;
 for (const auto& Entry : Match->PlayerArray)
 {
  auto* State = Cast<AVanthexPlayerState>(Entry.Get());
  if (State != OwnState && IsReportParticipant(State)) Targets.Add(State);
 }
}

bool UVanthexModerationComponent::CanSubmitReport() const
{
 const auto* PC = Cast<AVanthexPlayerController>(GetOwner());
 return !bReportPending && PC && PC->IsLocalController() && GetWorld() && GetNetMode() != NM_Standalone;
}

void UVanthexModerationComponent::SubmitReport(int32 TargetId, EVanthexReportReason Reason)
{
 if (!CanSubmitReport() || !ReportReasonCode(Reason)) return;
 bReportPending = true;
 ReportStatus = NSLOCTEXT("VanthexModeration", "Sending", "Meldung wird an den Spielserver gesendet.");
 ServerSubmitReport(TargetId, Reason);
}

void UVanthexModerationComponent::ServerSubmitReport_Implementation(int32 TargetId, EVanthexReportReason Reason)
{
 const auto* PC = Cast<AVanthexPlayerController>(GetOwner());
 if (!PC || !PC->HasAuthority()) return;
 const double Now = FPlatformTime::Seconds();
 if (Now < NextReportAt)
 {
  ClientReportResult(EVanthexReportResult::Cooldown);
  return;
 }
 NextReportAt = Now + 20.0;

 const TCHAR* ReasonCode = ReportReasonCode(Reason);
 TArray<AVanthexPlayerState*> Targets;
 GetReportTargets(Targets);
 const auto* TargetEntry = Targets.FindByPredicate([TargetId](const AVanthexPlayerState* State)
 {
  return State->GetPlayerId() == TargetId;
 });
 const auto* Match = GetWorld() ? GetWorld()->GetGameState<AVanthexGameState>() : nullptr;
 const auto* Reporter = PC->GetPlayerState<AVanthexPlayerState>();
 const bool bReporterOnRoster = Match && Reporter && Match->PlayerArray.ContainsByPredicate([Reporter](const auto& Entry)
 {
  return Entry.Get() == Reporter;
 });
 if (!ReasonCode || !TargetEntry || !bReporterOnRoster)
 {
  ClientReportResult(EVanthexReportResult::Invalid);
  return;
 }

 const AVanthexPlayerState* Target = *TargetEntry;
 const uint64 ReportKey = (static_cast<uint64>(static_cast<uint32>(TargetId)) << 8) | static_cast<uint8>(Reason);
 if (StoredReports.Contains(ReportKey))
 {
  ClientReportResult(EVanthexReportResult::Duplicate);
  return;
 }

 // Keep reports structured and server-local. Display names never come from the report RPC.
 const FString TargetName = UVanthexCommunicationComponent::CleanText(Target->GetPlayerName(), 32).Replace(TEXT("\x0085"), TEXT(""));
 const FDateTime Utc = FDateTime::UtcNow();
 const FString Directory = FPaths::ProjectSavedDir() / TEXT("Moderation");
 IFileManager::Get().MakeDirectory(*Directory, true);
 const FString File = Directory / FString::Printf(TEXT("PlayerReports-%s.tsv"), *Utc.ToString(TEXT("%Y%m%d")));
 const FString Entry = FString::Printf(TEXT("utc=%s\tmap=%s\tround=%d\treporter=%d\ttarget=%d\tteam=%d\tslot=%d\treason=%s\tname=%s\n"),
  *Utc.ToIso8601(), *GetWorld()->GetMapName(), Match->GetRoundIndex(), Reporter->GetPlayerId(), TargetId,
  static_cast<int32>(Target->GetTeam()), Target->GetSpawnSlot(), ReasonCode, *TargetName);
 if (!FFileHelper::SaveStringToFile(Entry, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append))
 {
  ClientReportResult(EVanthexReportResult::StorageError);
  return;
 }
 StoredReports.Add(ReportKey);
 ClientReportResult(EVanthexReportResult::Stored);
}

void UVanthexModerationComponent::ClientReportResult_Implementation(EVanthexReportResult Result)
{
 bReportPending = false;
 switch (Result)
 {
 case EVanthexReportResult::Stored:
  ReportStatus = NSLOCTEXT("VanthexModeration", "Stored", "Meldung auf dem Spielserver zur Pr\u00fcfung gespeichert.");
  break;
 case EVanthexReportResult::Cooldown:
  ReportStatus = NSLOCTEXT("VanthexModeration", "Cooldown", "Bitte warte kurz bis zur n\u00e4chsten Meldung.");
  break;
 case EVanthexReportResult::Duplicate:
  ReportStatus = NSLOCTEXT("VanthexModeration", "Duplicate", "Diese Meldung wurde bereits gespeichert.");
  break;
 case EVanthexReportResult::StorageError:
  ReportStatus = NSLOCTEXT("VanthexModeration", "StorageError", "Der Spielserver konnte die Meldung nicht speichern.");
  break;
 default:
  ReportStatus = NSLOCTEXT("VanthexModeration", "Invalid", "Der Spieler ist nicht mehr f\u00fcr eine Meldung verf\u00fcgbar.");
  break;
 }
}
