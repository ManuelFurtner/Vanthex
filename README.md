# VANTHEX

VANTHEX ist ein C++-Prototyp für einen taktischen 3v3-FPS mit Unreal Engine 5.8. Die aktuelle Startkarte ist **GRID V3 / MERIDIAN**. Das Projekt enthält Source, Konfiguration, Karten und Grafik-/Audio-Assets; lokale Build-Dateien und Caches werden neu erzeugt.

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

WASD bewegt die Figur, Maus zielt, linke Maustaste schießt, R lädt nach, Umschalt sprintet, Leertaste springt. E nimmt den Core auf, G legt ihn ab und Q wirft ihn. Escape öffnet das Sitzungsmenü für Hosting und direkten Beitritt. GRID V3 verwendet die grundlegenden Kampf-, Core- und Extraktionsmechaniken; ihre markierten zukünftigen SHIFT-Stationen sind noch nicht aktiv.

## Dokumentation und Prüfungen

- [Neues Waffen- und Wirtschaftssystem (Design)](Docs/ECONOMY_AND_WEAPONS.md)
- [Waffenklang und räumliches Audio (Planung)](Docs/WEAPON_AUDIO_DESIGN.md)

- [Puška / nož: prebacivanje, animacija i provjere](Docs/WEAPON_EQUIPMENT.md)
- **1**: puška, **3**: nož, lijevi klik: pucanje ili ubod. Prebacivanje ima animaciju spremanja i izvlačenja.

- [Karte und Layout](Docs/GRID_V3_LAYOUT.md)
- [GRID-V3-Prüfbericht](Docs/GRID_V3_VALIDATION.md)
- [Bullet-Audio: Architektur, Konfiguration und Tests](Docs/BULLET_AUDIO.md)
- [Audio-Quelldateien und erneuter Import](AudioSource/Bullet/README.md)

Nach dem Editor-Build können `Run-BulletAudio-Checks.ps1` und `Run-GridV3-Checks.ps1` ausgeführt werden. Sie erzeugen Berichte im lokalen Ordner `Saved`, der nicht versioniert wird. Bei einem anderen Installationspfad die Engine-Parameter beziehungsweise Skriptpfade anpassen.

Die Grafik-Kits und importierten Modelle sind Fremdassets. Dieses Repository erteilt keine zusätzliche Lizenz zur separaten Weiterverwendung dieser Assets. Die Bullet-Audio-Quelldateien sind austauschbare Synthese-Prototypen; ein abschließendes Produktions-Mixing ist offen.
