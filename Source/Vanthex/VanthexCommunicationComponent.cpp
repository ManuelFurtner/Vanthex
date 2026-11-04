#include "VanthexCommunicationComponent.h"
#include "VanthexPlayerController.h"
#include "VanthexPlayerState.h"
#include "VanthexCharacter.h"
#include "VanthexBuyMenuComponent.h"
#include "VanthexLocalSettings.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameStateBase.h"
#include "UnrealClient.h"
#include "VoiceModule.h"
#include "Interfaces/VoiceCapture.h"
#include "Interfaces/VoiceCodec.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundGroups.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

namespace { constexpr int32 SampleRate=16000, FrameBytes=640, MaxPacket=1024; }

class SVanthexChat : public SCompoundWidget
{
public:
 SLATE_BEGIN_ARGS(SVanthexChat) {} SLATE_ARGUMENT(UVanthexCommunicationComponent*, Owner) SLATE_END_ARGS()
 void Construct(const FArguments& Args)
 {
  Owner=Args._Owner;
  ChildSlot.HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(24,0,24,160)
  [SNew(SBox).WidthOverride(460)
   [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
    .BorderBackgroundColor(FLinearColor(.01f,.025f,.035f,.8f)).Padding(12)
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
      .Text_Lambda([this](){return FText::FromString(Owner.IsValid()?Owner->VoiceStatus():FString());})
      .Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).ColorAndOpacity(FLinearColor(.2f,.9f,.8f)).AutoWrapText(true)]
     +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SBox).MaxDesiredHeight(220)
      [SAssignNew(HistoryScroll,SScrollBox)+SScrollBox::Slot()[SNew(STextBlock)
      .Text_Lambda([this](){return FText::FromString(Owner.IsValid()?Owner->History():FString());})
      .Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).AutoWrapText(true)]]]
     +SVerticalBox::Slot().AutoHeight()[SAssignNew(Input,SEditableTextBox)
      .Visibility_Lambda([this](){return Owner.IsValid()&&Owner->IsChatOpen()?EVisibility::Visible:EVisibility::Collapsed;})
      .OnTextChanged_Lambda([this](const FText& Value){if(Value.ToString().Len()>200) Input->SetText(FText::FromString(Value.ToString().Left(200)));})
      .OnTextCommitted_Lambda([this](const FText& Value,ETextCommit::Type Type){
       if(Type==ETextCommit::OnEnter && Owner.IsValid()) Owner->SubmitChat(Value.ToString());
      })]
    ]]];
 }
 void Focus(bool Team)
 {
  Input->SetText(FText::GetEmpty());
  Input->SetHintText(FText::FromString(Team?TEXT("TEAM: Enter senden / Esc abbrechen"):TEXT("ALLE: Enter senden / Esc abbrechen")));
  FSlateApplication::Get().SetKeyboardFocus(Input,EFocusCause::SetDirectly);
  ScrollToLatest();
 }
 void ScrollToLatest() { if(HistoryScroll) HistoryScroll->ScrollToEnd(); }
 virtual FReply OnPreviewKeyDown(const FGeometry& G,const FKeyEvent& E) override
 {
  if(E.GetKey()==EKeys::Escape && Owner.IsValid()) { Owner->CloseChat(); return FReply::Handled(); }
  return SCompoundWidget::OnPreviewKeyDown(G,E);
 }
private:
 TWeakObjectPtr<UVanthexCommunicationComponent> Owner;
 TSharedPtr<SEditableTextBox> Input;
 TSharedPtr<SScrollBox> HistoryScroll;
};

UVanthexCommunicationComponent::UVanthexCommunicationComponent()
{
 SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=true;
}
void UVanthexCommunicationComponent::BeginPlay()
{
 Super::BeginPlay();
 auto* PC=Cast<AVanthexPlayerController>(GetOwner());
 if(PC && PC->IsLocalController() && GetWorld()->GetGameViewport())
 {
  Overlay=SNew(SVanthexChat).Owner(this);
  Overlay->SetVisibility(EVisibility::HitTestInvisible);
  GetWorld()->GetGameViewport()->AddViewportWidgetContent(Overlay.ToSharedRef(),20);
 }
}
FString UVanthexCommunicationComponent::CleanText(const FString& Text,int32 Limit)
{
 FString Out;
 for(TCHAR C:Text.Left(FMath::Clamp(Limit,0,200)))
  if(C>=32 && C!=127 && !(C>=0x200B&&C<=0x200F) && !(C>=0x2028&&C<=0x202E) && !(C>=0x2060&&C<=0x206F)) Out.AppendChar(C);
 return Out.TrimStartAndEnd();
}
bool UVanthexCommunicationComponent::CanReceive(const AVanthexPlayerState* Sender,const AVanthexPlayerState* Recipient,bool TeamOnly)
{
 if(!Sender || !Recipient || Sender->IsABot() || Recipient->IsABot()) return false;
 if(Sender->GetTeam()==EVanthexTeam::None || Recipient->GetTeam()==EVanthexTeam::None) return false;
 // Eliminated players cannot feed spectators' information to living players.
 return (!TeamOnly || Sender->GetTeam()==Recipient->GetTeam()) && (Sender->IsAlive() || !Recipient->IsAlive());
}
void UVanthexCommunicationComponent::OpenTeamChat() { OpenChat(true); }
void UVanthexCommunicationComponent::OpenAllChat() { OpenChat(false); }
void UVanthexCommunicationComponent::OpenChat(bool TeamOnly)
{
 auto* PC=Cast<AVanthexPlayerController>(GetOwner());
 if(!PC || !PC->IsLocalController() || !Overlay || bChatOpen || PC->IsSessionMenuOpen()) return;
 PC->GetBuyMenu()->Close(); PC->HideScoreboard(); StopCapture();
 if(auto* Pawn=Cast<AVanthexCharacter>(PC->GetPawn())) { Pawn->ServerAction(1,false); Pawn->SetAiming(false); Pawn->SetSprinting(false); }
 bChatOpen=true; bTeamChat=TeamOnly;
 Overlay->SetVisibility(EVisibility::SelfHitTestInvisible);
 PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
 PC->SetInputMode(FInputModeUIOnly()); Overlay->Focus(TeamOnly);
}
void UVanthexCommunicationComponent::CloseChat()
{
 if(!bChatOpen) return;
 bChatOpen=false;
 if(Overlay) Overlay->SetVisibility(EVisibility::HitTestInvisible);
 if(auto* PC=Cast<AVanthexPlayerController>(GetOwner()))
 { PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false); PC->SetInputMode(FInputModeGameOnly()); }
}
void UVanthexCommunicationComponent::SubmitChat(const FString& Text)
{
 const FString Clean=CleanText(Text);
 const double Now=FPlatformTime::Seconds();
 if(!Clean.IsEmpty()) {
  if(Now-LastChatSent<1) { AddLine(TEXT("SYSTEM: Bitte kurz warten.")); return; }
  LastChatSent=Now; ServerChat(Clean,bTeamChat);
 }
 CloseChat();
}
void UVanthexCommunicationComponent::ServerChat_Implementation(const FString& Text,bool TeamOnly)
{
 const double Now=FPlatformTime::Seconds();
 if(Text.Len()>200 || Now-LastChatReceived<1) return;
 LastChatReceived=Now;
 const FString Clean=CleanText(Text); if(Clean.IsEmpty()) return;
 const auto* PC=Cast<AVanthexPlayerController>(GetOwner());
 const auto* Sender=PC?PC->GetPlayerState<AVanthexPlayerState>():nullptr;
 for(auto It=GetWorld()->GetPlayerControllerIterator();It;++It)
  if(auto* To=Cast<AVanthexPlayerController>(It->Get()); To && CanReceive(Sender,To->GetPlayerState<AVanthexPlayerState>(),TeamOnly))
   To->GetCommunication()->ClientChat(Sender->GetPlayerId(),CleanText(Sender->GetPlayerName(),32),Clean,TeamOnly);
}
void UVanthexCommunicationComponent::ClientChat_Implementation(int32 SenderId,const FString& Name,const FString& Text,bool TeamOnly)
{
 if(!IsMuted(SenderId)) AddLine(FString::Printf(TEXT("[%s] %s: %s"),TeamOnly?TEXT("TEAM"):TEXT("ALLE"),*CleanText(Name,32),*CleanText(Text)));
}
void UVanthexCommunicationComponent::AddLine(const FString& Line)
{
 Lines.Add(Line); if(Lines.Num()>8) Lines.RemoveAt(0); LastMessage=FPlatformTime::Seconds();
 if(Overlay) Overlay->ScrollToLatest();
}
FString UVanthexCommunicationComponent::History() const
{
 return bChatOpen || FPlatformTime::Seconds()-LastMessage<20 ? FString::Join(Lines,TEXT("\n")) : FString();
}
FString UVanthexCommunicationComponent::VoiceStatus() const
{
 FString Status=TEXT("Y Teamchat / T Alle / V Team-Voice");
 const auto* PC=Cast<AVanthexPlayerController>(GetOwner());
 const auto* PS=PC?PC->GetPlayerState<AVanthexPlayerState>():nullptr;
 if(PS && !PS->IsAlive()) Status+=TEXT(" | Nur ausgeschiedene Spieler hoeren/lesen dich");
 if(!UVanthexLocalSettings::Get()->bVoiceEnabled) Status+=TEXT(" | Voice AUS");
 else if(!MicrophoneError.IsEmpty()) Status+=TEXT(" | ")+MicrophoneError;
 else if(bCapturing) Status+=TEXT(" | MIKROFON AN");
 for(const auto& Pair:Speakers) if(Pair.Value.IsValid() && FPlatformTime::Seconds()-LastHeard.FindRef(Pair.Key)<.4)
  Status+=TEXT(" | ")+CleanText(Pair.Value->GetPlayerName(),32);
 return Status;
}
void UVanthexCommunicationComponent::ToggleMute(int32 Id)
{
 if(Muted.Contains(Id)) Muted.Remove(Id); else { Muted.Add(Id); DropSpeaker(Id); }
}
void UVanthexCommunicationComponent::StopCapture()
{
 if(Capture) { Capture->Stop(); Capture->Shutdown(); Capture.Reset(); }
 PendingPCM.Reset(); bCapturing=false;
}
void UVanthexCommunicationComponent::DropSpeaker(int32 Id)
{
 if(auto* Audio=Playback.FindRef(Id).Get()) { Audio->Stop(); Audio->DestroyComponent(); }
 Playback.Remove(Id); Waves.Remove(Id); Decoders.Remove(Id); LastHeard.Remove(Id); Speakers.Remove(Id);
 // Sequence survives short silence, preventing late packets from reopening a stream.
}
void UVanthexCommunicationComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 auto* PC=Cast<AVanthexPlayerController>(GetOwner()); if(!PC || !PC->IsLocalController()) return;
 auto* Settings=UVanthexLocalSettings::Get();
 const auto* Viewport=GetWorld()->GetGameViewport();
 const bool Focused=Viewport && Viewport->Viewport && Viewport->Viewport->HasFocus() && Viewport->Viewport->IsForegroundWindow();
 if(bChatOpen && FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive()) CloseChat();
 const bool Held=Settings->bVoiceEnabled && Focused && !bChatOpen && !PC->IsSessionMenuOpen() && !PC->GetBuyMenu()->IsOpen() && PC->IsInputKeyDown(EKeys::V);
 if(Held && !bWasVoiceHeld)
 {
  MicrophoneError.Empty();
  Capture=FVoiceModule::Get().CreateVoiceCapture(FString(),SampleRate,1);
  if(!Encoder) Encoder=FVoiceModule::Get().CreateVoiceEncoder(SampleRate,1,EAudioEncodeHint::VoiceEncode_Voice);
  bCapturing=Capture && Encoder && Capture->Start();
  if(!bCapturing) { StopCapture(); MicrophoneError=TEXT("Mikrofon nicht verfuegbar (Windows-Eingabe pruefen)"); }
  else Encoder->SetBitrate(24000);
 }
 if(!Held && bCapturing) StopCapture();
 bWasVoiceHeld=Held;
 TArray<int32> Expired;
 for(const auto& Pair:Speakers)
 {
  if(!Settings->bVoiceEnabled || !CanReceive(Pair.Value.Get(),PC->GetPlayerState<AVanthexPlayerState>(),true) || FPlatformTime::Seconds()-LastHeard.FindRef(Pair.Key)>1)
   Expired.Add(Pair.Key);
  else if(auto* Audio=Playback.FindRef(Pair.Key).Get()) Audio->SetVolumeMultiplier(UVanthexLocalSettings::SafeVolume(Settings->VoiceVolume));
 }
 for(int32 Id:Expired) DropSpeaker(Id);
 if(const auto* GS=GetWorld()->GetGameState())
 {
  for(auto It=ReceivedSequences.CreateIterator();It;++It)
   if(!GS->PlayerArray.ContainsByPredicate([&It](const auto& Player){return Player && Player->GetPlayerId()==It.Key();})) It.RemoveCurrent();
 }
 if(!bCapturing) return;
 uint32 Available=0; const auto State=Capture->GetCaptureState(Available);
 if(State==EVoiceCaptureState::Error) { StopCapture(); MicrophoneError=TEXT("Mikrofon getrennt. V erneut druecken."); return; }
 if(State!=EVoiceCaptureState::Ok || Available==0) return;
 // Discard a stalled capture rather than transmitting delayed speech in a burst.
 if(Available>6400) { StopCapture(); MicrophoneError=TEXT("Voice unterbrochen. V erneut druecken."); return; }
 TArray<uint8> Raw; Raw.SetNumUninitialized(Available); uint32 Written=0;
 if(Capture->GetVoiceData(Raw.GetData(),Available,Written)!=EVoiceCaptureState::Ok || Written>Available) return;
 PendingPCM.Append(Raw.GetData(),Written);
 int32 Frames=0;
 while(PendingPCM.Num()>=FrameBytes && Frames++<3)
 {
  TArray<uint8> Encoded; Encoded.SetNumUninitialized(MaxPacket); uint32 Size=MaxPacket;
  const int32 Remainder=Encoder->Encode(PendingPCM.GetData(),FrameBytes,Encoded.GetData(),Size);
  PendingPCM.RemoveAt(0,FrameBytes,EAllowShrinking::No);
  if(Remainder==0 && Size>0 && Size<=MaxPacket) { Encoded.SetNum(Size); ServerVoice(Encoded,++SendSequence); }
 }
 if(PendingPCM.Num()>=FrameBytes) PendingPCM.Reset();
}
void UVanthexCommunicationComponent::ServerVoice_Implementation(const TArray<uint8>& Data,uint32 Sequence)
{
 if(!VoiceBudget.Accept(Data.Num(),Sequence,FPlatformTime::Seconds())) return;
 auto* PC=Cast<AVanthexPlayerController>(GetOwner());
 auto* Sender=PC?PC->GetPlayerState<AVanthexPlayerState>():nullptr;
 for(auto It=GetWorld()->GetPlayerControllerIterator();It;++It)
  if(auto* To=Cast<AVanthexPlayerController>(It->Get()); To && To!=PC && CanReceive(Sender,To->GetPlayerState<AVanthexPlayerState>(),true))
   To->GetCommunication()->ClientVoice(Sender,Data,Sequence);
}
void UVanthexCommunicationComponent::ClientVoice_Implementation(AVanthexPlayerState* Sender,const TArray<uint8>& Data,uint32 Sequence)
{
 auto* PC=Cast<AVanthexPlayerController>(GetOwner());
 const auto* Settings=UVanthexLocalSettings::Get();
 if(!PC || !PC->IsLocalController() || !Settings->bVoiceEnabled || !CanReceive(Sender,PC->GetPlayerState<AVanthexPlayerState>(),true) || Data.IsEmpty() || Data.Num()>MaxPacket) return;
 const int32 Id=Sender->GetPlayerId(); if(IsMuted(Id)) return;
 if(const auto* Previous=ReceivedSequences.Find(Id); Previous && static_cast<int32>(Sequence-*Previous)<=0) return;
 ReceivedSequences.Add(Id,Sequence);
 if(!Decoders.Contains(Id))
 {
  if(Decoders.Num()>=6) return;
  auto Decoder=FVoiceModule::Get().CreateVoiceDecoder(SampleRate,1); if(!Decoder) return;
  Decoders.Add(Id,Decoder);
 }
 uint8 PCM[6400]; uint32 Size=sizeof(PCM);
 Decoders[Id]->Decode(Data.GetData(),Data.Num(),PCM,Size);
 if(Size==0 || Size>FrameBytes || Size%2!=0) { DropSpeaker(Id); return; }
 if(!Waves.Contains(Id))
 {
  auto* Wave=NewObject<USoundWaveProcedural>(this);
  Wave->SetSampleRate(SampleRate); Wave->NumChannels=1; Wave->Duration=INDEFINITELY_LOOPING_DURATION;
  Wave->SoundGroup=SOUNDGROUP_Voice; Wave->bLooping=false;
  Waves.Add(Id,Wave);
 }
 auto* Wave=Waves[Id].Get();
 if(Wave->GetAvailableAudioByteCount()>6400) Wave->ResetAudio();
 Wave->QueueAudio(PCM,Size);
 if(!Playback.FindRef(Id))
  Playback.Add(Id,UGameplayStatics::SpawnSound2D(this,Wave,UVanthexLocalSettings::SafeVolume(Settings->VoiceVolume),1,0,nullptr,false,false));
 else if(!Playback[Id]->IsPlaying()) Playback[Id]->Play();
 Speakers.Add(Id,Sender); LastHeard.Add(Id,FPlatformTime::Seconds());
}
void UVanthexCommunicationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 CloseChat(); StopCapture();
 TArray<int32> Ids; Waves.GetKeys(Ids); for(int32 Id:Ids) DropSpeaker(Id);
 if(Overlay && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Overlay.ToSharedRef());
 Overlay.Reset(); Encoder.Reset(); Super::EndPlay(Reason);
}
