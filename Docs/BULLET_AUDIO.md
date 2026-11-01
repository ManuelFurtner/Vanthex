# VANTHEX — Bullet Audio M1

Stand: 1. November 2026. Das Modul ergänzt bestätigte Hitscan-Schüsse um räumliche Vorbeifluggeräusche, optionale Überschallknalle und nahe Materialeinschläge. Es arbeitet rein kosmetisch. Schaden, Feuerrate, Rückstoß, Bewegung, Core, Extraktion und Teamregeln werden dadurch nicht geändert.

## Gefundene Architektur und Integration

| Vorhandene Klasse | Tatsächliche Zuständigkeit und Anbindung |
| --- | --- |
| `AWeaponBase` | Magazin, Nachladen und Waffenbasis; unverändert. |
| `AHitscanWeapon` | `FireShot()` prüft Autorität, Runde, Lebenszustand, Nachladen, Feuerrate und Munition. Der bestehende `ECC_Visibility`-Trace bestimmt Treffer und Schaden. Direkt danach erhält das Audiomodul dessen Start, Ende und `FHitResult`. `bReturnPhysicalMaterial` liefert zusätzlich die Oberfläche. |
| `AVanthexPlayerController` | Besitzt jeweils einen lokalen Audiorouter und die neue unzuverlässige Client-RPC. Die bestehende Trefferbestätigung bleibt getrennt. |
| `AVanthexCharacter`, `UHealthComponent`, `AVanthexPlayerState` | Liefern Kamera, Lebenszustand und Team für die Hörposition. Sie erhalten keine neuen Audio- oder Schadensregeln. |
| `AVanthexBotController` | Bestehender Schussablauf bleibt erhalten. Nur ausdrücklich gestartete Audiotests halten die Bots für reproduzierbare Tests an. |

Vorher gab es kein Bullet-Audio-System und keine entsprechenden Gameplay Tags. Der bestehende Schuss-Multicast löst visuelle Effekte aus. Die vorhandenen Factory-Audio-Assets enthalten Umgebungsgeräusche; deren Wiedergabe bleibt erhalten.

```text
gültiges FireShot auf dem Server
  → bestehender Hitscan und tatsächlicher blockierender Treffer
  → UVanthexBulletAudioComponent::DispatchShot
  → Distanz, gültiges Segment, hörbare Wandseite und freie Verbindung
  → ClientBulletAudio auf dem zuständigen PlayerController [Unreliable]
  → Receive: Sequenz, Begrenzung, aktuelle lokale Hörposition
  → Gameplay-Tag-Ereignis und Unreal PlaySoundAtLocation
```

## Segment, Hörposition und Fairness

`EvaluateNearMiss` berechnet den kürzesten Abstand zur tatsächlichen Schussstrecke. Bei einem blockierenden Treffer endet diese am `ImpactPoint`. Projektionen vor dem Start oder am beziehungsweise hinter dem Ende erzeugen keinen Vorbeiflug. Ungültige und entartete Strecken werden verworfen. Ein Treffer auf den Listener wird nicht zusätzlich als Near-Miss gemeldet.

Erst für Kandidaten innerhalb des Radius prüft ein kurzer Sichtstrahl zwischen Hörposition und Schallquelle eine seitliche Wand. Nahe Einschläge benötigen eine Verbindung zur getroffenen Wandseite; eine Rückseite wird nicht durch die Wand hörbar. Der Client prüft den aktuellen Abstand und die Verbindung erneut, damit eine inzwischen veränderte Position berücksichtigt wird. Das verändert keinen Waffen-Trace.

Die Hörreferenz ist die aktive Kamera der Figur plus konfigurierbarem `ListenerOffset`. Dadurch stimmen die Positionen auch beim Ducken besser mit der Audio Engine überein. Tote Spieler sehen und hören denselben lebenden Teamkameraden mit dem niedrigsten replizierten Spawn-Slot. Diese gemeinsame, deterministische Auswahl ersetzt die bisherige Auswahl nach lokaler Actor-Reihenfolge; Teamzuordnung und Lebensregeln bleiben unverändert.

Der Schütze hört keinen eigenen Near-Miss, kann jedoch seine eigenen nahen Wand- und Metalleinschläge hören. Treffer auf Figuren erzeugen keine falschen Beton- oder Metalleinschläge.

## Ereignisse und Schnittstellen

`OnAudioEvent(Event, Position, Gain)` ist ein Blueprint-fähiges Delegate am Router. Positionen sind Weltkoordinaten; Gain ist der Distanzfaktor vor der Lautstärke des Sound-Sets.

| Gameplay Tag | Verhalten |
| --- | --- |
| `Bullet.NearMiss.Close` | Close-Sound-Set für 0–50 cm. |
| `Bullet.NearMiss.Medium` | Medium-Sound-Set für über 50 bis 150 cm. |
| `Bullet.NearMiss.Far` | Far-Sound-Set für über 150 bis unter 300 cm. |
| `Bullet.Flyby` | Gemeinsames Vorbeiflugereignis; Sound-Set dient als Ersatz bei einem nicht belegten Near-Miss-Set. Es wird kein zweites Flyby-Layer zusätzlich gespielt. |
| `Bullet.Crack` | Zusätzlich nur bei einer Waffe mit `bSupersonicBullet=true`. |
| `Bullet.Impact.Nearby` | Materialabhängiger Einschlag, standardmäßig innerhalb von 600 cm. |
| `Bullet.Ricochet` | Reservierter Tag, derzeit nicht ausgelöst. Das Gameplay simuliert keine Abpraller. |

`DispatchShot` wird ausschließlich vom bestätigten serverseitigen Schussablauf aufgerufen. `ClientBulletAudio(const FVanthexBulletAudioPacket&)` ist `Client, Unreliable`. Das Paket enthält Sequenz, relevante Schallpositionen, Band, Materialschlüssel und kosmetische Flags. Es überträgt weder den vollständigen Strahl noch einen zusätzlichen Standort des verdeckten Schützen. `Receive` verwirft doppelte oder überholte Sequenzen.

## Profil und mitgelieferte Sounds

Das Profil liegt unter `/Game/Vanthex/Audio/Bullet/DA_BulletAudio`. Die Referenz steht in `H:\Vanthex\Config\DefaultGame.ini` und ist über **Project Settings → Game → Vanthex Bullet Audio** änderbar. Gameplay-Klassen enthalten keine Audio-Asset-Pfade.

| Einstellung | Standard |
| --- | --- |
| `CloseDistance` / `MediumDistance` / `MaxFlybyDistance` | 50 / 150 / 300 cm |
| `MaxImpactDistance` | 600 cm |
| `ListenerOffset` | 0 / 0 / 0 cm, in Blickrichtung gedreht |
| `MinEventInterval` | 0,045 s pro Listener |
| `MaxPacketsPerSecond` | 16 pro Listener und Zeitfenster |
| Whiz / Flyby-Ersatz / Crack / Impact-Lautstärke | 0,45 / 0,30 / 0,18 / 0,32 |
| Pitch-Variation | 0,97–1,03; keine zufällige Grundlautstärke |
| `SC_BulletAudio` | Höchstens 8 Stimmen pro Besitzer; leiseste Stimme wird bei Bedarf ersetzt |
| `SA_BulletSpatial` | 3D-Spatialization, innerer Radius 300 cm, weitere 300 cm lineare Abschwächung |

Der Distanzfaktor ist kontinuierlich: `(1 − Abstand / Maximaldistanz)²`. Bei 300 cm ist die Vorbeifluglautstärke null und es wird kein Ereignis mehr gesendet. Alle drei Standard-Near-Miss-Sets verwenden dieselben Whiz-Varianten mit gleicher Grundlautstärke, sodass die Bandgrenzen keinen Lautstärkesprung verursachen. Frei gewählte Ersatzsets müssen ebenfalls aufeinander abgestimmt werden.

Die 16 mitgelieferten Sounds sind ursprüngliche, reproduzierbare Synthese-Prototypen: drei Whiz-, zwei Flyby-, zwei Crack- sowie je drei Concrete-, Steel- und MetalGrate-Varianten. Es sind Mono-WAVs mit 44,1 kHz und 16 Bit, etwa 55–270 ms lang. Die Dateien enthalten keine eingebrannte Stereorichtung. Unreal übernimmt die Ortung. Es handelt sich nicht um professionelle Geschossaufnahmen oder eine physikalische Simulation.

Die kurzen Sounds werden einmalig am relevanten Punkt abgespielt. Für M1 gibt es weder bewegte Audioquellen noch einen Audio-Tick oder zusätzliche Bullet-Actors. Engine-Concurrency begrenzt überlappende Stimmen; die Ereignisbegrenzung schützt zusätzlich das Netzwerk und den lokalen Mix.

Die echte Mixer-Prüfung zeigte einen Sonderfall in UE 5.8 `FAudioDevice::GetAzimuth`: Bei exakt seitlichen Quellen ist die Vorwärtsprojektion null, wodurch auch links als rechts wiedergegeben wird. Der Router verschiebt ausschließlich die Wiedergabeposition in diesem Fall um 0,1 cm nach vorne. Segment, Distanz, Netzwerkposition und veröffentlichte Ereignisposition bleiben exakt. Der unveränderte Test mit rein seitlichen Quellen besteht damit.

## Konfiguration im Unreal-Editor

1. Im Content Browser `Vanthex/Audio/Bullet/DA_BulletAudio` öffnen. Distanzen, Listener-Offset und Grenzen dort anpassen; das Profil wird beim Spielstart geladen.
2. Unter `Events` die SoundWave-Varianten der Tags belegen. Unter `Impacts` Materialschlüssel und deren Varianten bearbeiten. Mono-Dateien ohne vorgefertigte Stereobewegung eignen sich für diese Positionswiedergabe.
3. `Attenuation` auf `SA_BulletSpatial` und `Concurrency` auf `SC_BulletAudio` belassen oder passende eigene Assets zuweisen. Eine fehlende räumliche Konfiguration führt zum Auslassen des Sounds, nicht zu ungewollter 2D-Wiedergabe.
4. In Waffenklassen unter **Weapon → Bullet Audio** `Supersonic Bullet` einstellen. Die vorhandene Rifle verwendet `true`; eine Unterschallwaffe setzt `false`.
5. Oberflächen über Tags auf der tatsächlich getroffenen Kollisionskomponente beziehungsweise ihrem Actor zuweisen: `Concrete`, `Steel`/`Metal`, `MetalGrate`/`Grate`. Die priorisierte Reihenfolge prüft Gitter vor generischem Metall. Unbekannte Oberflächen verwenden `DefaultSurface=Concrete`.
6. Alternativ einem Physical Material einen Physical Surface Type zuweisen und dessen Nummer unter `PhysicalSurfaces` auf `Concrete`, `Steel`, `MetalGrate` oder einen neuen Schlüssel abbilden. Diese explizite Zuordnung hat Vorrang vor Tags. Für weitere Materialien reicht ein neuer `Impacts`-Eintrag mit entsprechender Zuordnung.
7. Änderungen speichern und die Spielsession neu starten. Der Bullet-Content-Ordner ist in `DirectoriesToAlwaysCook` eingetragen, damit das über Projekteinstellungen geladene Profil auch beim späteren Packaging enthalten ist.

Die Quell-WAVs, Generator und Messwerte liegen unter `H:\Vanthex\AudioSource\Bullet`. Der Import ist wiederholbar:

```powershell
py 'H:\Vanthex\AudioSource\Bullet\generate_bullet_audio.py'
& 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'H:\Vanthex\Vanthex.uproject' -run=VanthexBuildBulletAudio -nullrhi -unattended
```

Der Commandlet erstellt standardmäßig nur fehlende Assets. `-ReplacePrototypeAssets` ersetzt bewusst auch bestehende Prototyp-Sounds und Profilwerte und ist für normale Editor-Anpassungen nicht erforderlich.

## Multiplayer und Fehlertoleranz

Die Waffenentscheidung bleibt auf dem Server. Es gibt keine lokale Audio-Prediction, keine zusätzliche Multicast-RPC und keine Zuverlässigkeitsanforderung für kosmetische Ereignisse. Nur räumlich relevante PlayerController erhalten ein Paket. Bots können Schüsse erzeugen, benötigen aber keinen eigenen Audioclient. Listen-Server spielen ihren lokalen Sound über denselben Empfänger ab. Dedicated Server rechnen die Relevanz, spielen selbst jedoch keinen Sound ab.

Sendeseite und Empfänger begrenzen Ereignisse unabhängig. Paketverluste, Überlast oder verspätete Ereignisse dürfen einen Sound auslassen; Munition und Schaden werden davon nicht beeinflusst. Fehlende Profile, leere Sound-Sets und Null-Referenzen werden abgefangen. Das technische Ereignis kann auch ohne zugewiesenen Sound verarbeitet werden.

## Dateien

Alle folgenden Source-Dateien liegen unter `H:\Vanthex\Source\Vanthex`.

| Neu | Zweck |
| --- | --- |
| `VanthexBulletAudioComponent.h/.cpp` | Geometrie, Relevanz, Surface-Auflösung, Tags und lokale Wiedergabe |
| `VanthexBulletAudioProfile.h/.cpp` | Editierbares Data Asset und Sound-Sets |
| `VanthexBulletAudioSettings.h` | Profilreferenz in Projekteinstellungen |
| `VanthexBuildBulletAudioCommandlet.h/.cpp` | Wiederholbarer WAV-Import und Erstellung der Standardassets |
| `VanthexBulletAudioTests.cpp` | Automation-Tests für Geometrie, Grenzen, Konfiguration und echte Wandkollision |
| `VanthexBulletAudioNetworkTest.cpp` | Ausdrücklich aktivierte Netzwerk-, Schnellfeuer- und Fehlreferenz-Prüfung |
| `VanthexBulletAudioPlaybackTest.cpp` | Ausdrücklich aktivierte lokale Mixer-Aufnahme für Positionsprüfung |
| `H:\Vanthex\Run-BulletAudio-Checks.ps1` | Automation, bestehende Regression und separate Netzwerkprozesse |
| `H:\Vanthex\Scripts\verify_bullet_audio_playback.py` | Prüft tatsächliche Mixer-Samples, Richtung und Pegel |
| `H:\Vanthex\AudioSource\Bullet\*` | 16 WAVs, Generator, Manifest und Quellenbeschreibung |
| `H:\Vanthex\Content\Vanthex\Audio\Bullet\*` | SoundWaves, Data Asset, Attenuation und Concurrency |
| `H:\Vanthex\Docs\BULLET_AUDIO.md` | Diese Integrations- und Prüfungsdokumentation |

Geändert wurden `HitscanWeapon.h/.cpp` (kosmetische Konfiguration und Dispatch), `VanthexPlayerController.h/.cpp` (Komponente, RPC, gemeinsame Zuschauerwahl und Test-Startpunkte), `VanthexBotController.cpp` (Ruhe für ausdrücklich aktivierte Testläufe), `Vanthex.Build.cs` (benötigte Module) und `H:\Vanthex\Config\DefaultGame.ini` (Profilreferenz und Cook-Ordner). Die Sicherung vor den Änderungen liegt unter `H:\Vanthex\_versions\20261101_before-bullet-audio`.

## Prüfplan und tatsächliche Ergebnisse

**Ausgeführt am 1. November 2026, Ergebnis PASS:** Editor und Game Win64 Development kompiliert; alle drei neuen Automation-Testgruppen und alle 43 bestehenden Gameplay-Prüfungen bestanden. Der Listen-Host und beide getrennten Remote-Clients melden PASS. Bei sechs Figuren (drei Clients und drei Bots) erhielt der Host 16 begrenzte Near-Miss-Pakete; beide entfernten Clients erhielten exakt ihr jeweils einziges relevantes Ereignis. Seitliche und vorgezogene Wände verhinderten weitere Zustellungen. Alle Figuren behielten 100 HP. Auch das vorübergehende Entfernen sämtlicher Sound-Zuordnungen blieb sicher.

Die tatsächliche Audioaufnahme besteht ebenfalls: 48 kHz Stereo, 4,992 Sekunden, Spitzenpegel 0,1057 ohne Clipping. Linkes Ereignis: RMS links 0,008247 / rechts 0,000006; rechtes Ereignis: links 0,000006 / rechts 0,008218. Der Steel-Einschlag ist mittig mit RMS 0,001974 auf beiden Kanälen. Genau drei Sounds wurden gespielt; zwei absichtlich doppelte/verspätete Pakete wurden verworfen. Die 19 erzeugten Unreal-Assets wurden ohne Importfehler oder Warnungen gespeichert; alle 16 WAV-Quellen wurden zusätzlich auf Format, Pegel und Nicht-Stille geprüft.

Kompakte Nachweise werden unter `H:\Vanthex\Docs\Validation\BulletAudio` mitversioniert. Die 30/100/250/500-cm-Fälle sind Geometrieprüfungen; die echte Netzwerkprüfung verwendet gültige Schüsse oberhalb der Figuren. Vorhandene Hitboxen wurden nicht für einen 30-cm-Schuss verändert.

| Auftrag | Prüfung / Nachweis |
| --- | --- |
| A — 30 cm | Automation: Close-Band, korrekter nächster Punkt und Abstand. |
| B — 100 cm | Automation: Medium-Band und geringere Intensität. |
| C — 250 cm | Automation: Far-Band und positive, dezente Intensität. |
| D — 500 cm | Automation: kein Near-Miss. |
| E — Wand | Automation: kein Ereignis hinter dem gekürzten Ende; echte Kollisionswelt prüft seitliche Wand und Freigabe nach deren Verschieben. |
| F — Schnellfeuer | Netzwerkfixture: mehrere bestätigte Schüsse, kontrollierte Ereignisrate; Concurrency zusätzlich im Asset begrenzt. |
| G — Multiplayer | Separater Listen-Host und zwei Remote-Clients, ergänzt um drei Bots: sechs Figuren in einer 3v3-Runde. Listener erhalten nur ihre relevanten Ereignisse. Dies ist kein Test mit sechs menschlichen Clients. |
| H — fehlende Assets | Netzwerkfixture: vorübergehend leere Sound-Zuordnung; Ereignisverarbeitung ohne Absturz. |
| Regression | Bestehender `VanthexSelfTest`: Waffen-, Core-, Extraktions- und Rundenprüfungen. |
| Tatsächliche Audiowiedergabe | Opt-in `-BulletAudioPlaybackTest`: Mixer-WAV mit links/rechts platziertem Near-Miss und Steel-Impact. Nichtleere Datei und Wiedergabeaufrufe allein beweisen noch keine hörbaren Samples; Pegel und Kanalrichtung zusätzlich prüfen. |

Standardlauf nach dem Editor-Build:

```powershell
& 'H:\Vanthex\Run-BulletAudio-Checks.ps1' -IncludePlayback
```

Der Runner akzeptiert nur aktuelle PASS-Berichte und erfolgreiche Automation-Ergebnisse. Berichte liegen unter `H:\Vanthex\Saved`: `BulletAudioAutomation-*/index.json`, `VanthexSelfTest.txt` und `BulletAudioNetwork-Host.txt`, `BulletAudioNetwork-Client1.txt`, `BulletAudioNetwork-Client2.txt`. Die Mixer-Prüfung erzeugt `BulletAudioPlayback.txt` und `BulletAudioPlayback.wav`.

Die optionale Mixer-Prüfung benötigt ein Audiogerät und echte Grafikinitialisierung. `-nullrhi` erzeugte hier keine verwertbare Mixeraufnahme. Der Runner verwendet deshalb `-RenderOffscreen`; `au.NeverDisableSubmixes=1` erhält die stillen Zeitabschnitte in der Testaufnahme. Diese Einstellungen gelten nur für den Prüfprozess.

## Grenzen und offene Punkte

- Die Prototyp-Synthese ist austauschbar. Klangrealismus, Lautstärke gegenüber Schritten und Hörbarkeit bei langem 3v3-Gefecht benötigen einen bewussten Hörtest und abschließendes Mixing.
- Die räumlichen One-Shots enthalten keine physikalische Geschosslaufzeit, Doppler-Bewegung, Raumsimulation oder Beugung um Wände. Die Wandprüfung ist bewusst konservativ. Während eines bereits gestarteten kurzen Sounds wird die Sichtverbindung nicht fortlaufend neu geprüft.
- Der aktuelle Netzwerktest deckt drei echte PlayerController über drei Prozesse ab. Sechs menschliche Clients, WAN-Latenz/Paketverlust und ein eigener Dedicated-Server-Prozess sind damit nicht nachgewiesen.
- Kameraübergänge in der Zuschaueransicht und Replikationsverzögerungen können kurzzeitig dazu führen, dass ein kosmetisches Ereignis ausgelassen wird. Der Server bleibt für die Auswahl relevanter Ereignisse maßgeblich.
- `MetalGrate` benötigt eine passende Zuordnung auf den getroffenen Kollisionsflächen. Generisch als `Metal` markierte vorhandene Geometrie klingt als Steel, bis ihre Materialkennzeichnung verfeinert wird.
- Ein Ricochet-Tag ist vorhanden; ohne tatsächliche Abpralllogik wird kein irreführender Abprall-Sound erzeugt. Charakter-Treffergeräusche und weitere Waffenprofile sind eigene spätere Audio-Erweiterungen.
