# FOUNDRY / Nachtschicht

Neue Standardkarte: `/Game/Vanthex/Maps/Foundry`.
Datei: `Content/Vanthex/Maps/Foundry.umap`.
Die gespeicherte Umgebung kann direkt im Unreal-Editor bearbeitet werden.
Start-Vanthex.cmd und das Host-Menue starten diese Karte.

## Umgebung

200 x 240 Meter Spielflaeche mit hoher Kranhalle, Roboter-Fertigungszellen,
Reaktorbruecke, vier Lager-/Versorgungsbezirken und Bahnverladung als langem
Flankenweg. Die gepruefte GRID-Grundstruktur fuer Ziele und Aufzuege wurde
weiterverwendet; Maschinen, Fracht und zusaetzliche Lagergassen veraendern die
Deckungs- und Wegfuehrung. Es ist keine unveraenderte Demo-Karte des Asset-Packs.

Das vom Nutzer bereitgestellte FactoryEnvironmentCollect-Kit liefert originale
Meshes, Texturen, Materialien und Fabrik-Ambiente. Die Originalprojektdateien
bleiben im Kit-Ordner. Die benoetigten Content-Pfade wurden unveraendert in das
Hauptprojekt uebernommen, damit Material-/Texturverweise erhalten bleiben.
100 Material-Assets, darunter 19 Master-Materialien, wurden in der Arbeitskopie
fuer Instancing vorbereitet. Wiederholte Objekte sind als Mesh-Instanzen
organisiert. Lumen, virtuelle Schatten, TSR, Dunst und warme/kalte Arbeitslichter
sind aktiv; fertige FPS-/VRAM-Benchmarks auf mehreren PCs stehen noch aus.

## Gameplay

Fuenf Fahrstuehle, vier hackbare Stromkreise, 24 Wartungsverstecke,
sechs Teamstarts, neutraler Kern und zwei Extraktionen. F bedient/hackt,
C nutzt ARC-Fernsteuerung. Ein Hack dauert vier Sekunden und bricht bei Schaden,
Entfernung oder Sichtunterbrechung ab. Rampen bleiben bei Stromausfall nutzbar.
Gameplay-Objekte entstehen serverseitig beim Start; die gespeicherte Karte
enthaelt die Umgebung und Beleuchtung ohne doppelte Kern-/Geraeteobjekte.

## Bearbeitung und Wiederherstellung

Die Factory-Kit-Instanzgruppen und Struktur-/Kollisionsobjekte sind im World
Outliner unter FOUNDRY sortiert. Manuelle Kartenbearbeitung ist moeglich.
Die Editor-Commandlet-Option -BakeFoundry erzeugt die Karte erneut aus
VanthexFoundryMap.cpp und ueberschreibt dabei manuelle Aenderungen an Foundry.umap.
Nicht nach manueller Bearbeitung ohne separate Sicherung ausfuehren.

Die alte Laufzeitkarte ist ueber /Engine/Maps/Entry -VanthexLegacyGrid erreichbar.
Mechanik-Selbsttests verwenden /Engine/Maps/Entry -VanthexSelfTest.
Run-Checks.ps1 prueft Regressionen, Karte, Energie/Aufzuege und Host/Client.
-VanthexMapShots schreibt acht Ansichten nach Saved/FoundryShots.

Backup vor diesem Umbau: _versions/20261101_before-foundry.

Geprueft: Editor-/Game-Build erfolgreich; 43 Mechaniktests, 69 Kartenpruefungen,
17 Energie-/Aufzugstests und Zwei-Prozess-Host/Client-Test auf Foundry bestanden.
Client und Host sehen sechs Charaktere, einen Kern, 15 Geraete und denselben
abgeschalteten Stromkreis. Die gespeicherte Karte verwendet 32 Kit-Mesh-Typen
mit 1.396 Instanzen. Waffen-/Figurenanimationen und weitere Release-Arbeit:
siehe NEXT_STEPS.md.
