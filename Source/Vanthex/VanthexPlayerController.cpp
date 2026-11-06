#include "VanthexPlayerController.h"
#include "SVanthexSessionMenu.h"
#include "VanthexLocalSettings.h"
#include "VanthexCommunicationComponent.h"

#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexGameMode.h"
#include "VanthexOperatorComponent.h"
#include "VanthexMapLayout.h"
#include "VanthexBuyMenuComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

namespace {
// A Tab press can originate in Slate while its release occurs after the menu
// closes. Observe that release before widget/viewport routing can consume it.
class FScoreboardKeyRelease final : public IInputProcessor
{
public:
 explicit FScoreboardKeyRelease(AVanthexPlayerController* PC):Owner(PC) {}
 virtual void Tick(float,FSlateApplication&,TSharedRef<ICursor>) override {}
 virtual bool HandleKeyUpEvent(FSlateApplication&,const FKeyEvent& Event) override {
  if(Event.GetKey()==EKeys::Tab) if(auto* PC=Owner.Get();PC&&PC->IsScoreboardOpen()) {
   PC->HideScoreboard();return true;
  }
  return false;
 }
 TWeakObjectPtr<AVanthexPlayerController> Owner;
};
}

AVanthexPlayerController::AVanthexPlayerController()
{
 Communication = CreateDefaultSubobject<UVanthexCommunicationComponent>(TEXT("Communication"));
	BulletAudio = CreateDefaultSubobject<UVanthexBulletAudioComponent>(TEXT("BulletAudio"));
	WeaponAudio = CreateDefaultSubobject<UVanthexWeaponAudioComponent>(TEXT("WeaponAudio"));
	BuyMenu = CreateDefaultSubobject<UVanthexBuyMenuComponent>(TEXT("BuyMenu"));
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void AVanthexPlayerController::BeginPlay()
{
	Super::BeginPlay();
 if(IsLocalController()) UVanthexLocalSettings::Get()->ApplyAudio(GetWorld());
 if(IsFrontEnd())
 {
  OpenSessionMenu();
  return;
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("LooseCoreNetworkTest"))) {
  extern void VanthexStartLooseCoreNetworkTest(AVanthexPlayerController* PC);VanthexStartLooseCoreNetworkTest(this);
 }
 if(IsLocalController()&&FSlateApplication::IsInitialized()) {
  ScoreboardKeyRelease=MakeShared<FScoreboardKeyRelease>(this);
  FSlateApplication::Get().RegisterInputPreProcessor(ScoreboardKeyRelease);
 }
	if(FParse::Param(FCommandLine::Get(),TEXT("CombatCollisionNetworkTest"))) {
		extern void VanthexStartCombatCollisionNetworkTest(AVanthexPlayerController* PC);
		VanthexStartCombatCollisionNetworkTest(this);
	}
	if(FParse::Param(FCommandLine::Get(),TEXT("WeaponAudioPlaybackTest"))) {
		extern void VanthexStartWeaponAudioPlaybackTest(AVanthexPlayerController* PC);
		VanthexStartWeaponAudioPlaybackTest(this);
	}
	if(FParse::Param(FCommandLine::Get(),TEXT("EconomyNetworkTest"))) {
		extern void VanthexStartEconomyNetworkTest(AVanthexPlayerController* PC);
		VanthexStartEconomyNetworkTest(this);
	}
 if(FParse::Param(FCommandLine::Get(),TEXT("GadgetNetworkTest"))) {
  extern void VanthexStartGadgetNetworkTest(AVanthexPlayerController* PC);VanthexStartGadgetNetworkTest(this);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("AimNetworkTest"))) {
  extern void VanthexStartAimNetworkTest(AVanthexPlayerController* PC);VanthexStartAimNetworkTest(this);
 }
	if(FParse::Param(FCommandLine::Get(),TEXT("EquipmentNetworkTest"))) {
		extern void VanthexStartEquipmentNetworkTest(AVanthexPlayerController* PC);
		VanthexStartEquipmentNetworkTest(this);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("BulletAudioNetworkTest")))
	{
		extern void VanthexStartBulletAudioNetworkTest(AVanthexPlayerController* PC);
		VanthexStartBulletAudioNetworkTest(this);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("BulletAudioPlaybackTest")))
	{
		extern void VanthexStartBulletAudioPlaybackTest(AVanthexPlayerController* PC);
		VanthexStartBulletAudioPlaybackTest(this);
	}

	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -89.f;
		PlayerCameraManager->ViewPitchMax = 89.f;
	}
}

void AVanthexPlayerController::RestartTest()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (AVanthexGameMode* Mode = Cast<AVanthexGameMode>(World->GetAuthGameMode()))
	{
		Mode->RestartTest();
	}
	else
	{
		UE_LOG(LogVanthex, Warning, TEXT("RestartTest found no VanthexGameMode"));
	}
}

void AVanthexPlayerController::PawnLeavingGame()
{
 // UE invokes this before AController::Destroyed calls GameMode::Logout.
 // Transfer first so the base implementation finds no roster pawn to destroy.
 if(UWorld* World=GetWorld())
  if(auto* Mode=World->GetAuthGameMode<AVanthexGameMode>()) Mode->HandOverPlayerSlot(this);
 Super::PawnLeavingGame();
}


bool AVanthexPlayerController::IsFrontEnd() const
{
 return VanthexMap::IsFrontEnd(GetWorld());
}

void AVanthexPlayerController::OpenSessionMap(const TCHAR* MapPath, bool bListen)
{
 UWorld* World = GetWorld();
 if(!IsLocalController() || !World) return;

 UVanthexLocalSettings::Get()->Persist();
 // A direct join may still be connecting when the player starts a local session.
 if(GEngine) GEngine->CancelPending(World);
 UGameplayStatics::OpenLevel(this, FName(MapPath), true, bListen ? TEXT("listen") : TEXT(""));
}

void AVanthexPlayerController::StartGame(bool bCaptureTheFlag, bool bListen)
{
 OpenSessionMap(bCaptureTheFlag ? VanthexMap::CTFPath : VanthexMap::GridV4Path, bListen);
}

void AVanthexPlayerController::HostGame()
{
 if(IsFrontEnd())
 {
  StartGame(false, true);
  return;
 }
 OpenSessionMap(VanthexMap::CurrentMap(GetWorld()), true);
}

void AVanthexPlayerController::JoinGame(const FString& Address)
{
 FString Validated, Error;
 if(!IsLocalController() || !UVanthexLocalSettings::ValidateAddress(Address, Validated, Error)) return;

 auto* Settings = UVanthexLocalSettings::Get();
 Settings->LastAddress = Validated;
 Settings->Persist();
 // Keep the front end available if the connection fails before loading a map.
 CloseSessionMenu();
 ClientTravel(Validated, TRAVEL_Absolute);
}

void AVanthexPlayerController::LeaveGame()
{
 OpenSessionMap(VanthexMap::FrontEndPath, false);
}


void AVanthexPlayerController::SetupInputComponent() {
 Super::SetupInputComponent();
 InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&AVanthexPlayerController::ToggleSessionMenu);
 if(IsFrontEnd()) return;
 InputComponent->BindKey(EKeys::Y,IE_Pressed,Communication.Get(),&UVanthexCommunicationComponent::OpenTeamChat);
 InputComponent->BindKey(EKeys::T,IE_Pressed,Communication.Get(),&UVanthexCommunicationComponent::OpenAllChat);
 InputComponent->BindKey(EKeys::B,IE_Pressed,BuyMenu.Get(),&UVanthexBuyMenuComponent::Toggle);
 InputComponent->BindKey(EKeys::Tab,IE_Pressed,this,&AVanthexPlayerController::ShowScoreboard);
 InputComponent->BindKey(EKeys::Tab,IE_Released,this,&AVanthexPlayerController::HideScoreboard);
}
void AVanthexPlayerController::CloseSessionMenu()
{
 if(!IsFrontEnd()) RemoveSessionMenu();
}

void AVanthexPlayerController::RemoveSessionMenu()
{
 if(!SessionMenu.IsValid()) return;

 UWorld* World = GetWorld();
 if(auto* Viewport = World ? World->GetGameViewport() : nullptr)
 {
  Viewport->RemoveViewportWidgetContent(SessionMenu.ToSharedRef());
  SetInputMode(FInputModeGameOnly());
 }
 SessionMenu.Reset();
 UVanthexLocalSettings::Get()->Persist();
 bShowMouseCursor = false;
 SetIgnoreMoveInput(false);
 SetIgnoreLookInput(false);
}
void AVanthexPlayerController::EndPlay(const EEndPlayReason::Type Reason) {
 if(ScoreboardKeyRelease&&FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(ScoreboardKeyRelease);
 ScoreboardKeyRelease.Reset();HideScoreboard();
 RemoveSessionMenu();
 Super::EndPlay(Reason);
}
void AVanthexPlayerController::ToggleSessionMenu()
{
 if(SessionMenu.IsValid())
 {
  CloseSessionMenu();
  return;
 }
 OpenSessionMenu();
}

void AVanthexPlayerController::OpenSessionMenu()
{
 if(SessionMenu.IsValid() || !IsLocalController() || !GetLocalPlayer() || !FSlateApplication::IsInitialized()) return;
 auto* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
 if(!Viewport) return;

 HideScoreboard();
 Communication->CloseChat();
 if(BuyMenu) BuyMenu->Close();
 if(auto* OwnPawn=Cast<AVanthexCharacter>(GetPawn())) {
  OwnPawn->ServerAction(1,false); OwnPawn->SetSprinting(false); OwnPawn->SetAiming(false);
 }
 SessionMenu=SNew(SVanthexSessionMenu).Controller(this).OnResume(FSimpleDelegate::CreateUObject(this,&AVanthexPlayerController::CloseSessionMenu));
 Viewport->AddViewportWidgetContent(SessionMenu.ToSharedRef(),100);
 bShowMouseCursor=true; SetIgnoreMoveInput(true); SetIgnoreLookInput(true);
 FInputModeUIOnly Mode; Mode.SetWidgetToFocus(SessionMenu); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
}


#include "VanthexCharacter.h"
#include "VanthexPlayerState.h"
#include "VanthexGameState.h"
#include "VanthexCore.h"
#include "VanthexShiftDevice.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
void AVanthexPlayerController::PlayerTick(float DeltaSeconds) {
 Super::PlayerTick(DeltaSeconds);
 if(IsFrontEnd())
 {
  // Retry if the viewport was unavailable at BeginPlay.
  OpenSessionMenu();
  return;
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("VanthexNetworkProbe")) && HasAuthority() && !IsLocalController() && !bProbeDamaged) {
  if(auto* RemoteCharacter=Cast<AVanthexCharacter>(GetPawn())) { FVanthexDamageRequest Damage; Damage.Damage=40; RemoteCharacter->GetHealthComponent()->ApplyDamage(Damage); bProbeDamaged=true; }
 }

 if (IsLocalController() && !FParse::Param(FCommandLine::Get(),TEXT("OperatorShots")) && !FParse::Param(FCommandLine::Get(),TEXT("WeaponModelShots")) && !FParse::Param(FCommandLine::Get(),TEXT("GadgetShots"))) {
  auto* OwnCharacter=Cast<AVanthexCharacter>(GetPawn());
  if (OwnCharacter && OwnCharacter->GetHealthComponent()->IsDead()) {
   if (APawn* Spectated = BulletAudio->GetListenerPawn()) { if(GetViewTarget()!=Spectated) SetViewTargetWithBlend(Spectated,0.3f); }
  } else if(OwnCharacter && GetViewTarget()!=OwnCharacter) SetViewTarget(OwnCharacter);
 }
 if(!FParse::Param(FCommandLine::Get(),TEXT("VanthexNetworkProbe")) || !IsLocalController() || bProbeReported) return;
 auto* ProbeCharacter=Cast<AVanthexCharacter>(GetPawn()); if(!ProbeCharacter || !ProbeCharacter->GetWeapon()) return;
 if(ProbeStart<0) { ProbeStart=GetWorld()->GetTimeSeconds(); ProbeOrigin=ProbeCharacter->GetActorLocation(); }
 const float Age=GetWorld()->GetTimeSeconds()-ProbeStart;
 if(HasAuthority() && Age>1 && Age<2) for(TActorIterator<AVanthexShiftDevice> It(GetWorld());It;++It) if(It->Kind==3 && It->Circuit==1) It->SetCircuitPower(false);
 if(Age>2 && Age<3) ProbeCharacter->DoMove(1,0);
 if (Age>4 && !bProbeFired) { ProbeCharacter->ServerAction(0,false); bProbeFired=true; }
 if (Age>4.3f && !bProbeStopped) { ProbeCharacter->ServerAction(1,false); bProbeStopped=true; }
 if(Age<10) return;
 int32 Characters=0,Cores=0,Devices=0,Operators=0; for(TActorIterator<AVanthexCharacter> It(GetWorld());It;++It) {
  ++Characters;
  if(const auto* Operator=It->FindComponentByClass<UVanthexOperatorComponent>(); Operator && Operator->IsPresentationReady() && Operator->GetOperatorId()==TEXT("ARC")) ++Operators;
 }
 for(TActorIterator<AVanthexCore> It(GetWorld());It;++It) ++Cores;
 int32 Offline=0; bool PowerCorrect=true;
 for(TActorIterator<AVanthexShiftDevice> It(GetWorld());It;++It) { ++Devices; Offline+=!It->bPowered; PowerCorrect &= It->bPowered==(It->Circuit!=1); }
 const auto* PS=GetPlayerState<AVanthexPlayerState>(); const auto* GS=GetWorld()->GetGameState<AVanthexGameState>();
 const float Moved=FVector::Dist2D(ProbeOrigin,ProbeCharacter->GetActorLocation());
 const bool bGridV3=VanthexMap::IsFacility(GetWorld());
 bool Pass=Characters==6 && Operators==6 && Cores==1 && Devices==(bGridV3?0:15) && Offline==(bGridV3?0:3) && PowerCorrect && PS && PS->GetTeam()!=EVanthexTeam::None && GS && GS->GetRoundIndex()>0 && Moved>100 && ProbeCharacter->GetWeapon()->GetAmmoInMagazine()<30 && (HasAuthority() || FMath::IsNearlyEqual(ProbeCharacter->GetHealthComponent()->GetHealth(),60.f));
 const FString Report=FString::Printf(TEXT("%s netmode=%d characters=%d operators=%d cores=%d devices=%d offline=%d team=%d moved=%.1f health=%.0f ammo=%d"),Pass?TEXT("PASS"):TEXT("FAIL"),int32(GetNetMode()),Characters,Operators,Cores,Devices,Offline,PS?int32(PS->GetTeam()):-1,Moved,ProbeCharacter->GetHealthComponent()->GetHealth(),ProbeCharacter->GetWeapon()->GetAmmoInMagazine());
 FString Name=HasAuthority()?TEXT("NetworkHost.txt"):TEXT("NetworkClient.txt");
 if(bGridV3) Name=FString(VanthexMap::IsGridV4(GetWorld())?TEXT("GridV4"):TEXT("GridV3"))+Name;
 FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/Name)); bProbeReported=true;
}

void AVanthexPlayerController::ClientConfirmHit_Implementation(bool bHeadshot) { HitMarkerUntil=GetWorld()->GetTimeSeconds()+0.2f; bHitMarkerHead=bHeadshot; }

void AVanthexPlayerController::ClientBulletAudio_Implementation(const FVanthexBulletAudioPacket& Packet)
{
 if (BulletAudio) BulletAudio->Receive(Packet);
}

void AVanthexPlayerController::ClientWeaponAudio_Implementation(const FVanthexWeaponAudioPacket& Packet)
{
 if (WeaponAudio) WeaponAudio->Receive(Packet);
}

