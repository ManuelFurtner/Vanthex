#include "VanthexBuyMenuComponent.h"
#include "VanthexPlayerController.h"
#include "VanthexPlayerState.h"
#include "VanthexGameState.h"
#include "VanthexCharacter.h"
#include "VanthexWeaponAudioComponent.h"
#include "VanthexEconomyTypes.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

UVanthexBuyMenuComponent::UVanthexBuyMenuComponent()
{
 SetIsReplicatedByDefault(true);
 PrimaryComponentTick.bCanEverTick = true;
 PrimaryComponentTick.TickInterval = .1f;
}

bool UVanthexBuyMenuComponent::CanBuy() const
{
 const auto* PC = Cast<AVanthexPlayerController>(GetOwner());
 const auto* State = PC ? PC->GetPlayerState<AVanthexPlayerState>() : nullptr;
 const auto* Match = GetWorld() ? GetWorld()->GetGameState<AVanthexGameState>() : nullptr;
 return State && State->IsAlive() && State->GetTeam() != EVanthexTeam::None && Match && Match->GetPhase() == EVanthexRoundPhase::BuyPhase;
}

void UVanthexBuyMenuComponent::Toggle()
{
 if (IsOpen()) Close(); else if (CanBuy()) Open();
}

void UVanthexBuyMenuComponent::Open()
{
 auto* PC = Cast<AVanthexPlayerController>(GetOwner());
 auto* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
 if (!PC || !PC->IsLocalController() || !Viewport || IsOpen() || PC->IsSessionMenuOpen()) return;
 static const FTextBlockStyle Title = FTextBlockStyle().SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 28)).SetColorAndOpacity(FLinearColor(.25f,.85f,1.f));
 static const FTextBlockStyle Text = FTextBlockStyle().SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 17)).SetColorAndOpacity(FLinearColor(.91f,.94f,1.f));
 static const FTextBlockStyle Small = FTextBlockStyle().SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 13)).SetColorAndOpacity(FLinearColor(.63f,.72f,.8f));
 auto Columns = SNew(SHorizontalBox);
 const TArray<TArray<FName>> Groups = {{TEXT("P250"),TEXT("MP5"),TEXT("MP7"),TEXT("Shotgun")}, {TEXT("M4"),TEXT("AK"),TEXT("AUG"),TEXT("DMR")}, {TEXT("ArmorLight"),TEXT("ArmorHeavy"),TEXT("Flash"),TEXT("Smoke"),TEXT("Frag"),TEXT("EMP"),TEXT("Sensor")}};
 const TCHAR* Labels[] = {TEXT("NAHKAMPF & SEKUNDAER"),TEXT("PRAEZISION & REICHWEITE"),TEXT("RUESTUNG & GADGETS")};
 for (int32 Group = 0; Group < Groups.Num(); ++Group) {
  auto List = SNew(SVerticalBox);
  List->AddSlot().AutoHeight().Padding(6,12)[SNew(STextBlock).TextStyle(&Small).Text(FText::FromString(Labels[Group]))];
  for (FName Id : Groups[Group]) {
   List->AddSlot().AutoHeight().Padding(4)[SNew(SButton).ContentPadding(FMargin(12,10)).ButtonColorAndOpacity(FLinearColor(.055f,.10f,.15f))
    .IsEnabled_Lambda([this,Id]() {
     const auto* OwnerPC = Cast<AVanthexPlayerController>(GetOwner());
     const auto* State = OwnerPC ? OwnerPC->GetPlayerState<AVanthexPlayerState>() : nullptr;
     if (!CanBuy() || Pending || !State) return false;
     FVanthexEconomyState Preview = State->GetEconomy();
     return VanthexEconomy::TryApplyPurchase(Preview, Id);
    })
    .ToolTipText_Lambda([this,Id]() {
     if (Pending) return FText::FromString(TEXT("Serverbestaetigung abwarten."));
     if (!CanBuy()) return FText::FromString(TEXT("Nur waehrend der Kaufphase verfuegbar."));
     const auto* OwnerPC = Cast<AVanthexPlayerController>(GetOwner());
     const auto* State = OwnerPC ? OwnerPC->GetPlayerState<AVanthexPlayerState>() : nullptr;
     if (!State) return FText::FromString(TEXT("Spielerstatus ist noch nicht bereit."));
     FVanthexEconomyState Preview = State->GetEconomy();
     FString Reason;
     return VanthexEconomy::TryApplyPurchase(Preview, Id, &Reason) ? FText::GetEmpty() : FText::FromString(Reason);
    })
    .OnClicked_Lambda([this,Id]() { Purchase(Id); return FReply::Handled(); })
    [SNew(STextBlock).TextStyle(&Text).Text_Lambda([Id]() { const auto* Item = VanthexEconomy::FindItem(Id); return FText::FromString(FString::Printf(TEXT("%s    %d CR"), Item ? *Item->DisplayName : *Id.ToString(), Item ? Item->Price : 0)); })]];
  }
  Columns->AddSlot().FillWidth(1).Padding(8)[List];
 }
 auto Contents = SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(STextBlock).TextStyle(&Title).Text(FText::FromString(TEXT("VANTHEX / AUSRUESTUNG")))]
 +SVerticalBox::Slot().AutoHeight().Padding(12,0)[SNew(STextBlock).TextStyle(&Text).Text_Lambda([this]() {
  const auto* PC = Cast<AVanthexPlayerController>(GetOwner()); const auto* PS = PC ? PC->GetPlayerState<AVanthexPlayerState>() : nullptr; const auto* GS = GetWorld()->GetGameState<AVanthexGameState>();
  return FText::FromString(FString::Printf(TEXT("%d CR     |     Kaufphase: %d s     |     B schliesst"), PS ? PS->GetEconomy().Credits : 0, GS ? FMath::CeilToInt(GS->GetTimeRemaining()) : 0)); })]
 +SVerticalBox::Slot().AutoHeight()[Columns]
 +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(STextBlock).TextStyle(&Text).Text_Lambda([this]() {
  const auto* PC = Cast<AVanthexPlayerController>(GetOwner()); const auto* PS = PC ? PC->GetPlayerState<AVanthexPlayerState>() : nullptr;
  if (!PS) return FText::GetEmpty(); const auto& E = PS->GetEconomy();
  return FText::FromString(FString::Printf(TEXT("1  %s     2  %s     3  Messer\nRuestung %.0f     |     Gadgets %d / 2"), E.PrimaryWeapon.IsNone() ? TEXT("Kein Primaer") : *E.PrimaryWeapon.ToString(), *E.Sidearm.ToString(), E.ArmorPoints, E.Gadgets.Num())); })]
 +SVerticalBox::Slot().AutoHeight().Padding(12,2)[SNew(STextBlock).TextStyle(&Small).Text(FText::FromString(TEXT("4 / 5: Gadget werfen. Flash blendet auch Verbuendete. Ersatzkaeufe ohne Erstattung.")))]
 +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(STextBlock).TextStyle(&Text).Text_Lambda([this]() { return FText::FromString(Feedback); })]
 +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(SButton).ContentPadding(FMargin(12,8)).OnClicked_Lambda([this]() { Close(); return FReply::Handled(); })[SNew(STextBlock).TextStyle(&Text).Text(FText::FromString(TEXT("Bereit / Schliessen")))]];
 Menu = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.012f,.022f,.035f,.98f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
  [SNew(SBox).WidthOverride(1170).MaxDesiredHeight(900)[SNew(SScrollBox)+SScrollBox::Slot()[Contents]]];
 Viewport->AddViewportWidgetContent(Menu.ToSharedRef(), 90);
 PC->bShowMouseCursor = true;
 PC->SetIgnoreLookInput(true);
 FInputModeGameAndUI Mode; Mode.SetWidgetToFocus(Menu); Mode.SetHideCursorDuringCapture(false); PC->SetInputMode(Mode);
}

void UVanthexBuyMenuComponent::Close()
{
 if (!IsOpen()) return;
 if (GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Menu.ToSharedRef());
 Menu.Reset();
 if (auto* PC = Cast<AVanthexPlayerController>(GetOwner()); PC && PC->IsLocalController()) {
  PC->ResetIgnoreLookInput();
  if (!PC->IsSessionMenuOpen()) { PC->bShowMouseCursor = false; PC->SetInputMode(FInputModeGameOnly()); }
 }
}

void UVanthexBuyMenuComponent::Purchase(FName Item)
{
 if (!CanBuy() || Pending) return;
 Pending = true; Feedback = TEXT("Kauf wird bestaetigt ...");
 if (++NextRequestId == 0) ++NextRequestId;
 ServerPurchase(Item, NextRequestId);
}

void UVanthexBuyMenuComponent::ServerPurchase_Implementation(FName Item, uint32 RequestId)
{
 auto* PC = Cast<AVanthexPlayerController>(GetOwner());
 auto* State = PC ? PC->GetPlayerState<AVanthexPlayerState>() : nullptr;
 const bool Accepted = State && State->TryPurchase(Item, RequestId);
 if (Accepted) if (auto* Pawn = Cast<AVanthexCharacter>(PC->GetPawn())) Pawn->SyncEconomyLoadout(false);
 const FString Reason = Accepted ? FString() : State ? State->GetLastPurchaseError() : TEXT("Spielerstatus ist noch nicht bereit.");
 // A Client RPC on this owned controller component reveals the reason only to its buyer.
 ClientPurchaseResult(RequestId, Accepted, Item, State ? State->GetEconomy().Credits : 0, Reason);
}

void UVanthexBuyMenuComponent::ClientPurchaseResult_Implementation(uint32 RequestId, bool Accepted, FName Item, int32 Credits, const FString& RejectionReason)
{
 if (!RequestId || RequestId <= LastResponseId) return;
 LastResponseId = RequestId; LastAccepted = Accepted; Pending = false;
 const auto* CatalogItem = VanthexEconomy::FindItem(Item);
 Feedback = Accepted ? FString::Printf(TEXT("%s gekauft. %d CR verbleiben."), CatalogItem ? *CatalogItem->DisplayName : *Item.ToString(), Credits)
  : FString::Printf(TEXT("Kauf abgelehnt: %s"), RejectionReason.IsEmpty() ? TEXT("Der Server konnte den Kauf nicht bestaetigen.") : *RejectionReason);
 if (auto* PC = Cast<AVanthexPlayerController>(GetOwner()); PC && PC->GetWeaponAudio()) PC->GetWeaponAudio()->PlayUI(Accepted ? EVanthexWeaponAudioEvent::PurchaseAccepted : EVanthexWeaponAudioEvent::PurchaseRejected);
}

void UVanthexBuyMenuComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta, TickType, Function);
 auto* PC = Cast<AVanthexPlayerController>(GetOwner());
 if (!PC || !PC->IsLocalController()) return;
 const auto* GS = GetWorld()->GetGameState<AVanthexGameState>();
 if (!GS) return;
 const auto Phase = GS->GetPhase();
 if (Phase != LastPhase) {
  if (Phase == EVanthexRoundPhase::BuyPhase) { Feedback.Empty(); Pending = false; Open(); if(PC->GetWeaponAudio()) PC->GetWeaponAudio()->PlayUI(EVanthexWeaponAudioEvent::BuyPhaseStart); }
  else { Close(); if (Phase == EVanthexRoundPhase::RoundActive && PC->GetWeaponAudio()) PC->GetWeaponAudio()->PlayUI(EVanthexWeaponAudioEvent::RoundStart); }
  LastPhase = Phase;
 }
}

void UVanthexBuyMenuComponent::EndPlay(const EEndPlayReason::Type Reason) { Close(); Super::EndPlay(Reason); }
