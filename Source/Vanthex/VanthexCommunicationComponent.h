#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VanthexCommunicationComponent.generated.h"

class AVanthexPlayerState;
class IVoiceCapture;
class IVoiceEncoder;
class IVoiceDecoder;
class USoundWaveProcedural;
class UAudioComponent;
class SVanthexChat;

/** Hard limits before fan-out; voice never enters the reliable RPC queue. */
struct FVanthexVoiceBudget
{
 double Window=0;
 int32 Packets=0, Bytes=0;
 uint32 LastSequence=0;
 bool bHasSequence=false;
 bool Accept(int32 Size,uint32 Sequence,double Now)
 {
  if(Size<=0 || Size>1024 || (bHasSequence && static_cast<int32>(Sequence-LastSequence)<=0)) return false;
  if(Now-Window>=1) { Window=Now; Packets=0; Bytes=0; }
  if(Packets>=60 || Bytes+Size>24000) return false;
  ++Packets; Bytes+=Size; LastSequence=Sequence; bHasSequence=true; return true;
 }
};

/** Owner-only RPC endpoint: the server chooses recipients and sender identity. */
UCLASS()
class UVanthexCommunicationComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVanthexCommunicationComponent();
 void OpenTeamChat();
 void OpenAllChat();
 void CloseChat();
 void SubmitChat(const FString& Text);
 bool IsChatOpen() const { return bChatOpen; }
 void ToggleMute(int32 PlayerId);
 bool IsMuted(int32 PlayerId) const { return Muted.Contains(PlayerId); }
 FString History() const;
 FString VoiceStatus() const;
 static FString CleanText(const FString& Text, int32 Limit=200);
 static bool CanReceive(const AVanthexPlayerState* Sender,const AVanthexPlayerState* Recipient,bool TeamOnly);
 UFUNCTION(Server,Reliable) void ServerChat(const FString& Text,bool TeamOnly);
 UFUNCTION(Client,Reliable) void ClientChat(int32 SenderId,const FString& Name,const FString& Text,bool TeamOnly);
 UFUNCTION(Server,Unreliable) void ServerVoice(const TArray<uint8>& Data,uint32 Sequence);
 UFUNCTION(Client,Unreliable) void ClientVoice(AVanthexPlayerState* Sender,const TArray<uint8>& Data,uint32 Sequence);
 virtual void TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function) override;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 void OpenChat(bool TeamOnly);
 void StopCapture();
 void DropSpeaker(int32 Id);
 void AddLine(const FString& Line);
 bool bChatOpen=false,bTeamChat=true,bWasVoiceHeld=false,bCapturing=false;
 FString MicrophoneError;
 double LastMessage=-100,LastChatSent=-100,LastChatReceived=-100;
 uint32 SendSequence=0;
 FVanthexVoiceBudget VoiceBudget;
 TSet<int32> Muted;
 TArray<FString> Lines;
 TSharedPtr<SVanthexChat> Overlay;
 TSharedPtr<IVoiceCapture> Capture;
 TSharedPtr<IVoiceEncoder> Encoder;
 TArray<uint8> PendingPCM;
 TMap<int32,TSharedPtr<IVoiceDecoder>> Decoders;
 TMap<int32,uint32> ReceivedSequences;
 TMap<int32,double> LastHeard;
 TMap<int32,TWeakObjectPtr<AVanthexPlayerState>> Speakers;
 UPROPERTY(Transient) TMap<int32,TObjectPtr<USoundWaveProcedural>> Waves;
 UPROPERTY(Transient) TMap<int32,TObjectPtr<UAudioComponent>> Playback;
};
