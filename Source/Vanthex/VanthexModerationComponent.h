#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VanthexModerationComponent.generated.h"

class AVanthexPlayerState;

UENUM()
enum class EVanthexReportReason : uint8
{
 Harassment,
 Cheating,
 Disruption
};

UENUM()
enum class EVanthexReportResult : uint8
{
 Stored,
 Invalid,
 Cooldown,
 Duplicate,
 StorageError
};

/** Owner requests and feedback. Report records remain on the authoritative server. */
UCLASS()
class UVanthexModerationComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVanthexModerationComponent();
 void GetReportTargets(TArray<AVanthexPlayerState*>& Targets) const;
 bool CanSubmitReport() const;
 void SubmitReport(int32 TargetId, EVanthexReportReason Reason);
 const FText& GetReportStatus() const { return ReportStatus; }

private:
 UFUNCTION(Server, Reliable) void ServerSubmitReport(int32 TargetId, EVanthexReportReason Reason);
 UFUNCTION(Client, Reliable) void ClientReportResult(EVanthexReportResult Result);

 FText ReportStatus;
 bool bReportPending = false;
 double NextReportAt = 0.0;
 TSet<uint64> StoredReports;
};
