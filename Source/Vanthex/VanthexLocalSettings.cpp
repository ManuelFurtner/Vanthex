#include "VanthexLocalSettings.h"
#include "AudioDevice.h"
#include "Engine/World.h"
#include "GameFramework/InputSettings.h"

namespace
{
bool IsReservedGameplayKey(const FKey& Key)
{
 // Session, scoreboard, communication, team ping, and test controls are fixed bindings.
 return Key == EKeys::Escape || Key == EKeys::Tab || Key == EKeys::B ||
  Key == EKeys::Y || Key == EKeys::T || Key == EKeys::V || Key == EKeys::F5 ||
  Key == EKeys::MiddleMouseButton ||
  GetDefault<UInputSettings>()->ConsoleKeys.Contains(Key);
}

bool IsAllowedGameplayKey(const FKey& Key)
{
 return Key.IsValid() && Key != EKeys::AnyKey && Key.IsBindableToActions() &&
  !Key.IsAnalog() && !Key.IsGamepadKey() && !Key.IsTouch() && !IsReservedGameplayKey(Key);
}
}

UVanthexLocalSettings* UVanthexLocalSettings::Get()
{
 auto* Settings = GetMutableDefault<UVanthexLocalSettings>();
 if (!Settings->bGameplayKeysValidated)
 {
  Settings->ValidateGameplayKeys();
  Settings->bGameplayKeysValidated = true;
 }
 return Settings;
}

float UVanthexLocalSettings::SafeVolume(float Value)
{
	return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 1.f) : 0.f;
}

void UVanthexLocalSettings::ApplyAudio(UWorld* World)
{
	MasterVolume = SafeVolume(MasterVolume); CombatVolume = SafeVolume(CombatVolume);
	MovementVolume = SafeVolume(MovementVolume);
	UIVolume = SafeVolume(UIVolume); RoomVolume = SafeVolume(RoomVolume);
 VoiceVolume = SafeVolume(VoiceVolume);
	if (World) if (auto Device = World->GetAudioDevice(); Device.IsValid())
		Device->SetTransientPrimaryVolume(MasterVolume);
}

float UVanthexLocalSettings::GetMouseSensitivity(float ADSBlend) const
{
 const float Hip = FMath::IsFinite(MouseSensitivity) ? FMath::Clamp(MouseSensitivity, .1f, 5.f) : 1.f;
 const float Blend = FMath::IsFinite(ADSBlend) ? FMath::Clamp(ADSBlend, 0.f, 1.f) : 0.f;
 return Hip * FMath::Lerp(1.f, GetADSMouseSensitivity(), Blend);
}

float UVanthexLocalSettings::GetADSMouseSensitivity() const
{
 return FMath::IsFinite(ADSMouseSensitivity) ? FMath::Clamp(ADSMouseSensitivity, .1f, 3.f) : 1.f;
}

const TArray<FVanthexGameplayBinding>& UVanthexLocalSettings::GameplayBindings()
{
 using Action = EVanthexGameplayAction;
 static const TArray<FVanthexGameplayBinding> Bindings = {
  {Action::Forward, TEXT("Forward"), NSLOCTEXT("VanthexControls", "Forward", "Vorw\u00e4rts"), EKeys::W},
  {Action::Back, TEXT("Back"), NSLOCTEXT("VanthexControls", "Back", "R\u00fcckw\u00e4rts"), EKeys::S},
  {Action::Left, TEXT("Left"), NSLOCTEXT("VanthexControls", "Left", "Nach links"), EKeys::A},
  {Action::Right, TEXT("Right"), NSLOCTEXT("VanthexControls", "Right", "Nach rechts"), EKeys::D},
  {Action::Jump, TEXT("Jump"), NSLOCTEXT("VanthexControls", "Jump", "Springen"), EKeys::SpaceBar},
  {Action::Sprint, TEXT("Sprint"), NSLOCTEXT("VanthexControls", "Sprint", "Sprinten"), EKeys::LeftShift},
  {Action::Crouch, TEXT("Crouch"), NSLOCTEXT("VanthexControls", "Crouch", "Ducken"), EKeys::LeftControl},
  {Action::Fire, TEXT("Fire"), NSLOCTEXT("VanthexControls", "Fire", "Feuern / Angriff"), EKeys::LeftMouseButton},
  {Action::Aim, TEXT("Aim"), NSLOCTEXT("VanthexControls", "Aim", "Zielen"), EKeys::RightMouseButton},
  {Action::Reload, TEXT("Reload"), NSLOCTEXT("VanthexControls", "Reload", "Nachladen"), EKeys::R},
  {Action::Pickup, TEXT("Pickup"), NSLOCTEXT("VanthexControls", "Pickup", "Core / Waffe aufnehmen"), EKeys::E},
  {Action::Drop, TEXT("Drop"), NSLOCTEXT("VanthexControls", "Drop", "Core ablegen"), EKeys::G},
  {Action::Throw, TEXT("Throw"), NSLOCTEXT("VanthexControls", "Throw", "Core werfen"), EKeys::Q},
  {Action::Interact, TEXT("Interact"), NSLOCTEXT("VanthexControls", "Interact", "Interagieren"), EKeys::F},
  {Action::Operator, TEXT("Operator"), NSLOCTEXT("VanthexControls", "Operator", "Operator-F\u00e4higkeit"), EKeys::C},
  {Action::Primary, TEXT("Primary"), NSLOCTEXT("VanthexControls", "Primary", "Prim\u00e4rwaffe"), EKeys::One},
  {Action::Sidearm, TEXT("Sidearm"), NSLOCTEXT("VanthexControls", "Sidearm", "Pistole"), EKeys::Two},
  {Action::Knife, TEXT("Knife"), NSLOCTEXT("VanthexControls", "Knife", "Messer"), EKeys::Three},
  {Action::NextWeapon, TEXT("NextWeapon"), NSLOCTEXT("VanthexControls", "NextWeapon", "N\u00e4chste Waffe"), EKeys::MouseScrollDown},
  {Action::PreviousWeapon, TEXT("PreviousWeapon"), NSLOCTEXT("VanthexControls", "PreviousWeapon", "Vorherige Waffe"), EKeys::MouseScrollUp},
  {Action::GadgetOne, TEXT("GadgetOne"), NSLOCTEXT("VanthexControls", "GadgetOne", "Gadget 1"), EKeys::Four},
  {Action::GadgetTwo, TEXT("GadgetTwo"), NSLOCTEXT("VanthexControls", "GadgetTwo", "Gadget 2"), EKeys::Five},
  {Action::DropWeapon, TEXT("DropWeapon"), NSLOCTEXT("VanthexControls", "DropWeapon", "Waffe ablegen"), EKeys::X},
 };
 return Bindings;
}

FKey UVanthexLocalSettings::GetGameplayKey(EVanthexGameplayAction Action) const
{
 const auto& Bindings = GameplayBindings();
 const int32 Index = static_cast<int32>(Action);
 if (!Bindings.IsValidIndex(Index)) return EKeys::Invalid;
 const auto& Binding = Bindings[Index];
 if (const FName* Override = GameplayKeyOverrides.Find(Binding.Id)) return FKey(*Override);
 return Binding.DefaultKey;
}

void UVanthexLocalSettings::ValidateGameplayKeys()
{
 TSet<FKey> Used;
 for (const auto& Binding : GameplayBindings())
 {
  if (const FName* Override = GameplayKeyOverrides.Find(Binding.Id); Override && !IsAllowedGameplayKey(FKey(*Override)))
   GameplayKeyOverrides.Remove(Binding.Id);
  FKey Key = GetGameplayKey(Binding.Action);
  if (Binding.Action == EVanthexGameplayAction::DropWeapon && Used.Contains(Key))
  {
   // Older settings could already assign X. Preserve them and move only the new action.
   Key = EKeys::Invalid;
   for (const FKey& Candidate : {EKeys::Z, EKeys::H, EKeys::J, EKeys::K, EKeys::L, EKeys::U, EKeys::I,
        EKeys::O, EKeys::P, EKeys::M, EKeys::N, EKeys::Zero, EKeys::Six, EKeys::Seven, EKeys::Eight,
        EKeys::Nine, EKeys::Insert, EKeys::Home, EKeys::End, EKeys::PageUp, EKeys::PageDown,
        EKeys::F6, EKeys::F7, EKeys::F8, EKeys::F9, EKeys::F10, EKeys::F11, EKeys::F12})
   {
    if (IsAllowedGameplayKey(Candidate) && !Used.Contains(Candidate))
    {
     Key = Candidate;
     break;
    }
   }
   GameplayKeyOverrides.Add(Binding.Id, Key.GetFName());
  }
  if (Used.Contains(Key))
  {
   GameplayKeyOverrides.Reset();
   return;
  }
  Used.Add(Key);
 }
 if (GetGameplayKey(EVanthexGameplayAction::Crouch) == EKeys::LeftControl && Used.Contains(EKeys::RightControl))
  GameplayKeyOverrides.Reset();
}

bool UVanthexLocalSettings::TrySetGameplayKey(EVanthexGameplayAction Action, FKey Key, FText& Error)
{
 Error = FText::GetEmpty();
 const auto& Bindings = GameplayBindings();
 const int32 Index = static_cast<int32>(Action);
 if (!Bindings.IsValidIndex(Index) || !IsAllowedGameplayKey(Key))
 {
  Error = NSLOCTEXT("VanthexControls", "ReservedKey", "Diese Taste ist reserviert oder wird nicht unterst\u00fctzt.");
  return false;
 }
 for (const auto& Binding : Bindings)
 {
  if (Binding.Action == Action) continue;
  const FKey OtherKey = GetGameplayKey(Binding.Action);
  const bool bCrouchAlias = Binding.Action == EVanthexGameplayAction::Crouch &&
   OtherKey == EKeys::LeftControl && Key == EKeys::RightControl;
  const bool bRestoreAlias = Action == EVanthexGameplayAction::Crouch &&
   Key == EKeys::LeftControl && OtherKey == EKeys::RightControl;
  if (OtherKey == Key || bCrouchAlias || bRestoreAlias)
  {
   Error = FText::Format(NSLOCTEXT("VanthexControls", "KeyConflict", "{0} ist bereits f\u00fcr {1} belegt."),
    (bRestoreAlias ? EKeys::RightControl : Key).GetDisplayName(), Binding.Label);
   return false;
  }
 }
 const auto& Binding = Bindings[Index];
 if (Key == Binding.DefaultKey) GameplayKeyOverrides.Remove(Binding.Id);
 else GameplayKeyOverrides.Add(Binding.Id, Key.GetFName());
 return true;
}

void UVanthexLocalSettings::ResetGameplayKeys()
{
 GameplayKeyOverrides.Reset();
}

void UVanthexLocalSettings::Persist() { SaveConfig(); }

FVanthexCrosshairStyle UVanthexLocalSettings::GetCrosshairStyle() const
{
 return FVanthexCrosshairStyle::Make(CrosshairThickness,CrosshairLength,CrosshairGap,CrosshairColor);
}
void UVanthexLocalSettings::ResetCrosshair()
{
 CrosshairThickness=1.5f; CrosshairLength=8.f; CrosshairGap=0.f; CrosshairColor=FLinearColor::White;
 Persist();
}

bool UVanthexLocalSettings::ValidateAddress(const FString& Input, FString& Address, FString& Error)
{
	Address.Reset(); Error.Reset();
	const FString Candidate = Input.TrimStartAndEnd();
	if (Candidate.IsEmpty() || Candidate.Len() > 253)
	{ Error = TEXT("Bitte Hostname oder IPv4-Adresse eingeben."); return false; }
	FString Host = Candidate, Port;
	if (Candidate.Contains(TEXT(":")))
	{
		Candidate.Split(TEXT(":"), &Host, &Port);
		if (Port.IsEmpty() || Port.Len() > 5)
		{ Error = TEXT("Port muss zwischen 1 und 65535 liegen."); return false; }
		for (TCHAR C : Port) if (C < '0' || C > '9')
		{ Error = TEXT("Port muss eine Zahl sein."); return false; }
		const int32 Number = FCString::Atoi(*Port);
		if (Number < 1 || Number > 65535)
		{ Error = TEXT("Port muss zwischen 1 und 65535 liegen."); return false; }
	}
	if (Host.IsEmpty() || Host.StartsWith(TEXT(".")) || Host.EndsWith(TEXT(".")) || Host.Contains(TEXT("..")))
	{ Error = TEXT("Ungueltiger Hostname."); return false; }
	for (TCHAR C : Host) if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '.' || C == '-'))
	{ Error = TEXT("Hostname oder IPv4 verwenden, ohne URL, Leerzeichen oder Optionen."); return false; }
	TArray<FString> Labels; Host.ParseIntoArray(Labels,TEXT("."),false);
	bool Numeric=true;
	for(TCHAR C:Host) if(C!='.' && (C<'0' || C>'9')) Numeric=false;
	for(const FString& Label:Labels)
		if(Label.IsEmpty() || Label.Len()>63 || Label.StartsWith(TEXT("-")) || Label.EndsWith(TEXT("-"))
			|| (Numeric && (Label.Len()>3 || FCString::Atoi(*Label)>255)))
		{ Error=TEXT("Ungueltiger Hostname oder IPv4-Adresse."); return false; }
	if(Numeric && Labels.Num()!=4) { Error=TEXT("IPv4-Adresse muss vier Zahlen enthalten."); return false; }
	Address = Candidate;
	return true;
}
