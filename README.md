# VANTHEX

VANTHEX ist ein C++-Prototyp für einen taktischen 3v3-FPS mit Unreal Engine 5.8. Die aktuelle Startkarte ist **GRID V3 / MERIDIAN**. Das Projekt enthält Source, Konfiguration, Karten und Grafik-/Audio-Assets; lokale Build-Dateien und Caches werden neu erzeugt.

Spieler und Bots verwenden den ersten animierten Operator **ARC** mit Teamfarben, Helm und Gewehr in der Außenansicht. Offline spielen ein Mensch und fünf Bots im 3v3. Details und Prüfnachweise: [Operator ARC](Docs/OPERATOR_ARC.md); eigener Testlauf: `Run-Operator-Checks.ps1`.

## Voraussetzungen

- Windows, Unreal Engine **5.8**; Entwicklung und Prüfungen erfolgen mit 5.8.3.
- Eine für Unreal eingerichtete Visual-Studio-C++-Toolchain samt Windows SDK.
- Git und **Git LFS**. Unreal-Assets werden über LFS gespeichert; ein Checkout nur mit kleinen Pointer-Dateien ist nicht spielbereit.

## Projekt beziehen

```powershell
git clone https://github.com/ManuelFurtner/Vanthex.git
Set-Location Vanthex
git lfs install --local
git lfs pull
git lfs fsck
```

Der vollständige Content umfasst ungefähr 9,4 GB. Unreal Engine, generierte Binärdateien und lokale Sicherungskopien sind nicht Teil des Repositorys.

## Bauen und starten

Den Engine-Pfad an die eigene Installation anpassen:

```powershell
$VanthexEngineRoot = 'H:\epic\UE_5.8'
$VanthexProjectFile = Join-Path (Get-Location) 'Vanthex.uproject'
& "$VanthexEngineRoot\Engine\Build\BatchFiles\Build.bat" VanthexEditor Win64 Development "-Project=$VanthexProjectFile" -WaitMutex
& "$VanthexEngineRoot\Engine\Binaries\Win64\UnrealEditor.exe" $VanthexProjectFile /Game/Vanthex/Maps/GridV3 -game -windowed
```

Ohne `-game` öffnet sich der Editor. Vor einem vollständigen C++-Build Änderungen speichern und den Editor schließen.

Auf der ursprünglichen Entwicklungsmaschine startet `Start-Vanthex.cmd` direkt das Spiel und `Open-GridV3-Editor.cmd` den Editor. Diese Hilfsskripte verwenden den Engine-Pfad `H:\epic\UE_5.8`; auf einem anderen Rechner muss dieser angepasst werden. Ein eigenständiges paketiertes Windows-Spiel ist damit noch nicht enthalten.

## Steuerung und Spielstand

WASD bewegt die Figur, Maus zielt, **B** öffnet oder schließt das Kaufmenü während der Kaufphase. **1** wählt eine gekaufte Primärwaffe, **2** die Pistole und **3** das Messer. Die linke Maustaste schießt oder führt einen Messerstich aus; **R** lädt mit endlicher Reservemunition nach. Umschalt sprintet, Leertaste springt. **E** nimmt den Core auf, **G** legt ihn ab und **Q** wirft ihn. Escape öffnet das Sitzungsmenü für Hosting und direkten Beitritt. GRID V3 verwendet die grundlegenden Kampf-, Core- und Extraktionsmechaniken; ihre markierten zukünftigen SHIFT-Stationen sind noch nicht aktiv.

Ein Match beginnt mit **800 CR**, Standardpistole und Messer. Vor jeder 180-Sekunden-Runde liegen **25 Sekunden Kaufphase** mit gesperrter Bewegung und gesperrten Angriffen. Überlebende behalten Waffen, verbleibende Rüstung und Gadget-Inventar; HP, Magazine und Reserve werden für die nächste Runde aufgefüllt. Tod kostet die Ausrüstung, Credits bleiben erhalten. Leichte und schwere Rüstung absorbieren Körpertreffer; Kopf, Messer und Umgebungsschaden umgehen sie. Die zwei Gadget-Plätze sind kaufbar, ihre konkreten Wirkungen sind noch nicht umgesetzt.

## Dokumentation und Prüfungen

- [Wirtschaft, Rüstung und Waffen-Audio: Implementierung und offene Punkte](Docs/ECONOMY_IMPLEMENTATION.md)
- [Waffen- und Wirtschaftsregeln](Docs/ECONOMY_AND_WEAPONS.md)
- [Waffenklang und räumliches Audio: Design und aktueller Umfang](Docs/WEAPON_AUDIO_DESIGN.md)

- [Gewehr und Messer: Waffenwechsel, Animationen und Prüfungen](Docs/WEAPON_EQUIPMENT.md)
- [Nachweise der Zusammenführung mit ARC](Docs/Validation/EquipmentMerge)

- [Karte und Layout](Docs/GRID_V3_LAYOUT.md)
- [GRID-V3-Prüfbericht](Docs/GRID_V3_VALIDATION.md)
- [Bullet-Audio: Architektur, Konfiguration und Tests](Docs/BULLET_AUDIO.md)
- [Audio-Quelldateien und erneuter Import](AudioSource/Bullet/README.md)

Nach dem Editor-Build können `Run-BulletAudio-Checks.ps1`, `Run-GridV3-Checks.ps1`, `Run-Operator-Checks.ps1` und `Run-Equipment-Network-Checks.ps1` ausgeführt werden. `Run-Equipment-Checks.ps1` baut Editor und Game und prüft die Waffenintegration. Die Skripte erzeugen Berichte im lokalen Ordner `Saved`, der nicht versioniert wird. Bei einem anderen Installationspfad die Engine-Parameter beziehungsweise Skriptpfade anpassen.

Für die neue Wirtschaft stehen `Run-Economy-Checks.ps1`, `Run-Economy-Network-Checks.ps1` sowie die separaten Diagnosen `-WeaponProfileTest` und `-WeaponAudioPlaybackTest` bereit. Editor/Game, 14 Automationgruppen, 45 Economy-, 32 Core- und 24 Waffenprüfungen sowie der echte Host-/Client-Kaufablauf einschließlich Bot-Rückgabe sind bestanden. Mixeraufnahme und vier aktuelle Renderansichten sind ebenfalls geprüft. [Prüfumfang und Grenzen](Docs/ECONOMY_IMPLEMENTATION.md#vorliegende-validierung) sowie die [Berichte und Aufnahmen](Docs/Validation/EconomyArmorAudio) dokumentieren den Stand.

Die Grafik-Kits und importierten Modelle sind Fremdassets. Dieses Repository erteilt keine zusätzliche Lizenz zur separaten Weiterverwendung dieser Assets. Die Bullet- und Waffen-Audioquellen sind austauschbare Synthese-Prototypen; die neuen Waffenquellen umfassen 119 originale Mono-WAVs. Separate Ego-Arme, Waffenmodelle in Produktionsqualität, abschließendes Mixing und menschliche Hörabnahme bleiben offen.
