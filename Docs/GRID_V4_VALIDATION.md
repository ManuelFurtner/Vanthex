# GRID V4 / AEGIS — Reparaturprüfung vom 4. November 2026

Das bestehende Level `/Game/Vanthex/Maps/GridV4` wurde überarbeitet und neu gespeichert. `Start-Vanthex.cmd` und `Start-GridV4.cmd` starten diese Karte; `Open-GridV4-Editor.cmd` öffnet sie im Editor.

## Reparaturen

- Treppen öffnen die Außenhülle nur an den tatsächlichen Ein- und Ausgängen. Unter den Rampen schließen Wände die zuvor offenen Ränder. Geländer ersetzen Außenwände nur dort, wo darunter spielbarer Boden liegt.
- Treppenseiten haben durchgehende schräge Verkleidungen und begrenzte Kollisionssegmente, die nicht in die oberen Querwege hineinragen.
- Bodenmöbel werden nur auf unterstützten Flächen und ohne Überschneidung mit vorhandenen Hindernissen gesetzt. Überschneidende Eckstützen und schwebende Schaltschränke wurden entfernt; angebautes Zubehör hängt von der erfolgreichen Platzierung seines Trägers ab. Konsolenmodelle wurden ersetzt, Lüftung und Träger an Decken angeschlossen, der obere Ring erhielt Aufhängungen.
- Der Core steht in einem kleineren Innenbereich mit 4,2 m hohen, versetzten Schutzwänden, vier versetzten Zugängen und einem gestützten Dach. Fünf direkte Schusslinien aus den Hauptrichtungen und von oben sind blockiert. Die vorgesehene Passöffnung zum unteren Spielbereich bleibt erhalten.
- Hellere Raum- und Wegbeleuchtung, geringere Leuchtmaterial-Intensität und weniger Bloom verbessern die Lesbarkeit. Leuchten sitzen an Decken beziehungsweise unter oberen Plattformen.

## Aktueller Stand und Ergebnisse

25 Räume, 46 Verbindungen, sechs Treppen; 11.802 Kit-Instanzen in 27 Gruppen, 1.753 Kollisionskörper, 194 Lichtquellen, kein fehlendes Kit-Mesh beim Erstellen.

| Prüfung | Ergebnis |
|---|---|
| UE 5.8.3 Editor und Game, Win64 Development | Beide Builds erfolgreich |
| Kartenprüfung auf dem endgültig gespeicherten Level | 188/188 bestanden |
| Außenhülle: 7.954 Bodenpositionen, 83.748 Kapsel-Sweeps | 0 ungeschützte Ausgänge ins Leere oder auf nicht spielbare Dächer |
| 62 bodenstehende Kit-Möbel, neun Stützpunkte pro Grundfläche | 0 ohne Bodenstützung |
| Gameplay mit echter Spiellogik und CharacterMovement | 41/41 bestanden |
| Alle sechs Treppen auf- und ablaufen | 12/12 bestanden, jeweils etwa 3,65 s |
| Gehen und Springen gegen drei zuvor offene Begrenzungen | 3/3 bestanden |
| Extraktion beider Teams an beiden Zielen | 4/4 bestanden, 14,95–15,00 s bei 50-ms-Testtakt |
| Core-Aufnahme, Abwurf, Tod, Würfe und Abfangen | Bestanden |
| Renderaufnahmen | 22 Ansichten; nach letzter optischer Korrektur Ansichten 7, 11 und 17 erneut aufgenommen |

Die neue Außenhüllenprüfung fand am alten gespeicherten Level 80 unsichere Proben. Das Baseline-Protokoll ist absichtlich als fehlgeschlagen aufbewahrt. Die ältere Routenprüfung allein hatte diese Fehler nicht erkannt.

Der vollständige Gameplay-Lauf erfolgte vor dem letzten Austausch der sichtbaren Treppenverkleidung und Konsolenmeshes. Diese rein optischen Änderungen ließen die 1.753 Kollisionskörper unverändert; beide Builds, Kartenerstellung, alle 188 Kartenprüfungen und die drei betroffenen Bildaufnahmen wurden danach wiederholt.

Core → A: 109,92 m / nominell 18,32 s; Core → B: 122,86 m / 20,48 s, jeweils zusätzlich 15 s Extraktion. Mittlere Spawnwege: Alpha 57,99 m, Bravo 56,23 m, Unterschied 3,1 %. Ab Landebereich der unteren Passöffnung bleiben 85,58 m bis B; Anlauf und Fallzeit sind darin nicht enthalten.

## Reproduktion und Grenzen

`Run-GridV4-Checks.ps1` enthält Karten-, Gameplay-, Core- und Netzwerkprüfungen. Erstellen bei geschlossenem Editor: `UnrealEditor-Cmd.exe Vanthex.uproject -run=VanthexBuildArt -BakeGridV4 -nullrhi -unattended`. Rendern: `UnrealEditor.exe Vanthex.uproject /Game/Vanthex/Maps/GridV4 -game -GridV4Shots -RenderOffscreen -ResX=1280 -ResY=720`; `-GridV4Shot=7` wählt eine Ansicht.

Die Außenhüllenprüfung tastet Boden im 100-cm-Raster in vier Richtungen ab, mit stehender beziehungsweise geduckter Kapsel und bis zu 150 cm angehobenen Sprungproben. Sie ist kein Beweis für jede denkbare Bewegung oder Physikkombination. Die Möbelprüfung umfasst 62 ausgewählte bodenstehende Kit-Instanzen, nicht jedes dekorative Anbauteil. Die Routenlängen sind approximativ; Gefechtsbalancing und menschliches Spielgefühl benötigen weitere Spielrunden. Kamera-Zeitfenster sind kein Gefechtsbenchmark.

Die früheren 24 allgemeinen Automation-Tests, 150 GRID-V3-Prüfungen und Host-/Client-Prüfungen sind im historischen Nachweisordner dokumentiert; sie wurden in diesem auf die Karte begrenzten Reparaturlauf nicht erneut durchgeführt. Aktive SHIFT-Hacks/Aufzüge bleiben vorbereitete Integrationspunkte. Charakter-Hitboxen bleiben die vorhandenen vereinfachten Proxies.

Fab-Pakete und daraus erzeugte Materialinstanzen unter `Content/Vanthex/Materials/Aegis` bleiben lokal und von öffentlichen Uploads ausgeschlossen. Dieser Reparaturlauf enthält keinen GitHub-Upload.

[Aktuelle Protokolle, Messfenster und Bilder](Validation/GridV4Repair)

[Historischer Nachweis vor der Reparatur](Validation/GridV4)
