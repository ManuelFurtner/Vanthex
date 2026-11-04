# Audio und Spielmenü – 4. November 2026

Basis: main `7eac32d` mit Aegis GRID V4, Wirtschaft, Gadgets und bestehendem
räumlichem Waffen-Audio. Dieser Umbau ist auf `codex/audio-menu-polish`.

## Menü

Escape öffnet ein natives Slate-Menü mit dunklen Flächen, türkiser Navigation,
Runden-/Spielstand und drei Bereichen: Spielen, Einstellungen und Steuerung.
Das Layout skaliert bei kleinen Viewports herunter; Inhaltsseiten sind scrollbar.
Aktive Navigation wird hervorgehoben. Buttons und Slider sind per Tastatur bedienbar.
Escape schließt auch dann, wenn ein Eingabefeld den Fokus hat.

Spielen bietet Fortsetzen, LAN-Hosting, gespeicherte Hostadresse, direkten Beitritt
und Sitzungsverlassen. Hosting/Verlassen benötigen einen zweiten Klick auf dieselbe
Schaltfläche, damit eine laufende Sitzung nicht versehentlich ersetzt wird.
IPv4/Hostname und optionaler Port werden vor dem Beitritt geprüft; ungültige Eingaben
zeigen einen Text statt die Sitzung zu verlassen. IPv6 und URL-/Travel-Optionen sind
für dieses Eingabefeld nicht vorgesehen. Erreichbarkeit des Hosts wird dadurch
nicht garantiert; Engine-Verbindungsfehler bleiben über den bestehenden Netzwerkpfad sichtbar.

Während das Menü offen ist, besitzt es exklusiven UI-Eingabefokus. Waffenfeuer,
Sprint und ADS werden beim Öffnen beendet; Bewegung und Blick sind gesperrt.
Die Online-Runde läuft weiter. Beim Schließen kehren Maus und Spieleingabe zurück.
Das vorhandene Kaufmenü bleibt erhalten und wird beim Öffnen des Sitzungsmenüs geschlossen.

## Einstellungen und Mix

Lokale Lautstärken werden in GameUserSettings.ini gespeichert und beim nächsten
Levelstart wieder angewendet: Gesamt 80 %, Waffen/Bewegung 85 %, Kauf-/UI-Hinweise
55 %, Waffen-Raumreflexionen 65 %. Gesamtlautstärke steuert das lokale Audiogerät;
die anderen Regler betreffen den Weapon-/Bullet-Audiorouter. Gadget- und
Umgebungsklang folgen der Gesamtlautstärke. Werte werden auf 0–100 % begrenzt.
Audio wird sofort geändert und beim Schließen gespeichert; ein UI-Testton hilft
beim Einstellen. Bereits gestartete kurze Waffensounds behalten ihren bisherigen
Einzelpegel, bis das nächste Ereignis beginnt.

Grafikqualität, VSync und FPS-Limit (60/120/144/240/unbegrenzt) werden ausdrücklich
mit „Grafik anwenden“ übernommen und über Unreal gespeichert. Das FPS-Limit ist
keine Server-Tickrate. Bildschirmauflösung und Anzeigemodus werden hier nicht geändert.

Die frühe Raumreflexion verwendet jetzt eine leise, tiefpassgefilterte Kopie der
tatsächlich gespielten Waffenvariante. Damit bleibt AUG/MP7-Charakter auch in der
Reflexion erhalten. Die vorhandenen räumlichen Quellen, Portalwege, Wanddämpfung,
Prioritäten, Netzereignisse und Trefferregeln bleiben maßgeblich. Dies ist eine
vereinfachte frühe Reflexion, keine vollständige physikalische Raumakustik.

## AUG-/MP7-Quellen

Sechs eigene Mono-Quelldateien wurden neu synthetisiert: AUG mit längerem,
kräftigerem Druckkörper; MP7 mit kürzerem, höherem Impuls. Geräuschschichten und ein
kurzer mechanischer Anteil ersetzen den langen tonalen Ausklang. Transienten
haben einen 0,75-ms-Einblendrand; Peaks behalten etwa 6,38 dB Headroom.
Es handelt sich um deterministische Synthese, keine professionellen Waffenaufnahmen.
Die optionale Fab-Bank wird nicht überschrieben.

Die WAVs und das Manifest sind aktualisiert. **Gespeicherte Unreal-SoundWave-Assets
müssen auf einer Maschine mit Unreal noch gezielt aktualisiert werden**, sonst
spielt die vorhandene Bank weiter ihre bisherigen Quelldaten:

```powershell
./Run-AudioMenu-Checks.ps1 -EngineRoot 'H:\epic\UE_5.8' -RefreshFeaturedShots
```

Der neue Commandlet-Schalter ersetzt nur die sechs eigenen AUG-/MP7-Schussassets.
Vorhandene Profileinstellungen und andere vorhandene Sounds bleiben erhalten;
fehlende Assets werden wie bisher erzeugt. Der optionale Fab-Override bleibt aktiv,
falls vorhanden. `-VanthexOwnWeaponAudio` wählt für den Hörtest die eigene Bank.

## Prüfstand

Lokal ausgeführt: alle **119 WAV-Quellen** gegen Manifest/Hash, Mono PCM16/48 kHz,
Dauer, Nicht-Stille, Spitzenpegel, Randdiskontinuitäten und unterschiedliche
AUG-/MP7-Varianten geprüft. Bericht: `Validation/AudioMenu/source-audio.json`.
Die Reproduzierbarkeit der Synthese und der Patch wurden zusätzlich geprüft.
Das sind Quelldateiprüfungen und kein Nachweis der Unreal-Mixerausgabe.

Neue Unreal-Automation: `Vanthex.Menu.ConnectionInput` und `Vanthex.Menu.AudioBounds`.
Der Runner kompiliert Editor/Game und verlangt erfolgreiche frische Testberichte.
Hier fehlen Unreal Engine und Build-Toolchain: **Build, Automation, SoundWave-Reimport,
gerenderte Menüprüfung und Hörtest wurden für diese Änderung nicht ausgeführt.**

Vor Übernahme nach main erforderlich:

1. Editor/Game bauen, Menütests und bestehende Wirtschafts-/Waffen-Audiotests ausführen.
2. Menü bei 1280×720, 1920×1080 und Ultrawide öffnen; Tabs, Scrollen, Tastaturfokus,
   Escape aus dem Adressfeld und wiederholtes Öffnen/Schließen kontrollieren.
3. BuyPhase-/Rundenwechsel bei offenem Menü testen; kein verdecktes Feuern, kein
   blockierter Mauszeiger nach dem Schließen. Spielstand und Timer aktualisieren sich.
4. Host/Join/Leave, ungültige und unerreichbare Adressen sowie echte Netzwerkfehler
   testen. Audio/Grafik speichern, Level wechseln und neu starten.
5. Eigene AUG/MP7-Bank reimportieren; direkten Schuss, Nachbarraum und obere Plattform
   hören. Abbruch beim Nachladen und 3v3-Mix prüfen. Reflexionen dürfen keine
   zusätzliche Schussfolge vortäuschen. Tatsächliche Mixeraufnahme auf Clipping prüfen.
