#include "SVanthexSessionMenu.h"
#include "VanthexMenuStyle.h"
#include "Algo/Find.h"
#include "VanthexCommunicationComponent.h"
#include "VanthexLocalSettings.h"
#include "VanthexPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

using namespace VanthexMenuStyle;

namespace
{
const FLinearColor SelectedOption(.025f, .32f, .3f);

TSharedRef<SWidget> SettingRow(const FString& Label, const TSharedRef<SWidget>& Control)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(Label, 16)]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).MinDesiredWidth(180)[Control]
		];
}

struct FDisplaySelection
{
	EWindowMode::Type Mode = EWindowMode::Windowed;
	FIntPoint Resolution = FIntPoint::ZeroValue;
	FIntPoint Desktop = FIntPoint::ZeroValue;
	TArray<FIntPoint> WindowSizes;
	bool bPending = false;
};
}

TSharedRef<SWidget> SVanthexSessionMenu::SettingsPage()
{
	auto Options = SNew(SWidgetSwitcher)
		+ SWidgetSwitcher::Slot()[VideoPage()]
		+ SWidgetSwitcher::Slot()[AudioPage()]
		+ SWidgetSwitcher::Slot()[ControlsPage()]
		+ SWidgetSwitcher::Slot()[CrosshairPage()];
	auto Tabs = SNew(SHorizontalBox);
	const TCHAR* Labels[] = {TEXT("Grafik"), TEXT("Audio"), TEXT("Maus & Steuerung"), TEXT("Fadenkreuz")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Labels); ++Index)
	{
		Tabs->AddSlot().FillWidth(Index == 2 ? 1.55f : 1.f).Padding(0, 0, 8, 0)
		[
			SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(8, 11))
			.ButtonColorAndOpacity_Lambda([Options, Index]()
			{
				return Options->GetActiveWidgetIndex() == Index ? SelectedOption : Panel;
			})
			.OnClicked_Lambda([this, Options, Index]()
			{
				Options->SetActiveWidgetIndex(Index);
				if (Feedback.IsValid()) Feedback->SetText(FText::GetEmpty());
				return FReply::Handled();
			})
			[Text(Labels[Index], 14)]
		];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)[Text(TEXT("EINSTELLUNGEN"), 28)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)[Tabs]
		+ SVerticalBox::Slot().FillHeight(1)[Options];
}

TSharedRef<SWidget> SVanthexSessionMenu::VideoPage()
{
	auto Display = MakeShared<FDisplaySelection>();
	const auto* PC = Controller.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const auto* Video = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const bool bCanChangeDisplay = Video && World && World->WorldType != EWorldType::PIE;
	if (Video)
	{
		Display->Mode = Video->GetFullscreenMode();
		Display->Resolution = Video->GetScreenResolution();
		Display->Desktop = Video->GetDesktopResolution();
		if (bCanChangeDisplay)
		{
			UKismetSystemLibrary::GetConvenientWindowedResolutions(Display->WindowSizes);
			Display->WindowSizes.RemoveAll([Display](const FIntPoint& Size)
			{
				return Size.X < 800 || Size.Y < 450 || Size.X >= Display->Desktop.X || Size.Y >= Display->Desktop.Y;
			});
			if (Display->Resolution.X >= 800 && Display->Resolution.Y >= 450 &&
				Display->Resolution.X < Display->Desktop.X && Display->Resolution.Y < Display->Desktop.Y)
			{
				Display->WindowSizes.AddUnique(Display->Resolution);
			}
			Display->WindowSizes.Sort([](const FIntPoint& A, const FIntPoint& B)
			{
				return A.X == B.X ? A.Y < B.Y : A.X < B.X;
			});
		}
	}

	auto Presets = SNew(SHorizontalBox);
	const TCHAR* QualityNames[] = {TEXT("Niedrig"), TEXT("Mittel"), TEXT("Hoch"), TEXT("Episch")};
	for (int32 Level = 0; Level < UE_ARRAY_COUNT(QualityNames); ++Level)
	{
		Presets->AddSlot().FillWidth(1).Padding(0, 0, 8, 0)
		[
			SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(10, 12))
			.ButtonColorAndOpacity_Lambda([this, Level]() { return Quality == Level ? SelectedOption : Panel; })
			.OnClicked_Lambda([this, Level]() { Quality = Level; return FReply::Handled(); })
			[Text(QualityNames[Level], 15)]
		];
	}

	auto ModeControl = SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(12, 10))
		.IsEnabled(bCanChangeDisplay && !Display->WindowSizes.IsEmpty())
		.ToolTipText(FText::FromString(TEXT("Zwischen Fenster und randlosem Vollbild wechseln")))
		.OnClicked_Lambda([Display]()
		{
			Display->Mode = Display->Mode == EWindowMode::Windowed ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;
			if (Display->Mode == EWindowMode::Windowed && !Display->WindowSizes.Contains(Display->Resolution))
				Display->Resolution = Display->WindowSizes.Last();
			Display->bPending = true;
			return FReply::Handled();
		})
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 15)).ColorAndOpacity(Cyan).Text_Lambda([Display]()
			{
				const TCHAR* Name = Display->Mode == EWindowMode::Windowed ? TEXT("Fenster") :
					Display->Mode == EWindowMode::WindowedFullscreen ? TEXT("Randlos") : TEXT("Vollbild");
				return FText::FromString(Name);
			})
		];

	auto ResolutionControl = SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(12, 10))
		.IsEnabled_Lambda([Display, bCanChangeDisplay]()
		{
			return bCanChangeDisplay && Display->Mode == EWindowMode::Windowed && Display->WindowSizes.Num() > 1;
		})
		.ToolTipText(FText::FromString(TEXT("Naechste Fensteraufloesung auswaehlen")))
		.OnClicked_Lambda([Display]()
		{
			const int32 Current = Display->WindowSizes.IndexOfByKey(Display->Resolution);
			Display->Resolution = Display->WindowSizes[(Current + 1) % Display->WindowSizes.Num()];
			Display->bPending = true;
			return FReply::Handled();
		})
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 15)).ColorAndOpacity(Cyan).Text_Lambda([Display]()
			{
				const FIntPoint Size = Display->Mode == EWindowMode::WindowedFullscreen ? Display->Desktop : Display->Resolution;
				return FText::FromString(Size.X > 0 && Size.Y > 0 ? FString::Printf(TEXT("%d x %d"), Size.X, Size.Y) : TEXT("Automatisch"));
			})
		];

	auto FrameControl = SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(12, 10))
		.ToolTipText(FText::FromString(TEXT("Naechstes FPS-Limit auswaehlen")))
		.OnClicked_Lambda([this]()
		{
			const int32 Limits[] = {0, 60, 120, 144, 240};
			const int32* Current = Algo::Find(Limits, FrameLimit);
			FrameLimit = Limits[Current ? (Current - Limits + 1) % UE_ARRAY_COUNT(Limits) : 1];
			return FReply::Handled();
		})
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 15)).ColorAndOpacity(Cyan).Text_Lambda([this]()
			{
				return FText::FromString(FrameLimit > 0 ? FString::Printf(TEXT("%d FPS"), FrameLimit) : TEXT("Unbegrenzt"));
			})
		];

	return SNew(SScrollBox).AnimateWheelScrolling(true)
		+ SScrollBox::Slot().Padding(0, 0, 12, 0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[Text(TEXT("GRAFIKQUALITAET"), 12, Cyan)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)[Presets]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[SettingRow(TEXT("Bildmodus"), ModeControl)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[SettingRow(TEXT("Aufloesung"), ResolutionControl)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[SettingRow(TEXT("FPS-Limit"), FrameControl)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bVSync ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bVSync = State == ECheckBoxState::Checked; })
				[Text(TEXT("  VSync"), 15)]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Button(TEXT("GRAFIK ANWENDEN"), [this, Display, bCanChangeDisplay]()
				{
					if (auto* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
					{
						if (Quality >= 0) Settings->SetOverallScalabilityLevel(Quality);
						Settings->SetVSyncEnabled(bVSync);
						Settings->SetFrameRateLimit(FrameLimit);
						if (Display->bPending && bCanChangeDisplay)
						{
							Settings->SetFullscreenMode(Display->Mode);
							Settings->SetScreenResolution(Display->Mode == EWindowMode::Windowed ? Display->Resolution : Display->Desktop);
							Settings->ApplyResolutionSettings(false);
							Settings->ConfirmVideoMode();
							Display->bPending = false;
						}
						Settings->ApplyNonResolutionSettings();
						Settings->SaveSettings();
						if (Feedback.IsValid()) Feedback->SetText(FText::FromString(TEXT("Grafikeinstellungen gespeichert.")));
					}
					return FReply::Handled();
				}, true)
			]
		];
}

TSharedRef<SWidget> SVanthexSessionMenu::AudioPage()
{
	auto* Settings = UVanthexLocalSettings::Get();
	auto Mutes = SNew(SVerticalBox);
	Mutes->AddSlot().AutoHeight().Padding(0, 4, 0, 10)[Text(TEXT("SPIELER STUMMSCHALTEN"), 12, Cyan)];
	const auto* PC = Controller.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const auto* Match = World ? World->GetGameState() : nullptr;
	bool bHasPlayers = false;
	if (Match && PC->GetCommunication())
	{
		for (const auto& State : Match->PlayerArray)
		{
			if (!State || State->IsABot() || State == PC->PlayerState) continue;
			bHasPlayers = true;
			const int32 Id = State->GetPlayerId();
			const FString Name = UVanthexCommunicationComponent::CleanText(State->GetPlayerName(), 32);
			Mutes->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this, Id]()
				{
					const auto* Owner = Controller.Get();
					const auto* Comms = Owner ? Owner->GetCommunication() : nullptr;
					return Comms && Comms->IsMuted(Id) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this, Id](ECheckBoxState State)
				{
					const auto* Owner = Controller.Get();
					auto* Comms = Owner ? Owner->GetCommunication() : nullptr;
					if (Comms && Comms->IsMuted(Id) != (State == ECheckBoxState::Checked)) Comms->ToggleMute(Id);
				})
				[Text(Name, 14)]
			];
		}
	}
	if (!bHasPlayers)
		Mutes->AddSlot().AutoHeight()[Text(TEXT("Andere Spieler erscheinen hier in einer Mehrspielersitzung."), 13, Muted)];

	return SNew(SScrollBox).AnimateWheelScrolling(true)
		+ SScrollBox::Slot().Padding(0, 0, 12, 0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[Text(TEXT("LAUTSTAERKE"), 12, Cyan)]
			+ SVerticalBox::Slot().AutoHeight()[VolumeRow(TEXT("Gesamtlautstaerke"), &Settings->MasterVolume)]
			+ SVerticalBox::Slot().AutoHeight()[VolumeRow(TEXT("Waffen & Bewegung"), &Settings->CombatVolume)]
			+ SVerticalBox::Slot().AutoHeight()[VolumeRow(TEXT("Menue & Kaufhinweise"), &Settings->UIVolume)]
			+ SVerticalBox::Slot().AutoHeight()[VolumeRow(TEXT("Waffen-Raumreflexionen"), &Settings->RoomVolume)]
			+ SVerticalBox::Slot().AutoHeight()[VolumeRow(TEXT("Team-Voice"), &Settings->VoiceVolume)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([Settings]() { return Settings->bVoiceEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([Settings](ECheckBoxState State)
				{
					Settings->bVoiceEnabled = State == ECheckBoxState::Checked;
					Settings->Persist();
				})
				[Text(TEXT("  Team-Voice aktivieren. V zum Sprechen halten."), 15)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)[Mutes]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Button(TEXT("UI-TESTTON"), [this]()
				{
					const auto* Owner = Controller.Get();
					if (auto* Audio = Owner ? Owner->GetWeaponAudio() : nullptr)
						Audio->PlayUI(EVanthexWeaponAudioEvent::BuyPhaseStart);
					return FReply::Handled();
				})
			]
		];
}

TSharedRef<SWidget> SVanthexSessionMenu::VolumeRow(const FString& Label, float* Value)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1)[Text(Label, 15)]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 15)).ColorAndOpacity(Cyan).Text_Lambda([Value]()
				{
					return FText::AsPercent(UVanthexLocalSettings::SafeVolume(*Value));
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
		[
			SNew(SSlider).SliderHandleColor(Cyan)
			.Value_Lambda([Value]() { return UVanthexLocalSettings::SafeVolume(*Value); })
			.OnValueChanged_Lambda([this, Value](float NewValue)
			{
				*Value = NewValue;
				const auto* PC = Controller.Get();
				UVanthexLocalSettings::Get()->ApplyAudio(PC ? PC->GetWorld() : nullptr);
			})
			.OnMouseCaptureEnd_Lambda([]() { UVanthexLocalSettings::Get()->Persist(); })
			.OnControllerCaptureEnd_Lambda([]() { UVanthexLocalSettings::Get()->Persist(); })
		];
}
