#include "VanthexPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexBotController.h"
#include "VanthexBuyMenuComponent.h"
#include "VanthexCharacter.h"
#include "VanthexGadgetActor.h"
#include "VanthexGadgetComponent.h"
#include "VanthexGameState.h"
#include "VanthexPlayerState.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

namespace
{
const FVector NetworkGadgetFloor(-50000,0,4950);

AVanthexCharacter* GadgetNetworkPawn(UWorld* World,EVanthexTeam Team,int32 Slot)
{
 for(TActorIterator<AVanthexCharacter> It(World);It;++It)
  if(const auto* PS=It->GetPlayerState<AVanthexPlayerState>();PS&&PS->GetTeam()==Team&&PS->GetSpawnSlot()==Slot)return *It;
 return nullptr;
}

int32 NetworkGadgetCount(UWorld* World,FName Id,bool ActivatedOnly=false)
{
 int32 Count=0;
 for(TActorIterator<AVanthexGadgetActor> It(World);It;++It)if(IsValid(*It)&&It->GadgetId==Id&&(!ActivatedOnly||It->bActivated))++Count;
 return Count;
}

int32 NetworkMarks(AVanthexCharacter* Character)
{
 if(!Character||!Character->GetGadgets())return 0;
 const auto* Component=Character->GetGadgets();int32 Count=0;
 for(const auto& Mark:Component->GetMarks())if(Mark.Expires>Component->GetServerTime())++Count;
 return Count;
}

bool InitialGadgets(const AVanthexPlayerState* State)
{
 if(!State)return false;const auto& Inventory=State->GetEconomy();
 return Inventory.Credits==250&&Inventory.Gadgets.Num()==2&&Inventory.Gadgets[0]==TEXT("Sensor")&&Inventory.Gadgets[1]==TEXT("Smoke");
}

struct FGadgetNetworkProbe
{
 FTimerHandle Timer;
 TWeakObjectPtr<AActor> Floor;
 TWeakObjectPtr<AVanthexCharacter> Participant;
 double Started=0,Ready=-1,BuyRequestAt=-1,ActiveAt=-1,SmokeAt=-1,FlashAt=-1;
 bool FloorReady=false,Joined=false,SentBuy=false,Purchased=false,BuyBlocked=false,SentBuyThrow=false,Placed=false;
 bool Crouch=false,Stand=false,SentSensor=false,SentSmoke=false,SensorReplicated=false,SmokeReplicated=false;
 bool ConsumedOnce=false,Marks=false,MarkPrivacy=false,Flash=false,FlashPrivacy=false,FlashSent=false,Returned=false,Finished=false;
 int32 Credits=-1,Inventory=-1,SensorCount=0,SmokeCount=0,MarkCount=0;
 float Capsule=0,Camera=0,FlashPower=0;
 void Finish(UWorld* World,bool Host,bool Timeout)
 {
  if(Finished)return;Finished=true;
  const bool Pass=!Timeout&&FloorReady&&Joined&&Purchased&&BuyBlocked&&Crouch&&Stand&&SensorReplicated&&SmokeReplicated
   &&ConsumedOnce&&Marks&&MarkPrivacy&&Flash&&FlashPrivacy&&(Host?Returned:SentBuy&&SentSensor&&SentSmoke);
  const FString Report=FString::Printf(TEXT("%s role=%s joined=%d floor=%d purchased250=%d buy_blocked=%d crouch60_camera42=%d standing96_camera64=%d sensor_replicated=%d smoke_replicated=%d consume_once=%d marks=%d marks_private=%d flash=%d flash_private=%d returned_bot=%d sent_buy=%d sent_sensor=%d sent_smoke=%d credits=%d inventory=%d sensors=%d smokes=%d active_marks=%d capsule=%.1f camera=%.1f flash_strength=%.3f elapsed=%.2f timeout=%d\n"),
   Pass?TEXT("PASS"):TEXT("FAIL"),Host?TEXT("host"):TEXT("client"),Joined,FloorReady,Purchased,BuyBlocked,Crouch,Stand,SensorReplicated,SmokeReplicated,ConsumedOnce,Marks,MarkPrivacy,Flash,FlashPrivacy,Returned,SentBuy,SentSensor,SentSmoke,Credits,Inventory,SensorCount,SmokeCount,MarkCount,Capsule,Camera,FlashPower,World->GetTimeSeconds()-Started,Timeout);
  const bool Saved=FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/(Host?TEXT("GadgetNetworkHost.txt"):TEXT("GadgetNetworkClient.txt"))));
  UE_LOG(LogVanthex,Display,TEXT("GADGET_NETWORK %s saved=%d"),*Report,Saved);
  if(Floor.IsValid())Floor->Destroy();
  const uint8 ExitCode=Pass&&Saved?0:1;
  World->GetTimerManager().ClearTimer(Timer);
  FPlatformMisc::RequestExitWithStatus(false,ExitCode);
 }
};
}
#endif

void VanthexStartGadgetNetworkTest(AVanthexPlayerController* Controller)
{
#if !UE_BUILD_SHIPPING
 if(!Controller||!Controller->IsLocalController()||!Controller->GetWorld())return;
 auto* World=Controller->GetWorld();
 const TWeakObjectPtr<AVanthexPlayerController> WeakController(Controller);
 const auto Probe=MakeShared<FGadgetNetworkProbe>();Probe->Started=World->GetTimeSeconds();
 FActorSpawnParameters FloorParameters;FloorParameters.Name=TEXT("GadgetNetworkFloorFixture");
 auto* Floor=World->SpawnActor<AActor>(FVector::ZeroVector,FRotator::ZeroRotator,FloorParameters);
 if(Floor)
 {
  Floor->SetNetAddressable();
  auto* Box=NewObject<UBoxComponent>(Floor,TEXT("GadgetNetworkFloor"));Floor->SetRootComponent(Box);Floor->AddInstanceComponent(Box);Box->SetNetAddressable();
  Box->SetBoxExtent(FVector(6000,6000,50));Box->SetCollisionProfileName(TEXT("BlockAll"));Box->RegisterComponent();Floor->SetActorLocation(NetworkGadgetFloor-FVector(0,0,50));Box->SetMobility(EComponentMobility::Static);
  Probe->Floor=Floor;Probe->FloorReady=true;
 }
 World->GetTimerManager().SetTimer(Probe->Timer,FTimerDelegate::CreateLambda([WeakController,Probe]()
 {
  const auto Lifetime=Probe;
  auto* PC=WeakController.Get();if(!PC||Probe->Finished)return;auto* W=PC->GetWorld();if(!W)return;
  const bool Host=PC->HasAuthority();const double Now=W->GetTimeSeconds();
  if(Now-Probe->Started>90.){Probe->Finish(W,Host,true);return;}
  const auto* GS=W->GetGameState<AVanthexGameState>();if(!GS)return;
  const bool Buy=GS->GetPhase()==EVanthexRoundPhase::BuyPhase,Active=GS->GetPhase()==EVanthexRoundPhase::RoundActive;
  auto* Human=Host?GadgetNetworkPawn(W,EVanthexTeam::Bravo,0):Cast<AVanthexCharacter>(PC->GetPawn());
  auto* HostPawn=GadgetNetworkPawn(W,EVanthexTeam::Alpha,0);
  auto* Ally=GadgetNetworkPawn(W,EVanthexTeam::Bravo,1);
  auto* State=Human?Human->GetPlayerState<AVanthexPlayerState>():nullptr;
  if(!Human||!State||!Human->GetGadgets()||!HostPawn||!Ally)return;
  if(Host&&Probe->Joined&&Cast<AVanthexBotController>(Human->GetController()))
  {
   Probe->Returned=Probe->ConsumedOnce&&State->GetEconomy().Credits==250&&State->GetEconomy().Gadgets.IsEmpty();
   Probe->Finish(W,true,false);return;
  }
  if(Host&&!Cast<AVanthexPlayerController>(Human->GetController()))return;
  if(!Probe->Joined)
  {
   if(!Buy||State->GetTeam()!=EVanthexTeam::Bravo||State->GetSpawnSlot()!=0)return;
   Probe->Participant=Human;Probe->Joined=true;Probe->Ready=Now;
  }
  Probe->Credits=State->GetEconomy().Credits;Probe->Inventory=State->GetEconomy().Gadgets.Num();
  if(!Host&&Buy&&!Probe->SentBuy&&Now-Probe->Ready>.4&&PC->GetBuyMenu())
  {
   Probe->SentBuy=true;PC->GetBuyMenu()->ServerPurchase(TEXT("Sensor"),1);PC->GetBuyMenu()->ServerPurchase(TEXT("Smoke"),2);
   PC->GetBuyMenu()->ServerPurchase(TEXT("Smoke"),2);
  }
  if(Buy&&InitialGadgets(State))
  {
   Probe->Purchased=true;
   if(Probe->BuyRequestAt<0.)Probe->BuyRequestAt=Now;
   if(!Host&&!Probe->SentBuyThrow){Probe->SentBuyThrow=true;Human->GetGadgets()->ServerThrow(0,1);}
   if(Now-Probe->BuyRequestAt>.7)Probe->BuyBlocked=NetworkGadgetCount(W,TEXT("Sensor"))==0&&NetworkGadgetCount(W,TEXT("Smoke"))==0;
  }
  if(!Active||!Probe->Purchased)return;
  if(Host&&!Probe->Placed)
  {
   int32 Index=0;
   for(TActorIterator<AVanthexCharacter> It(W);It;++It)
   {
    FVector Location=NetworkGadgetFloor+FVector(3500,1000+Index*400,96);
    if(*It==Human)Location=NetworkGadgetFloor+FVector(-400,0,96);
    else if(*It==HostPawn)Location=NetworkGadgetFloor+FVector(600,0,96);
    else if(*It==Ally)Location=NetworkGadgetFloor+FVector(-400,400,96);
    It->GetCharacterMovement()->StopMovementImmediately();It->SetActorLocation(Location,false,nullptr,ETeleportType::TeleportPhysics);It->ForceNetUpdate();++Index;
   }
   HostPawn->GetController()->SetControlRotation(FRotator(0,180,0));
   Probe->Placed=true;Probe->ActiveAt=Now;
  }
  if(!Host&&Probe->ActiveAt<0.)
  {
   if(FVector::Dist2D(Human->GetActorLocation(),NetworkGadgetFloor+FVector(-400,0,0))>100.f)return;
   Probe->Placed=true;Probe->ActiveAt=Now;PC->SetControlRotation(FRotator::ZeroRotator);Human->SetCrouching(true);
  }
  if(Probe->ActiveAt<0.)return;
  const double Age=Now-Probe->ActiveAt;
  Probe->Capsule=Human->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();Probe->Camera=float(Human->GetFirstPersonCamera()->GetRelativeLocation().Z);
  if(Human->bIsCrouched&&FMath::IsNearlyEqual(Probe->Capsule,60.f)&&FMath::IsNearlyEqual(Probe->Camera,42.f))Probe->Crouch=true;
  if(!Host&&Age>=1.5&&Age<2.5)Human->SetCrouching(false);
  if(Probe->Crouch&&!Human->bIsCrouched&&FMath::IsNearlyEqual(Probe->Capsule,96.f)&&FMath::IsNearlyEqual(Probe->Camera,64.f))Probe->Stand=true;
  if(!Host&&Age>=3.&&Probe->Stand&&!Probe->SentSensor)
  {
   Probe->SentSensor=true;Human->GetGadgets()->ServerThrow(0,2);Human->GetGadgets()->ServerThrow(0,2);
  }
  Probe->SensorCount=NetworkGadgetCount(W,TEXT("Sensor"));Probe->SmokeCount=NetworkGadgetCount(W,TEXT("Smoke"));
  if(Probe->SensorCount==1&&Probe->Inventory==1&&State->GetEconomy().Gadgets[0]==TEXT("Smoke"))Probe->SensorReplicated=true;
  if(NetworkGadgetCount(W,TEXT("Sensor"),true)==1&&NetworkMarks(Human)>0)
  {
   Probe->Marks=true;
   // Server sees ally marks; client must not receive that other pawn's private array.
   Probe->MarkPrivacy=NetworkMarks(HostPawn)==0&&(Host?NetworkMarks(Ally)>0:NetworkMarks(Ally)==0);
  }
  Probe->MarkCount=NetworkMarks(Human);
  if(!Host&&Age>=6.&&Probe->Marks&&Probe->MarkPrivacy&&!Probe->SentSmoke)
  {
   Probe->SentSmoke=true;PC->SetControlRotation(FRotator(0,180,0));
   // The earlier replay used ID 2; ID 3 must consume precisely the remaining Smoke.
   Human->GetGadgets()->ServerThrow(0,3);Human->GetGadgets()->ServerThrow(0,3);Human->GetGadgets()->ServerThrow(0,2);Probe->SmokeAt=Now;
  }
  if(!Host&&Probe->SentSmoke&&Now-Probe->SmokeAt>.2)PC->SetControlRotation(FRotator::ZeroRotator);
  if(Probe->SmokeCount==1&&Probe->Inventory==0&&Probe->Credits==250)
  {
   Probe->SmokeReplicated=true;Probe->ConsumedOnce=Probe->SensorReplicated;
   if(Host&&Probe->SmokeAt<0.)Probe->SmokeAt=Now;
  }
  if(Host&&Probe->SmokeReplicated&&Probe->Marks&&Probe->MarkPrivacy&&Probe->Stand&&!Probe->FlashSent&&Now-Probe->SmokeAt>=1.2)
  {
   // Smoke's replication/consumption is already observed by both peers. Remove
   // this separate fixture before isolating the Flash owner-privacy assertion.
   for(TActorIterator<AVanthexGadgetActor> It(W);It;++It)if(It->GadgetId==TEXT("Smoke"))It->Destroy();
   const FTransform Transform(FRotator::ZeroRotator,NetworkGadgetFloor+FVector(0,0,160));
   auto* Flash=W->SpawnActorDeferred<AVanthexGadgetActor>(AVanthexGadgetActor::StaticClass(),Transform,HostPawn,HostPawn,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   Probe->FlashSent=true;Probe->FlashAt=Now;
   if(Flash){Flash->Configure(TEXT("Flash"),HostPawn);Flash->FinishSpawning(Transform);Flash->Detonate();}
   Probe->Flash=Human->GetGadgets()->GetFlashOpacity()>.2f;
   Probe->FlashPrivacy=HostPawn->GetGadgets()->GetFlashOpacity()>.2f;
   UE_LOG(LogVanthex,Display,TEXT("GADGET_NETWORK real flash remote=%.3f host=%.3f"),Human->GetGadgets()->GetFlashOpacity(),HostPawn->GetGadgets()->GetFlashOpacity());
  }
  Probe->FlashPower=Human->GetGadgets()->GetFlashOpacity();
  if(!Host&&Probe->FlashPower>.2f)
  {
   Probe->Flash=true;Probe->FlashPrivacy=HostPawn->GetGadgets()->GetFlashOpacity()==0.f;
   if(Probe->FlashAt<0.)Probe->FlashAt=Now;
  }
  if(!Host&&Probe->Flash&&Now-Probe->FlashAt>=2.)Probe->Finish(W,false,false);
 }),.05f,true);
#endif
}
