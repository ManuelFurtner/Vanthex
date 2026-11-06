#include "SVanthexSessionMenu.h"
#include "VanthexPlayerController.h"
#include "VanthexCharacter.h"
#include "VanthexLocalSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "VanthexMenuStyle.h"
#include "Engine/World.h"
#include "VanthexGameState.h"
#include "VanthexMapLayout.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

using namespace VanthexMenuStyle;

void SVanthexSessionMenu::Construct(const FArguments& Args)
{
	Controller = Args._Controller;
	Resume = Args._OnResume;
	bFrontEnd = Controller.IsValid() && Controller->IsFrontEnd();
	bCaptureTheFlag = Controller.IsValid() && VanthexMap::IsCTF(Controller->GetWorld());
	if (auto* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Quality = Settings->GetOverallScalabilityLevel();
		FrameLimit = FMath::RoundToInt(Settings->GetFrameRateLimit()); bVSync = Settings->IsVSyncEnabled();
	}
	const FString MenuTitle = bFrontEnd ? TEXT("HAUPTMENÜ") : TEXT("PARTIEMENÜ");
	auto Navigation = SNew(SVerticalBox);
	Navigation->AddSlot().AutoHeight().Padding(0,0,0,6)[Text(TEXT("VANTHEX"),30,Cyan)];
	Navigation->AddSlot().AutoHeight().Padding(0,0,0,34)[Text(TEXT("TACTICAL TEAM OPERATIONS"),11,Muted)];
	const TCHAR* Tabs[] = {TEXT("SPIELEN"), TEXT("EINSTELLUNGEN"), TEXT("NETZWERK")};
	for (int32 I = 0; I < UE_ARRAY_COUNT(Tabs); ++I)
		Navigation->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(Tabs[I],[this,I](){ShowPage(I); return FReply::Handled();},false,I)];
	Navigation->AddSlot().FillHeight(1)[SNew(SBox)];
	Navigation->AddSlot().AutoHeight().Padding(0,24,0,10)[Text(TEXT("CORE 3v3 / CAPTURE THE FLAG 5v5"),12,Cyan)];
	if (bFrontEnd)
	{
		Navigation->AddSlot().AutoHeight()[Button(TEXT("SPIEL BEENDEN"), [this]()
		{
			if (auto* PC = Controller.Get())
			{
				UVanthexLocalSettings::Get()->Persist();
				UKismetSystemLibrary::QuitGame(PC, PC, EQuitPreference::Quit, false);
			}
			return FReply::Handled();
		})];
	}
	else
	{
		Navigation->AddSlot().AutoHeight()[Text(TEXT("ESC  Zurück ins Spiel"), 12, Muted)];
	}
	ChildSlot[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.003f,.007f,.01f,.96f)).Padding(20)
	[SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
	[SNew(SBox).WidthOverride(1120).HeightOverride(690)
	[SNew(SHorizontalBox)
	+SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(290)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel).Padding(26)[Navigation]]]
	+SHorizontalBox::Slot().FillWidth(1).Padding(2,0,0,0)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(34)
	[SNew(SVerticalBox)
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Text(MenuTitle,13,Cyan)]
	+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,22)[SNew(STextBlock).Text_Lambda([this](){return Status();}).Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).ColorAndOpacity(Muted)]
	+SVerticalBox::Slot().FillHeight(1)[SAssignNew(Pages,SWidgetSwitcher)
	+SWidgetSwitcher::Slot()[PlayPage()]
	+SWidgetSwitcher::Slot()[SettingsPage()]
	+SWidgetSwitcher::Slot()[NetworkPage()]]
	+SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)[SAssignNew(Feedback,STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).ColorAndOpacity(FLinearColor(1.f,.67f,.27f)).AutoWrapText(true)]
	]]]]]];
}

TSharedRef<SWidget> SVanthexSessionMenu::Button(const FString& Label, TFunction<FReply()> Action, bool Primary, int32 Tab)
{
	return SNew(SButton).ContentPadding(FMargin(18,13)).HAlign(HAlign_Left)
		.ButtonColorAndOpacity_Lambda([this,Primary,Tab](){return Primary || (Tab>=0 && Tab==ActivePage) ? FLinearColor(.035f,.3f,.29f,1) : FLinearColor(.055f,.082f,.10f,1);})
		.OnClicked_Lambda([Action=MoveTemp(Action)](){return Action();})[Text(Label,16)];
}

TSharedRef<SWidget> SVanthexSessionMenu::ModeCard(bool bCTF)
{
    const FLinearColor Accent = bCTF ? FLinearColor(.95f, .67f, .34f) : Cyan;
    return SNew(SButton)
        .ButtonStyle(FCoreStyle::Get(), "NoBorder")
        .ContentPadding(0)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        .OnClicked_Lambda([this, bCTF]()
        {
            bCaptureTheFlag = bCTF;
            PendingAction.Reset();
            Feedback->SetText(FText::GetEmpty());
            return FReply::Handled();
        })
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor_Lambda([this, bCTF]()
            {
                return bCaptureTheFlag == bCTF ? FLinearColor(.04f, .15f, .17f) : Panel;
            })
            .Padding(18)
            [
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                [Text(bCTF ? TEXT("5 GEGEN 5  /  CROSSROADS") : TEXT("3 GEGEN 3  /  GRID V4"), 12, Accent)]
                +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                [Text(bCTF ? TEXT("Capture the Flag") : TEXT("Core"), 25)]
                +SVerticalBox::Slot().FillHeight(1).Padding(0, 0, 0, 14)
                [Text(bCTF ? TEXT("Erobere die Flagge in der römischen Arena.") : TEXT("Sichere den Core und bring ihn zur Extraktion."), 15, Muted)]
                +SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                    .ColorAndOpacity(Accent)
                    .Text_Lambda([this, bCTF]()
                    {
                        return FText::FromString(bCaptureTheFlag == bCTF ? TEXT("AUSGEWÄHLT") : TEXT("AUSWÄHLEN"));
                    })
                ]
            ]
        ];
}

TSharedRef<SWidget> SVanthexSessionMenu::PlayPage()
{
    auto Content = SNew(SVerticalBox);
    if (!bFrontEnd)
    {
        Content->AddSlot().AutoHeight().Padding(0, 0, 0, 22)
        [Button(TEXT("PARTIE FORTSETZEN"), [this]() { Resume.ExecuteIfBound(); return FReply::Handled(); }, true)];
    }
    Content->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[Text(TEXT("Spielmodus wählen"), 30)];
    Content->AddSlot().AutoHeight().Padding(0, 0, 0, 18)
    [Text(TEXT("Wähle eine Arena und starte eine lokale Partie mit Bots."), 16, Muted)];
    Content->AddSlot().AutoHeight().Padding(0, 0, 0, 18)
    [
        SNew(SBox).MinDesiredHeight(166)
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 8, 0)[ModeCard(false)]
            +SHorizontalBox::Slot().FillWidth(1).Padding(8, 0, 0, 0)[ModeCard(true)]
        ]
    ];
    Content->AddSlot().AutoHeight().Padding(0, 0, 0, 10)
    [Button(TEXT("PARTIE STARTEN"), [this]() { return RequestTravel(ETravelAction::Play); }, true)];
    if (bFrontEnd)
    {
        Content->AddSlot().AutoHeight()
        [Button(TEXT("SOLO-TRAINING"), [this]()
        {
            if (auto* PC = Controller.Get()) PC->StartTraining();
            return FReply::Handled();
        })];
    }
    if (!bFrontEnd)
    {
        Content->AddSlot().AutoHeight()
        [Button(TEXT("ZUM HAUPTMENÜ"), [this]() { return RequestTravel(ETravelAction::MainMenu); })];
    }
    return SNew(SScrollBox) + SScrollBox::Slot()[Content];
}

TSharedRef<SWidget> SVanthexSessionMenu::NetworkPage()
{
    return SNew(SScrollBox) + SScrollBox::Slot()
    [
        SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[Text(TEXT("Multiplayer"), 30)]
        +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
        [
            SNew(STextBlock)
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
            .ColorAndOpacity(Muted)
            .Text_Lambda([this]()
            {
                return FText::FromString(bCaptureTheFlag ? TEXT("Ausgewählter Modus  /  Capture the Flag") : TEXT("Ausgewählter Modus  /  Core"));
            })
        ]
        +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 24)
        [Button(TEXT("LAN-PARTIE HOSTEN"), [this]() { return RequestTravel(ETravelAction::Host); }, true)]
        +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[Text(TEXT("DIREKT BEITRETEN"), 12, Cyan)]
        +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
        [
            SAssignNew(AddressBox, SEditableTextBox)
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
            .Padding(FMargin(14, 12))
            .Text(FText::FromString(UVanthexLocalSettings::Get()->LastAddress))
            .HintText(FText::FromString(TEXT("IP-Adresse oder Hostname")))
            .OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type Commit)
            {
                if (Commit == ETextCommit::OnEnter) Join();
            })
        ]
        +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
        [Button(TEXT("VERBINDEN"), [this]() { return Join(); })]
        +SVerticalBox::Slot().AutoHeight()
        [Text(TEXT("Der Host muss erreichbar sein. Standardport ist 7777."), 13, Muted)]
    ];
}

TSharedRef<SWidget> SVanthexSessionMenu::ControlsPage()
{
	auto List = SNew(SVerticalBox);
	auto* Settings = UVanthexLocalSettings::Get();
	List->AddSlot().AutoHeight().Padding(0, 0, 0, 14)[Text(TEXT("Maus"), 22)];
	List->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
	[NumberRow(TEXT("Maus-Empfindlichkeit"), &Settings->MouseSensitivity, .1f, 5.f, 1.f, .01f)];
	List->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
	[NumberRow(TEXT("Empfindlichkeit beim Zielen (Faktor)"), &Settings->ADSMouseSensitivity, .1f, 3.f, 1.f, .01f)];
	List->AddSlot().AutoHeight().Padding(0, 0, 0, 14)
	[Text(TEXT("Der Faktor wirkt beim Blick durchs Visier. Standard ist 1,00."), 13, Muted)];
	List->AddSlot().AutoHeight().Padding(0, 0, 0, 24)
	[Button(TEXT("MAUS AUF STANDARD"), [Settings]()
	{
		Settings->MouseSensitivity = 1.f;
		Settings->ADSMouseSensitivity = 1.f;
		Settings->Persist();
		return FReply::Handled();
	})];
	List->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[Text(TEXT("Tastenbelegung"), 22)];
	List->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
	[Text(TEXT("Belegung anklicken und eine Taste oder Maustaste dr\u00fccken. Esc bricht die Auswahl ab."), 13, Muted)];
	List->AddSlot().AutoHeight().Padding(0, 0, 0, 14)
	[Button(TEXT("TASTEN AUF STANDARD"), [this, Settings]()
	{
		Settings->ResetGameplayKeys();
		Settings->Persist();
		RefreshLocalGameplayBindings();
		if (Feedback.IsValid()) Feedback->SetText(NSLOCTEXT("VanthexControls", "KeysReset", "Standardbelegung wiederhergestellt."));
		return FReply::Handled();
	})];
	for (const auto& Binding : UVanthexLocalSettings::GameplayBindings())
	{
		const auto Action = Binding.Action;
		List->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel).Padding(12, 6)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Binding.Label).Font(FCoreStyle::GetDefaultFontStyle("Regular", 14)).ColorAndOpacity(White)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(172)
					[
						SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(7, 6))
						.OnClicked_Lambda([this, Action]()
						{
							PendingKeyBinding = Action;
							if (auto Scroll = ControlsScroll.Pin()) Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Never);
							if (Feedback.IsValid()) Feedback->SetText(NSLOCTEXT("VanthexControls", "CaptureHint", "Neue Taste dr\u00fccken. Esc bricht ab."));
							return FReply::Handled().SetUserFocus(AsShared());
						})
						[
							SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).ColorAndOpacity(Cyan)
							.Text_Lambda([this, Settings, Action]()
							{
								return PendingKeyBinding.IsSet() && PendingKeyBinding.GetValue() == Action
									? NSLOCTEXT("VanthexControls", "CaptureKey", "Taste dr\u00fccken")
									: Settings->GetGameplayKey(Action).GetDisplayName();
							})
						]
					]
				]
			]
		];
	}
	List->AddSlot().AutoHeight().Padding(0, 12, 0, 0)
	[Text(TEXT("Feste Tasten: B Kaufmen\u00fc / Tab Teamstand / Esc Men\u00fc / Y Teamchat / T Alle / V Team-Voice / F5 Neustart"), 13, Muted)];

	auto Scroll = SNew(SScrollBox).AnimateWheelScrolling(true) + SScrollBox::Slot().Padding(0, 0, 12, 0)[List];
	ControlsScroll = Scroll;
	return Scroll;
}

void SVanthexSessionMenu::RefreshLocalGameplayBindings()
{
	const auto* PC = Controller.Get();
	if (auto* Character = PC ? Cast<AVanthexCharacter>(PC->GetPawn()) : nullptr)
		Character->RefreshGameplayBindings();
}

void SVanthexSessionMenu::EndKeyCapture()
{
	PendingKeyBinding.Reset();
	if (auto Scroll = ControlsScroll.Pin()) Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
}

FReply SVanthexSessionMenu::CaptureGameplayKey(const FKey& Key)
{
	const auto Action = PendingKeyBinding.GetValue();
	EndKeyCapture();
	FText Error;
	auto* Settings = UVanthexLocalSettings::Get();
	if (Settings->TrySetGameplayKey(Action, Key, Error))
	{
		Settings->Persist();
		RefreshLocalGameplayBindings();
		Error = NSLOCTEXT("VanthexControls", "KeySaved", "Taste gespeichert.");
	}
	if (Feedback.IsValid()) Feedback->SetText(Error);
	return FReply::Handled();
}

void SVanthexSessionMenu::ShowPage(int32 Page)
{
	EndKeyCapture();
	ActivePage = Page;
	PendingAction.Reset();
	Pages->SetActiveWidgetIndex(Page);
	Feedback->SetText(FText::GetEmpty());
}

FReply SVanthexSessionMenu::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (PendingKeyBinding.IsSet())
    {
        if (Event.GetKey() == EKeys::Escape)
        {
            EndKeyCapture();
            if (Feedback.IsValid()) Feedback->SetText(FText::GetEmpty());
            return FReply::Handled();
        }
        return Event.IsRepeat() ? FReply::Handled() : CaptureGameplayKey(Event.GetKey());
    }
    if (Event.GetKey() != EKeys::Escape)
    {
        return SCompoundWidget::OnPreviewKeyDown(Geometry, Event);
    }
    if (bFrontEnd)
    {
        ShowPage(0);
        return FReply::Handled().SetUserFocus(AsShared());
    }
    Resume.ExecuteIfBound();
    return FReply::Handled();
}

FReply SVanthexSessionMenu::OnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    return PendingKeyBinding.IsSet() ? CaptureGameplayKey(Event.GetEffectingButton()) : SCompoundWidget::OnPreviewMouseButtonDown(Geometry, Event);
}

FReply SVanthexSessionMenu::OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
    const float Delta = Event.GetWheelDelta();
    if (PendingKeyBinding.IsSet() && !FMath::IsNearlyZero(Delta))
        return CaptureGameplayKey(Delta > 0.f ? EKeys::MouseScrollUp : EKeys::MouseScrollDown);
    return SCompoundWidget::OnMouseWheel(Geometry, Event);
}

FReply SVanthexSessionMenu::RequestTravel(ETravelAction Action)
{
    if (!bFrontEnd && (!PendingAction.IsSet() || PendingAction.GetValue() != Action))
    {
        PendingAction = Action;
        Feedback->SetText(FText::FromString(TEXT("Die aktuelle Partie endet. Wähle dieselbe Schaltfläche erneut, um fortzufahren.")));
        return FReply::Handled();
    }
    if (auto* PC = Controller.Get())
    {
        if (Action == ETravelAction::MainMenu) PC->LeaveGame();
        else PC->StartGame(bCaptureTheFlag, Action == ETravelAction::Host);
    }
    return FReply::Handled();
}

FReply SVanthexSessionMenu::Join()
{
    FString Address, Error;
    if (!UVanthexLocalSettings::ValidateAddress(AddressBox->GetText().ToString(), Address, Error))
    {
        Feedback->SetText(FText::FromString(Error));
        return FReply::Handled();
    }
    if (auto* PC = Controller.Get())
    {
        Feedback->SetText(FText::FromString(TEXT("Verbindung wird aufgebaut …")));
        PC->JoinGame(Address);
    }
    return FReply::Handled();
}

FText SVanthexSessionMenu::Status() const
{
	const auto* PC=Controller.Get();
	const auto* GS=PC&&PC->GetWorld()?PC->GetWorld()->GetGameState<AVanthexGameState>():nullptr;
	return FText::FromString(GS?FString::Printf(TEXT("ALPHA %d : %d BRAVO     /     RUNDE %d     /     %02d:%02d"),GS->GetAlphaScore(),GS->GetBravoScore(),GS->GetRoundIndex(),FMath::Max(0,FMath::CeilToInt(GS->GetTimeRemaining()))/60,FMath::Max(0,FMath::CeilToInt(GS->GetTimeRemaining()))%60):TEXT("LOKAL SPIELEN  /  LAN  /  EINSTELLUNGEN"));
}

namespace
{
class SCrosshairPreview final : public SLeafWidget
{
public:
 SLATE_BEGIN_ARGS(SCrosshairPreview) {} SLATE_END_ARGS()
 void Construct(const FArguments&) { ForceVolatile(true); }
 virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(500,140); }
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,
  int32 Layer,const FWidgetStyle&,bool) const override
 {
  const auto Style=UVanthexLocalSettings::Get()->GetCrosshairStyle();
  const auto Size=G.GetLocalSize();
  const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
  // Two backgrounds make dark and bright colors easy to evaluate.
  FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),Brush,ESlateDrawEffect::None,FLinearColor(.025f,.04f,.055f));
  FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(Size.X*.5f,Size.Y),FSlateLayoutTransform(FVector2D(Size.X*.5f,0))),Brush,ESlateDrawEffect::None,FLinearColor(.32f,.36f,.38f));
  for(float X:{.25f,.75f}) for(const auto& R:Style.Rects())
   FSlateDrawElement::MakeBox(Out,Layer+1,G.ToPaintGeometry(FVector2D(R.Z,R.W),FSlateLayoutTransform(FVector2D(Size.X*X+R.X,Size.Y*.5f+R.Y))),Brush,ESlateDrawEffect::None,Style.Color);
  return Layer+1;
 }
};
}

TSharedRef<SWidget> SVanthexSessionMenu::NumberRow(const FString& Label,float* Value,float Min,float Max,float Fallback,float Step)
{
 return SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(Label,15)]
  +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(140)
   [SNew(SSpinBox<float>).MinValue(Min).MaxValue(Max).MinSliderValue(Min).MaxSliderValue(Max)
    .Delta(Step>0?Step:(Max<=1?.01f:.5f)).MinFractionalDigits(0).MaxFractionalDigits(2)
    .Value_Lambda([Value,Min,Max,Fallback](){return FVanthexCrosshairStyle::Safe(*Value,Min,Max,Fallback);})
    .OnValueChanged_Lambda([Value,Min,Max,Fallback](float V){*Value=FVanthexCrosshairStyle::Safe(V,Min,Max,Fallback);})
    .OnValueCommitted_Lambda([Value,Min,Max,Fallback](float V,ETextCommit::Type){*Value=FVanthexCrosshairStyle::Safe(V,Min,Max,Fallback); UVanthexLocalSettings::Get()->Persist();})
    .OnEndSliderMovement_Lambda([](float){UVanthexLocalSettings::Get()->Persist();})]];
}

TSharedRef<SWidget> SVanthexSessionMenu::CrosshairPage()
{
 auto* Settings=UVanthexLocalSettings::Get();
 auto Presets=SNew(SHorizontalBox);
 const FLinearColor Colors[]={FLinearColor::White,FLinearColor(.1f,1.f,.15f),Cyan,FLinearColor(1.f,.85f,.05f),FLinearColor(1.f,.15f,.1f),FLinearColor(1.f,.1f,.8f)};
 const TCHAR* Names[]={TEXT("Weiss"),TEXT("Gruen"),TEXT("Cyan"),TEXT("Gelb"),TEXT("Rot"),TEXT("Pink")};
 for(int32 I=0;I<6;++I)
 {
  const auto Color=Colors[I];
  Presets->AddSlot().FillWidth(1).Padding(0,0,6,0)[SNew(SButton).ContentPadding(8).ButtonColorAndOpacity(Color)
   .ToolTipText(FText::FromString(Names[I]))
   .OnClicked_Lambda([Settings,Color](){Settings->CrosshairColor=Color; Settings->Persist(); return FReply::Handled();})
   [Text(Names[I],12,FLinearColor::Black)]];
 }
 auto Rows=SNew(SVerticalBox);
 Rows->AddSlot().AutoHeight().Padding(0,0,0,16)[Text(TEXT("DEIN FADENKREUZ"),27)];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,10)[SNew(SBox).HeightOverride(140)[SNew(SCrosshairPreview)]];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,20)[Text(TEXT("Live-Vorschau auf dunklem und hellem Hintergrund. Aenderungen gelten sofort und werden lokal gespeichert."),13,Muted)];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,12)[NumberRow(TEXT("Dicke (1-8 px)"),&Settings->CrosshairThickness,1,8,1.5f)];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,12)[NumberRow(TEXT("Laenge pro Strich (2-32 px)"),&Settings->CrosshairLength,2,32,8)];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,20)[NumberRow(TEXT("Abstand zur Mitte (0-24 px)"),&Settings->CrosshairGap,0,24,0)];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,10)[Text(TEXT("FARBE"),13,Cyan)];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,14)[Presets];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,10)[NumberRow(TEXT("Rot (0-1)"),&Settings->CrosshairColor.R,0,1,1)];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,10)[NumberRow(TEXT("Gruen (0-1)"),&Settings->CrosshairColor.G,0,1,1)];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,18)[NumberRow(TEXT("Blau (0-1)"),&Settings->CrosshairColor.B,0,1,1)];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,14)[Button(TEXT("FADENKREUZ ZURUECKSETZEN"),[Settings](){Settings->ResetCrosshair();return FReply::Handled();})];
 Rows->AddSlot().AutoHeight()[Text(TEXT("Betrifft das normale Fadenkreuz beim Schiessen ohne Visier. Reflexpunkt, Zielfernrohr und Trefferanzeige behalten ihre eigene Darstellung. Der Abstand wird von der Mitte bis zur inneren Strichkante gemessen."),12,Muted)];
 return SNew(SScrollBox)+SScrollBox::Slot()[Rows];
}
