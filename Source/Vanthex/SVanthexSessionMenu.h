#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AVanthexPlayerController;
class SWidgetSwitcher;
class SEditableTextBox;
class STextBlock;
class SScrollBox;
struct FKey;
enum class EVanthexGameplayAction : uint8;

/** Native game UI. A single focus root consumes Escape and owns all tab content. */
class SVanthexSessionMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVanthexSessionMenu) {}
		SLATE_ARGUMENT(AVanthexPlayerController*, Controller)
		SLATE_EVENT(FSimpleDelegate, OnResume)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply OnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
private:
	TWeakObjectPtr<AVanthexPlayerController> Controller;
	FSimpleDelegate Resume;
	TSharedPtr<SWidgetSwitcher> Pages;
	TSharedPtr<SEditableTextBox> AddressBox;
	TSharedPtr<STextBlock> Feedback;
	TWeakPtr<SScrollBox> ControlsScroll;
	TOptional<EVanthexGameplayAction> PendingKeyBinding;
	int32 ActivePage = 0;
	enum class ETravelAction { Play, Host, MainMenu };
	TOptional<ETravelAction> PendingAction;
	bool bFrontEnd = false;
	bool bCaptureTheFlag = false;
	int32 Quality = 2;
	int32 FrameLimit = 144;
	bool bVSync = false;
	TSharedRef<SWidget> PlayPage();
	TSharedRef<SWidget> NetworkPage();
	TSharedRef<SWidget> ModeCard(bool bCTF);
	TSharedRef<SWidget> VideoPage();
	TSharedRef<SWidget> AudioPage();
	TSharedRef<SWidget> SettingsPage();
	TSharedRef<SWidget> ControlsPage();
 TSharedRef<SWidget> CrosshairPage();
 TSharedRef<SWidget> NumberRow(const FString& Label,float* Value,float Min,float Max,float Fallback,float Step = 0.f);
	TSharedRef<SWidget> Button(const FString& Label, TFunction<FReply()> Action, bool Primary=false, int32 Tab=-1);
	TSharedRef<SWidget> VolumeRow(const FString& Label, float* Value);
	void ShowPage(int32 Page);
	FReply CaptureGameplayKey(const FKey& Key);
	void EndKeyCapture();
	void RefreshLocalGameplayBindings();
	FReply RequestTravel(ETravelAction Action);
	FReply Join();
	FText Status() const;
};
