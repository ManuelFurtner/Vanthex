#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VanthexCommunicationComponent.h"
#include "VanthexPlayerState.h"
#include "Engine/World.h"
#include "VoiceModule.h"
#include "Interfaces/VoiceCodec.h"

namespace { constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter; }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexChatTextTest,"Vanthex.Communication.TextBounds",Flags)
bool FVanthexChatTextTest::RunTest(const FString&)
{
 TestEqual(TEXT("Whitespace-only input is empty"),UVanthexCommunicationComponent::CleanText(TEXT(" \r\n\t ")),FString());
 TestEqual(TEXT("Single-line display, no bidi override"),UVanthexCommunicationComponent::CleanText(TEXT(" A\nB\tC\x202E")),FString(TEXT("ABC")));
 TestEqual(TEXT("Unicode text is retained"),UVanthexCommunicationComponent::CleanText(TEXT("Gr\u00fc\u00dfe / oru\u017eje")),FString(TEXT("Gr\u00fc\u00dfe / oru\u017eje")));
 TestEqual(TEXT("Server message length is bounded"),UVanthexCommunicationComponent::CleanText(FString::ChrN(500,TEXT('a'))).Len(),200);
 TestEqual(TEXT("Player names are bounded independently"),UVanthexCommunicationComponent::CleanText(FString::ChrN(80,TEXT('b')),32).Len(),32);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexChatRoutingTest,"Vanthex.Communication.RecipientPolicy",Flags)
bool FVanthexChatRoutingTest::RunTest(const FString&)
{
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
  .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
 if(!TestNotNull(TEXT("test world"),World)) return false;
 auto* Sender=World->SpawnActor<AVanthexPlayerState>();
 auto* Recipient=World->SpawnActor<AVanthexPlayerState>();
 if(!Sender || !Recipient) { AddError(TEXT("Cannot spawn test players")); World->DestroyWorld(false); return false; }
 TestFalse(TEXT("Unassigned players cannot transmit"),UVanthexCommunicationComponent::CanReceive(Sender,Recipient,true));
 Sender->SetTeam(EVanthexTeam::Alpha,0); Recipient->SetTeam(EVanthexTeam::Bravo,0);
 TestFalse(TEXT("Opponent cannot hear team voice or read team chat"),UVanthexCommunicationComponent::CanReceive(Sender,Recipient,true));
 TestTrue(TEXT("Opponent can read all chat"),UVanthexCommunicationComponent::CanReceive(Sender,Recipient,false));
 Recipient->SetTeam(EVanthexTeam::Alpha,1);
 TestTrue(TEXT("Team member can hear/read alive teammate"),UVanthexCommunicationComponent::CanReceive(Sender,Recipient,true));
 Sender->SetAlive(false);
 TestFalse(TEXT("Dead-to-live voice blocked"),UVanthexCommunicationComponent::CanReceive(Sender,Recipient,true));
 TestFalse(TEXT("Dead-to-live all chat blocked"),UVanthexCommunicationComponent::CanReceive(Sender,Recipient,false));
 Recipient->SetAlive(false);
 TestTrue(TEXT("Eliminated teammates can communicate"),UVanthexCommunicationComponent::CanReceive(Sender,Recipient,true));
 Sender->SetAlive(true);
 TestTrue(TEXT("Alive teammates can speak to eliminated teammate"),UVanthexCommunicationComponent::CanReceive(Sender,Recipient,true));
 TestFalse(TEXT("Disconnected player rejected"),UVanthexCommunicationComponent::CanReceive(nullptr,Recipient,true));
 World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexVoiceBudgetTest,"Vanthex.Communication.VoiceBudget",Flags)
bool FVanthexVoiceBudgetTest::RunTest(const FString&)
{
 FVanthexVoiceBudget Budget;
 TestFalse(TEXT("Empty payload rejected"),Budget.Accept(0,1,10));
 TestFalse(TEXT("Oversize payload rejected"),Budget.Accept(1025,1,10));
 for(uint32 Seq=1;Seq<=60;++Seq) TestTrue(TEXT("Normal traffic fits"),Budget.Accept(400,Seq,10));
 TestFalse(TEXT("Packet budget stops flooding"),Budget.Accept(1,61,10));
 TestTrue(TEXT("Budget replenishes"),Budget.Accept(400,61,11));
 TestFalse(TEXT("Duplicate dropped"),Budget.Accept(400,61,11));
 TestFalse(TEXT("Out-of-order dropped"),Budget.Accept(400,60,11));
 FVanthexVoiceBudget Bytes;
 for(uint32 Seq=1;Seq<=23;++Seq) TestTrue(TEXT("Bounded large packets"),Bytes.Accept(1024,Seq,10));
 TestFalse(TEXT("Byte budget also enforced"),Bytes.Accept(1024,24,10));
 FVanthexVoiceBudget Wrap;
 TestTrue(TEXT("Sequence near wrap"),Wrap.Accept(100,MAX_uint32,10));
 TestTrue(TEXT("Sequence wraps safely"),Wrap.Accept(100,0,10));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexVoiceCodecTest,"Vanthex.Communication.CodecRoundTrip",Flags)
bool FVanthexVoiceCodecTest::RunTest(const FString&)
{
 // No microphone: validates the exact wire frame size against the platform codec.
 auto Encoder=FVoiceModule::Get().CreateVoiceEncoder(16000,1,EAudioEncodeHint::VoiceEncode_Voice);
 auto Decoder=FVoiceModule::Get().CreateVoiceDecoder(16000,1);
 if(!Encoder || !Decoder) { AddError(TEXT("Voice codec unavailable on this platform")); return false; }
 Encoder->SetBitrate(24000);
 for(int32 Frame=0;Frame<6;++Frame)
 {
  int16 PCM[320]; for(int32 I=0;I<320;++I) PCM[I]=static_cast<int16>(8000*FMath::Sin((Frame*320+I)*.17f));
  uint8 Encoded[1024]; uint32 Size=sizeof(Encoded);
  TestEqual(TEXT("20ms frame completely encoded"),Encoder->Encode(reinterpret_cast<uint8*>(PCM),sizeof(PCM),Encoded,Size),0);
  if(!TestTrue(TEXT("Packet fits wire limit"),Size>0 && Size<=sizeof(Encoded))) return false;
  uint8 Decoded[6400]; uint32 DecodedSize=sizeof(Decoded);
  Decoder->Decode(Encoded,Size,Decoded,DecodedSize);
  TestEqual(TEXT("Decoder returns 20ms mono PCM16"),DecodedSize,uint32(sizeof(PCM)));
 }
 return true;
}
#endif
