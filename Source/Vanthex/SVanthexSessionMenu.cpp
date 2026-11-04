#include "SVanthexSessionMenu.h"
#include "VanthexPlayerController.h"
#include "VanthexLocalSettings.h"
#include "VanthexCommunicationComponent.h"
#include "VanthexPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "VanthexGameState.h"
#include "VanthexMapLayout.h"
#include "Engine/Engine.h"
#include "Engine/GameUserSettings.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

namespace
{
const FLinearColor Ink(.012f,.019f,.027f,1), Panel(.024f,.037f,.05f,1), Cyan(.14f,.82f,.78f,1), Muted(.5f,.6f,.65f,1), White(.88f,.93f,.94f,1);
TSharedRef<STextBlock> Text(const FString& Value, int32 Size=16, FLinearColor Color=White)
{
	return SNew(STextBlock).Text(FText::FromString(Value)).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).ColorAndOpacity(Color).AutoWrapText(true);
}
}

void SVanthexSessionMenu::Construct(const FArguments& Args)
{
	Controller = Args._Controller; Resume = Args._OnResume;
	if (auto* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Quality = Settings->GetOverallScalabilityLevel();
		FrameLimit = FMath::RoundToInt(Settings->GetFrameRateLimit()); bVSync = Settings->IsVSyncEnabled();
	}
	const FString Map = Controller.IsValid() && VanthexMap::IsGridV4(Controller->GetWorld()) ? TEXT("AEGIS / GRID V4") : TEXT("VANTHEX FACILITY");
	auto Navigation = SNew(SVerticalBox);
	Navigation->AddSlot().AutoHeight().Padding(0,0,0,6)[Text(TEXT("V A N T H E X"),30,Cyan)];
	Navigation->AddSlot().AutoHeight().Padding(0,0,0,34)[Text(TEXT("TACTICAL CORE OPERATIONS"),11,Muted)];
	const TCHAR* Tabs[] = {TEXT("01   SPIELEN"),TEXT("02   EINSTELLUNGEN"),TEXT("03   STEUERUNG")};
	for (int32 I=0; I<3; ++I)
		Navigation->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(Tabs[I],[this,I](){ShowPage(I); return FReply::Handled();},false,I)];
	Navigation->AddSlot().FillHeight(1)[SNew(SBox)];
	Navigation->AddSlot().AutoHeight().Padding(0,24,0,10)[Text(TEXT("3 vs 3  /  CORE  /  TAKTIK"),12,Cyan)];
	Navigation->AddSlot().AutoHeight()[Text(TEXT("ESC  Zurueck ins Spiel\nDie laufende Runde geht weiter."),12,Muted)];
	ChildSlot[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.003f,.007f,.01f,.96f)).Padding(20)
	[SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
	[SNew(SBox).WidthOverride(1120).HeightOverride(690)
	[SNew(SHorizontalBox)
	+SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(290)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel).Padding(26)[Navigation]]]
	+SHorizontalBox::Slot().FillWidth(1).Padding(2,0,0,0)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(34)
	[SNew(SVerticalBox)
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Text(Map,13,Cyan)]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,22)[SNew(STextBlock).Text_Lambda([this](){return Status();}).Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).ColorAndOpacity(Muted)]
	+SVerticalBox::Slot().FillHeight(1)[SAssignNew(Pages,SWidgetSwitcher)
	+SWidgetSwitcher::Slot()[PlayPage()]
	+SWidgetSwitcher::Slot()[SettingsPage()]
	+SWidgetSwitcher::Slot()[ControlsPage()]]
	+SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)[SAssignNew(Feedback,STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).ColorAndOpacity(FLinearColor(1.f,.67f,.27f)).AutoWrapText(true)]
	]]]]]];
}

TSharedRef<SWidget> SVanthexSessionMenu::Button(const FString& Label, TFunction<FReply()> Action, bool Primary, int32 Tab)
{
	return SNew(SButton).ContentPadding(FMargin(18,13)).HAlign(HAlign_Left)
		.ButtonColorAndOpacity_Lambda([this,Primary,Tab](){return Primary || (Tab>=0 && Tab==ActivePage) ? FLinearColor(.035f,.3f,.29f,1) : FLinearColor(.055f,.082f,.10f,1);})
		.OnClicked_Lambda([Action=MoveTemp(Action)](){return Action();})[Text(Label,16)];
}

TSharedRef<SWidget> SVanthexSessionMenu::PlayPage()
{
	return SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[Text(TEXT("BEREIT FUER DIE NAECHSTE RUNDE?"),27)]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,24)[Text(TEXT("Sichere den Core. Decke dein Team. Kontrolliere die Anlage."),15,Muted)]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("WEITERSPIELEN"),[this](){Resume.ExecuteIfBound(); return FReply::Handled();},true)]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,22)[Button(TEXT("LAN-SITZUNG HOSTEN"),[this](){return RequestTravel(1);})]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Text(TEXT("DIREKT BEITRETEN"),12,Cyan)]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SAssignNew(AddressBox,SEditableTextBox)
		.Font(FCoreStyle::GetDefaultFontStyle("Regular",18)).Padding(FMargin(14,12))
		.Text(FText::FromString(UVanthexLocalSettings::Get()->LastAddress))
		.HintText(FText::FromString(TEXT("192.168.1.10:7777 oder Hostname")))
		.OnTextCommitted_Lambda([this](const FText&,ETextCommit::Type Commit){if(Commit==ETextCommit::OnEnter) Join();})]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("VERBINDEN"),[this](){return Join();})]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,24)[Text(TEXT("Direkte IP-Verbindung. Der Host muss erreichbar sein; Port 7777 ist Standard."),12,Muted)]
	+SVerticalBox::Slot().AutoHeight()[Button(TEXT("SITZUNG VERLASSEN / LOKAL SPIELEN"),[this](){return RequestTravel(2);})]
	];
}

TSharedRef<SWidget> SVanthexSessionMenu::VolumeRow(const FString& Label,float* Value)
{
	return SNew(SVerticalBox)
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SNew(SHorizontalBox)
	+SHorizontalBox::Slot().FillWidth(1)[Text(Label,15)]
	+SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text_Lambda([Value](){return FText::AsPercent(UVanthexLocalSettings::SafeVolume(*Value));}).ColorAndOpacity(Cyan)]]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)[SNew(SSlider).Value_Lambda([Value](){return UVanthexLocalSettings::SafeVolume(*Value);})
		.OnValueChanged_Lambda([this,Value](float NewValue){*Value=NewValue; if(Controller.IsValid()) UVanthexLocalSettings::Get()->ApplyAudio(Controller->GetWorld());})];
}

TSharedRef<SWidget> SVanthexSessionMenu::SettingsPage()
{
	auto* Settings = UVanthexLocalSettings::Get();
 auto Mutes=SNew(SVerticalBox);
 Mutes->AddSlot().AutoHeight().Padding(0,4,0,10)[Text(TEXT("SPIELER STUMMSCHALTEN (TEXT + VOICE)"),12,Cyan)];
 if(Controller.IsValid() && Controller->GetWorld()->GetGameState())
  for(const auto& State:Controller->GetWorld()->GetGameState()->PlayerArray)
   if(State && !State->IsABot() && State!=Controller->PlayerState)
   {
    const int32 Id=State->GetPlayerId();
    const FString Name=UVanthexCommunicationComponent::CleanText(State->GetPlayerName(),32);
    Mutes->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SCheckBox)
     .IsChecked_Lambda([this,Id](){return Controller.IsValid() && Controller->GetCommunication()->IsMuted(Id)?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
     .OnCheckStateChanged_Lambda([this,Id](ECheckBoxState){if(Controller.IsValid()) Controller->GetCommunication()->ToggleMute(Id);})[Text(Name,14)]];
   }
	auto QualityButtons=SNew(SHorizontalBox);
	const TCHAR* Names[]={TEXT("Niedrig"),TEXT("Mittel"),TEXT("Hoch"),TEXT("Episch")};
	for(int32 I=0; I<4; ++I) QualityButtons->AddSlot().FillWidth(1).Padding(0,0,6,0)
	[Button(Names[I],[this,I](){Quality=I; Feedback->SetText(FText::FromString(TEXT("Grafik ausgewaehlt. Mit Grafik anwenden bestaetigen."))); return FReply::Handled();})];
	return SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)[Text(TEXT("DEIN SETUP"),27)]
	+SVerticalBox::Slot().AutoHeight()[VolumeRow(TEXT("Gesamtlautstaerke"),&Settings->MasterVolume)]
	+SVerticalBox::Slot().AutoHeight()[VolumeRow(TEXT("Waffen & Bewegung"),&Settings->CombatVolume)]
	+SVerticalBox::Slot().AutoHeight()[VolumeRow(TEXT("Menue & Kaufhinweise"),&Settings->UIVolume)]
	+SVerticalBox::Slot().AutoHeight()[VolumeRow(TEXT("Waffen-Raumreflexionen"),&Settings->RoomVolume)]
 +SVerticalBox::Slot().AutoHeight()[VolumeRow(TEXT("Team-Voice"),&Settings->VoiceVolume)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[SNew(SCheckBox)
  .IsChecked_Lambda([Settings](){return Settings->bVoiceEnabled?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
  .OnCheckStateChanged_Lambda([Settings](ECheckBoxState State){Settings->bVoiceEnabled=State==ECheckBoxState::Checked;})
  [Text(TEXT("  Voice aktivieren / V zum Sprechen halten"),15)]]
 +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)[Mutes]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)[Button(TEXT("UI-TESTTON"),[this](){if(Controller.IsValid()) Controller->GetWeaponAudio()->PlayUI(EVanthexWeaponAudioEvent::BuyPhaseStart); return FReply::Handled();})]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(STextBlock).Text_Lambda([this](){return FText::FromString(FString::Printf(TEXT("GRAFIKQUALITAET: %s"), Quality<0?TEXT("Benutzerdefiniert"):Quality==0?TEXT("Niedrig"):Quality==1?TEXT("Mittel"):Quality==2?TEXT("Hoch"):TEXT("Episch")));}).ColorAndOpacity(Cyan)]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)[QualityButtons]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)[SNew(SCheckBox).IsChecked_Lambda([this](){return bVSync?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
		.OnCheckStateChanged_Lambda([this](ECheckBoxState State){bVSync=State==ECheckBoxState::Checked;})[Text(TEXT("  VSync"),15)]]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)[SNew(SHorizontalBox)
	+SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("FPS-LIMIT WECHSELN"),[this](){FrameLimit=FrameLimit==60?120:FrameLimit==120?144:FrameLimit==144?240:FrameLimit==240?0:60; return FReply::Handled();})]
	+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14,0)[SNew(STextBlock).Text_Lambda([this](){return FText::FromString(FrameLimit>0?FString::Printf(TEXT("%d FPS"),FrameLimit):TEXT("Unbegrenzt"));}).ColorAndOpacity(Cyan)]]
	+SVerticalBox::Slot().AutoHeight()[Button(TEXT("GRAFIK ANWENDEN"),[this](){
		if(auto* Video=GEngine?GEngine->GetGameUserSettings():nullptr) {
			if(Quality>=0) Video->SetOverallScalabilityLevel(Quality);
			Video->SetVSyncEnabled(bVSync); Video->SetFrameRateLimit(FrameLimit); Video->ApplyNonResolutionSettings(); Video->SaveSettings();
			Feedback->SetText(FText::FromString(TEXT("Einstellungen gespeichert. Audio wird sofort uebernommen.")));
		} return FReply::Handled();},true)]
	];
}

TSharedRef<SWidget> SVanthexSessionMenu::ControlsPage()
{
	auto List=SNew(SVerticalBox);
	List->AddSlot().AutoHeight().Padding(0,0,0,20)[Text(TEXT("BEWEGUNG. TEAMWORK. CORE."),26)];
	const TCHAR* Keys[]={TEXT("W A S D / MAUS"),TEXT("SHIFT / STRG / LEERTASTE"),TEXT("LMB / RMB / R"),TEXT("1 / 2 / 3 / MAUSRAD"),TEXT("E / G / Q"),TEXT("4 / 5"),TEXT("F / C"),TEXT("B / TAB / ESC")};
	const TCHAR* Actions[]={TEXT("Bewegen / Umsehen"),TEXT("Sprinten / Ducken / Springen"),TEXT("Feuern / Zielen / Nachladen"),TEXT("Primaer / Pistole / Messer / Wechseln"),TEXT("Core aufnehmen / Ablegen / Werfen"),TEXT("Gadget verwenden"),TEXT("SHIFT / Operator-Faehigkeit (wo verfuegbar)"),TEXT("Kaufmenue / Teamstand / Menue")};
	for(int32 I=0; I<8; ++I) List->AddSlot().AutoHeight().Padding(0,0,0,2)
	[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel).Padding(14,12)
	[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(.46f)[Text(Keys[I],13,Cyan)]+SHorizontalBox::Slot().FillWidth(.54f)[Text(Actions[I],14)]]];
 List->AddSlot().AutoHeight().Padding(14,12)[Text(TEXT("Y: Teamchat   /   T: Alle   /   V halten: Team-Voice\nEnter senden / Esc abbrechen. Stummschaltung unter Einstellungen."),14,Cyan)];
	return SNew(SScrollBox)+SScrollBox::Slot()[List];
}

void SVanthexSessionMenu::ShowPage(int32 Page)
{
	ActivePage=Page; PendingAction=0; Pages->SetActiveWidgetIndex(Page);
	Feedback->SetText(FText::GetEmpty());
}

FReply SVanthexSessionMenu::OnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
	if(Event.GetKey()==EKeys::Escape) { Resume.ExecuteIfBound(); return FReply::Handled(); }
	return SCompoundWidget::OnPreviewKeyDown(Geometry,Event);
}

FReply SVanthexSessionMenu::RequestTravel(int32 Action)
{
	if(PendingAction!=Action) { PendingAction=Action; Feedback->SetText(FText::FromString(TEXT("Die aktuelle Sitzung wird verlassen. Zum Bestaetigen dieselbe Schaltflaeche erneut waehlen."))); return FReply::Handled(); }
	if(auto* PC=Controller.Get()) { Resume.ExecuteIfBound(); if(Action==1) PC->HostGame(); else PC->LeaveGame(); }
	return FReply::Handled();
}

FReply SVanthexSessionMenu::Join()
{
	FString Address,Error;
	if(!UVanthexLocalSettings::ValidateAddress(AddressBox->GetText().ToString(),Address,Error))
	{ Feedback->SetText(FText::FromString(Error)); return FReply::Handled(); }
	UVanthexLocalSettings::Get()->LastAddress=Address; UVanthexLocalSettings::Get()->Persist();
	if(auto* PC=Controller.Get()) { Resume.ExecuteIfBound(); PC->JoinGame(Address); }
	return FReply::Handled();
}

FText SVanthexSessionMenu::Status() const
{
	const auto* PC=Controller.Get();
	const auto* GS=PC&&PC->GetWorld()?PC->GetWorld()->GetGameState<AVanthexGameState>():nullptr;
	return FText::FromString(GS?FString::Printf(TEXT("ALPHA %d : %d BRAVO     /     RUNDE %d     /     %02d:%02d"),GS->GetAlphaScore(),GS->GetBravoScore(),GS->GetRoundIndex(),FMath::Max(0,FMath::CeilToInt(GS->GetTimeRemaining()))/60,FMath::Max(0,FMath::CeilToInt(GS->GetTimeRemaining()))%60):TEXT("SITZUNG WIRD VORBEREITET"));
}
