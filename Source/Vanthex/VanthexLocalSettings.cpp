#include "VanthexLocalSettings.h"
#include "AudioDevice.h"
#include "Engine/World.h"

UVanthexLocalSettings* UVanthexLocalSettings::Get() { return GetMutableDefault<UVanthexLocalSettings>(); }

float UVanthexLocalSettings::SafeVolume(float Value)
{
	return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 1.f) : 0.f;
}

void UVanthexLocalSettings::ApplyAudio(UWorld* World)
{
	MasterVolume = SafeVolume(MasterVolume); CombatVolume = SafeVolume(CombatVolume);
	UIVolume = SafeVolume(UIVolume); RoomVolume = SafeVolume(RoomVolume);
	if (World) if (auto Device = World->GetAudioDevice(); Device.IsValid())
		Device->SetTransientPrimaryVolume(MasterVolume);
}

void UVanthexLocalSettings::Persist() { SaveConfig(); }

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
