#include "VanthexPlayerController.h"

#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexGameMode.h"
#include "VanthexMapLayout.h"
#include "Engine/World.h"

AVanthexPlayerController::AVanthexPlayerController()
{
	BulletAudio = CreateDefaultSubobject<UVanthexBulletAudioComponent>(TEXT("BulletAudio"));
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void AVanthexPlayerController::BeginPlay()
{
	Super::BeginPlay();
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


#include "Kismet/GameplayStatics.h"
void AVanthexPlayerController::HostGame() { if (IsLocalController()) UGameplayStatics::OpenLevel(this, FName(VanthexMap::IsGridV3(GetWorld())?TEXT("/Game/Vanthex/Maps/GridV3"):TEXT("/Game/Vanthex/Maps/Foundry")), true, "listen"); }
void AVanthexPlayerController::JoinGame(const FString& Address) { if (IsLocalController() && !Address.IsEmpty() && !Address.Contains(TEXT("?"))) ClientTravel(Address, TRAVEL_Absolute); }
void AVanthexPlayerController::LeaveGame() { if (IsLocalController()) UGameplayStatics::OpenLevel(this, FName(VanthexMap::IsGridV3(GetWorld())?TEXT("/Game/Vanthex/Maps/GridV3"):TEXT("/Game/Vanthex/Maps/Foundry"))); }


#include "Engine/GameViewportClient.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
void AVanthexPlayerController::SetupInputComponent() { Super::SetupInputComponent(); InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&AVanthexPlayerController::ToggleSessionMenu); }
void AVanthexPlayerController::CloseSessionMenu() {
 if(SessionMenu.IsValid() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(SessionMenu.ToSharedRef());
 SessionMenu.Reset(); AddressBox.Reset(); bShowMouseCursor=false; SetInputMode(FInputModeGameOnly()); SetIgnoreMoveInput(false); SetIgnoreLookInput(false);
}
void AVanthexPlayerController::EndPlay(const EEndPlayReason::Type Reason) { if(IsLocalController()) CloseSessionMenu(); Super::EndPlay(Reason); }
void AVanthexPlayerController::ToggleSessionMenu() {
 if(!IsLocalController() || !GetWorld()->GetGameViewport()) return;
 if(SessionMenu.IsValid()) { CloseSessionMenu(); return; }
 if (auto* OwnPawn=Cast<AVanthexCharacter>(GetPawn())) OwnPawn->ServerAction(1,false);
 static const FTextBlockStyle MenuText = FTextBlockStyle().SetFont(FCoreStyle::GetDefaultFontStyle("Regular",22)).SetColorAndOpacity(FLinearColor::White);
 SessionMenu=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.012f,0.022f,0.035f,1.f)).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(760)[
 SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(STextBlock).TextStyle(&MenuText).Text(FText::FromString(TEXT("VANTHEX - Sitzung")))]
 +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SButton).TextStyle(&MenuText).ContentPadding(FMargin(16)).Text(FText::FromString(TEXT("Weiterspielen"))).OnClicked_Lambda([this](){CloseSessionMenu();return FReply::Handled();})]
 +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SButton).TextStyle(&MenuText).ContentPadding(FMargin(16)).Text(FText::FromString(TEXT("Spiel hosten (LAN / direkte Verbindung)"))).OnClicked_Lambda([this](){CloseSessionMenu();HostGame();return FReply::Handled();})]
 +SVerticalBox::Slot().AutoHeight().Padding(8)[SAssignNew(AddressBox,SEditableTextBox).Font(FCoreStyle::GetDefaultFontStyle("Regular",22)).HintText(FText::FromString(TEXT("Host-IP, z. B. 192.168.1.10:7777")))]
 +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SButton).TextStyle(&MenuText).ContentPadding(FMargin(16)).Text(FText::FromString(TEXT("Beitreten"))).OnClicked_Lambda([this](){const FString Address=AddressBox->GetText().ToString().TrimStartAndEnd();if(!Address.IsEmpty()){CloseSessionMenu();JoinGame(Address);}return FReply::Handled();})]
 +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SButton).TextStyle(&MenuText).ContentPadding(FMargin(16)).Text(FText::FromString(TEXT("Verlassen / lokales Spiel"))).OnClicked_Lambda([this](){CloseSessionMenu();LeaveGame();return FReply::Handled();})]
 +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(STextBlock).TextStyle(&MenuText).Text(FText::FromString(VanthexMap::IsGridV3(GetWorld())?TEXT("E: Core aufnehmen | G: Ablegen | Q: Werfen\nOnline laeuft die Runde bei offenem Menue weiter."):TEXT("F: Shift bedienen | C: ARC-Fernsteuerung\nOnline laeuft die Runde bei offenem Menue weiter.")))]
 ]];
 GetWorld()->GetGameViewport()->AddViewportWidgetContent(SessionMenu.ToSharedRef(),100);
 bShowMouseCursor=true; SetIgnoreMoveInput(true); SetIgnoreLookInput(true); FInputModeGameAndUI Mode; Mode.SetWidgetToFocus(SessionMenu); SetInputMode(Mode);
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
 if(FParse::Param(FCommandLine::Get(),TEXT("VanthexNetworkProbe")) && HasAuthority() && !IsLocalController() && !bProbeDamaged) {
  if(auto* RemoteCharacter=Cast<AVanthexCharacter>(GetPawn())) { FVanthexDamageRequest Damage; Damage.Damage=40; RemoteCharacter->GetHealthComponent()->ApplyDamage(Damage); bProbeDamaged=true; }
 }

 if (IsLocalController()) {
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
 int32 Characters=0,Cores=0,Devices=0; for(TActorIterator<AVanthexCharacter> It(GetWorld());It;++It) ++Characters;
 for(TActorIterator<AVanthexCore> It(GetWorld());It;++It) ++Cores;
 int32 Offline=0; bool PowerCorrect=true;
 for(TActorIterator<AVanthexShiftDevice> It(GetWorld());It;++It) { ++Devices; Offline+=!It->bPowered; PowerCorrect &= It->bPowered==(It->Circuit!=1); }
 const auto* PS=GetPlayerState<AVanthexPlayerState>(); const auto* GS=GetWorld()->GetGameState<AVanthexGameState>();
 const float Moved=FVector::Dist2D(ProbeOrigin,ProbeCharacter->GetActorLocation());
 const bool bGridV3=VanthexMap::IsGridV3(GetWorld());
 bool Pass=Characters==6 && Cores==1 && Devices==(bGridV3?0:15) && Offline==(bGridV3?0:3) && PowerCorrect && PS && PS->GetTeam()!=EVanthexTeam::None && GS && GS->GetRoundIndex()>0 && Moved>100 && ProbeCharacter->GetWeapon()->GetAmmoInMagazine()<30 && (HasAuthority() || FMath::IsNearlyEqual(ProbeCharacter->GetHealthComponent()->GetHealth(),60.f));
 const FString Report=FString::Printf(TEXT("%s netmode=%d characters=%d cores=%d devices=%d offline=%d team=%d moved=%.1f health=%.0f ammo=%d"),Pass?TEXT("PASS"):TEXT("FAIL"),int32(GetNetMode()),Characters,Cores,Devices,Offline,PS?int32(PS->GetTeam()):-1,Moved,ProbeCharacter->GetHealthComponent()->GetHealth(),ProbeCharacter->GetWeapon()->GetAmmoInMagazine());
 FString Name=HasAuthority()?TEXT("NetworkHost.txt"):TEXT("NetworkClient.txt");
 if(bGridV3) Name=TEXT("GridV3")+Name;
 FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/Name)); bProbeReported=true;
}

void AVanthexPlayerController::ClientConfirmHit_Implementation(bool bHeadshot) { HitMarkerUntil=GetWorld()->GetTimeSeconds()+0.2f; bHitMarkerHead=bHeadshot; }

void AVanthexPlayerController::ClientBulletAudio_Implementation(const FVanthexBulletAudioPacket& Packet)
{
 if (BulletAudio) BulletAudio->Receive(Packet);
}

