#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VanthexTypes.h"
#include "VanthexBuyMenuComponent.generated.h"

UCLASS()
class UVanthexBuyMenuComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVanthexBuyMenuComponent();
 void Toggle();
 void Close();
 bool IsOpen() const { return Menu.IsValid(); }
 void Purchase(FName Item);
 UFUNCTION(Server, Reliable) void ServerPurchase(FName Item, uint32 RequestId);
 UFUNCTION(Client, Reliable) void ClientPurchaseResult(uint32 RequestId, bool Accepted, FName Item, int32 Credits, const FString& RejectionReason);
 uint32 GetLastResponseId() const { return LastResponseId; }
 bool WasLastPurchaseAccepted() const { return LastAccepted; }
 virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function) override;
protected:
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 void Open();
 bool CanBuy() const;
 TSharedPtr<class SWidget> Menu;
 uint32 NextRequestId = 0, LastResponseId = 0;
 bool Pending = false, LastAccepted = false;
 EVanthexRoundPhase LastPhase = EVanthexRoundPhase::Waiting;
 FString Feedback;
};
