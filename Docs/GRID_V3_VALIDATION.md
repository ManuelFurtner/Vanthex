# GRID V3 — Prüfbericht

## Treppenkorrektur vom 2. November 2026

Alle vier Treppen wurden in der gespeicherten `GridV3.umap` neu aufgebaut. Das Kit-Modell steigt entlang seiner lokalen negativen X-Achse; die alte Drehung zeigte in die Gegenrichtung. Außerdem umfasst seine Bounding Box das Geländer und eignet sich nicht zum Anpassen der Stufenhöhe. Die neue Platzierung verwendet die tatsächlichen unteren und oberen Trittflächen und drei Abschnitte mit zusammen 48 Stufen je Treppe. Die erste Stufe liegt 12,5 cm über der unteren Plattform, die letzte genau auf der oberen Plattform. Ein dekorativer Brückenträger wurde an der Öffnung von Treppe 04 geteilt, damit er nicht durch deren oberste Stufen ragt.

- Editor- und Game-Build (Win64 Development): erfolgreich.
- Geometrie und Navigation: **150 Prüfungen bestanden**, einschließlich 21 Proben an echten sichtbaren Mesh-Dreiecken je Treppe und beiden Anschlüssen. Die neue Prüfung schlägt auf der alten gespeicherten Karte erwartungsgemäß fehl.
- Gameplay: **34 Prüfungen bestanden**, darunter alle acht tatsächlichen Auf- und Abstiege (3,65–3,69 s).
- Acht gerenderte Treppenansichten kontrolliert; die obere Ansicht von Treppe 04 wurde nach der Trägerkorrektur erneut geprüft. Diese letzte Änderung betrifft ausschließlich einen dekorativen Träger; die 1.260 Kollisionskörper bleiben unverändert. Geometrie und Navigation wurden danach erneut geprüft.

Die Stufenhöhen liegen bei ungefähr 11,6–13,4 cm. Die glatte Lauframpenkollision bleibt erhalten; ihre maximale Abweichung von den geprüften sichtbaren Trittflächen beträgt 12,23 cm. Das ist eine vereinfachte Bewegungskollision, keine exakte Fußplatzierung auf jeder Stufe.

Nachweise: [Geometrie](Validation/GridV3Stairs/geometry.txt), [Gameplay](Validation/GridV3Stairs/gameplay.txt), [vorher](Validation/GridV3Stairs/before.png), [nachher unten](Validation/GridV3Stairs/after-bottom.png), [Treppe 04 oben](Validation/GridV3Stairs/after-stair04-top.png). Wiederholen: `-GridV3Test`, `-GridV3GameTest` beziehungsweise `-GridV3Shots -GridV3StairShots` auf `/Game/Vanthex/Maps/GridV3`. Einzelansichten sind mit `-GridV3Shots -GridV3Shot=16` bis `23` abrufbar.

## Ursprünglicher Gesamtdurchlauf

Stand: 1. November 2026, abschließender Durchlauf. Geprüft wurde die eigenständige Karte `/Game/Vanthex/Maps/GridV3` mit 23 Räumen auf −600, 0 und +600 cm, 37 Verbindungseinträgen und vier unabhängigen Treppenrouten.

## Ergebnisse

| Prüfung | Ergebnis | Nachweis / Zeitpunkt |
|---|---|---|
| `VanthexEditor`, Win64 Development | Build erfolgreich | Unreal Engine 5.8 |
| `Vanthex`, Win64 Development | Build erfolgreich | Unreal Engine 5.8 |
| Geometrie und Navigation | **149 PASS, 0 FAIL** | [GridV3Validation.txt](../Saved/GridV3Validation.txt), 18:22:32 |
| Tatsächliche Gameplay-Abläufe | **34 PASS, 0 FAIL** | [GridV3Gameplay.txt](../Saved/GridV3Gameplay.txt), 18:23:52 |
| Bestehende Mechaniken / Regression | **43 PASS, 0 FAIL** | [VanthexSelfTest.txt](../Saved/VanthexSelfTest.txt), 18:22:21 |
| Listen-Server auf GRID V3 | **PASS** | [GridV3NetworkHost.txt](../Saved/GridV3NetworkHost.txt), 18:24:10 |
| Separater Netzwerk-Client | **PASS** | [GridV3NetworkClient.txt](../Saved/GridV3NetworkClient.txt), 18:24:18 |

Der Geometrietest prüft alle sechs tatsächlichen Spawn-Kapseln, sämtliche neun gegnerischen Spawn-Sichtlinien, Hin- und Rückwege zu allen 23 Räumen auf ihrer richtigen Etage, alle Verbindungsbiegungen, vier Treppen in beiden Richtungen sowie Core und beide Extraktionen. Der Kollisionsgraph umfasst 1.694 unterstützte Standpositionen und 4.670 mit einer Spielerkapsel geprüfte Verbindungen in einer zusammenhängenden Komponente. Alle direkten gegnerischen Spawn-Sichtlinien sind blockiert.

Der Gameplay-Test nutzt den laufenden Core, die Rundenuhr und CharacterMovement: Aufnahme durch beide Teams, Fallenlassen, Abwurf beim Tod, Wurf durch einen Türdurchgang, Mitspielerfang, gegnerisches Abfangen, Sprungpass von der Beobachtungsebene zum Hauptgeschoss, Abfangen durch die Bodenöffnung im Untergeschoss, erneute Aufnahme dort und Rücksetzung aus dem Kartenuntergrund. Das Verlassen einer Extraktion bricht den Fortschritt ab.

| Extraktionsfall | Gemessene Dauer bis zur Wertung |
|---|---:|
| Alpha → A | 4,95 s |
| Alpha → B | 5,00 s |
| Bravo → A | 5,00 s |
| Bravo → B | 5,00 s |

Die unveränderte Spielregel beträgt **5,00 Sekunden**; die Messung erfolgt mit einem Testintervall von 0,05 s. Alle vier Treppen wurden mit tatsächlicher Charakterbewegung aufwärts und abwärts gelaufen: sieben Durchläufe dauerten 3,65 s, der Abstieg an Verbindung 1 dauerte 3,69 s. Die Wegpunkte am Start und Ziel enthalten jeweils einen kurzen Übergang zur ebenen Landung.

Im Netzwerktest meldeten beide Prozesse sechs Charaktere, einen Core, keine aktiven SHIFT-Geräte und `offline=0`. Der Host registrierte 532,7 cm Bewegung und 28 Patronen; der Client 344,1 cm Bewegung, 60 Lebenspunkte nach Schaden und 28 Patronen. Dies belegt den getesteten lokalen Zwei-Prozess-Ablauf, keine vollständige Prüfung unter Internet-Latenz.

## Gemessene Wege

Die Distanzen stammen aus dem tatsächlichen Kollisionsgraphen mit 150-cm-Raster. Geh- und Sprintzeiten sind **berechnete Näherungen** bei 600 beziehungsweise 900 cm/s, keine gestoppten Spielerdurchläufe.

| Start → Core | Strecke | Gehen | Sprint |
|---|---:|---:|---:|
| Alpha 1 | 54,92 m | 9,15 s | 6,10 s |
| Alpha 2 | 53,21 m | 8,87 s | 5,91 s |
| Alpha 3 | 51,92 m | 8,65 s | 5,77 s |
| Bravo 1 | 50,83 m | 8,47 s | 5,65 s |
| Bravo 2 | 52,54 m | 8,76 s | 5,84 s |
| Bravo 3 | 52,54 m | 8,76 s | 5,84 s |

Alpha erreicht im Mittel **53,35 m / 8,89 s**, Bravo **51,97 m / 8,66 s**. Die relative Wegdifferenz beträgt **2,7 %** und liegt unter der gewählten Prüfgrenze von 10 %. Das ist eine geometrische Annäherung an faire Startwege; Sichtwinkel, Deckung und menschliche Entscheidungen müssen separat im 3v3 getestet werden.

| Start | Extraktion A: Strecke / Gehen | Extraktion B: Strecke / Gehen |
|---|---:|---:|
| Alpha 1 | 53,14 m / 8,86 s | 99,23 m / 16,54 s |
| Alpha 2 | 52,31 m / 8,72 s | 97,53 m / 16,25 s |
| Alpha 3 | 53,14 m / 8,86 s | 96,23 m / 16,04 s |
| Bravo 1 | 88,27 m / 14,71 s | 53,81 m / 8,97 s |
| Bravo 2 | 89,98 m / 15,00 s | 53,40 m / 8,90 s |
| Bravo 3 | 89,98 m / 15,00 s | 53,40 m / 8,90 s |
| Core | 50,46 m / 8,41 s | 47,10 m / 7,85 s |

Beide Teams können beide Extraktionen verwenden. Ab dem Core ist B geometrisch 3,36 m beziehungsweise nominal 0,56 s näher als A. Zur jeweiligen Laufzeit kommt der fünfsekündige Extraktionshalt hinzu. Die asymmetrischen äußeren Rotationen sind bewusst unterschiedlich lang und bleiben ein Thema für menschliche Spieltests.

## Umfang und verbleibende Prüfung

Die Geometrieprüfung verwendet die tatsächliche statische Kollision, Kapselradius 42 cm und halbe Höhe 96 cm sowie Bodenproben entlang der Kanten. Die acht live gelaufenen Treppen ergänzen diese Abfragen. Das beweist keine Freiheit von jeder denkbaren Stelle zum Hängenbleiben, jeder dynamischen Blockade oder jedem Mehrspieler-Gedränge. Beschleunigung, Kampf und Spielentscheidungen fehlen in den berechneten Reisezeiten.

Die abschließenden Grafikänderungen betreffen Materialien, sichtbare Deckenmodule und die Extraktionsdarstellung. Die Gameplay-Kollisionen bleiben dabei unverändert. Alle 15 finalen Ansichten sind in [GRID_V3_GALLERY.html](GRID_V3_GALLERY.html) aus `Saved/GridV3Shots/final/` vorhanden. Das Protokoll der gerenderten Prüfung steht in [GRID_V3_REVIEW.md](GRID_V3_REVIEW.md). Die Extraktionsmodelle behalten sehr dunkle Sockelflächen; Charaktere und Waffen verwenden weiterhin die bisherigen Prototypmodelle.

Foundry.umap ist unverändert (SHA-256: B070B57FC24672C662984E204BF924B724C2D021EDD0480DE1D6C90C27604B51). Die alten GRID- und Foundry-Bauquellen stimmen ebenfalls mit der Sicherung vor GRID V3 überein. Beide aktuellen Build-Ziele wurden nach den letzten Quelländerungen erfolgreich erstellt.

Es wird **keine FPS-Zahl und keine abgeschlossene kompetitive Balance** behauptet. M4 ist nicht implementiert; die Karte enthält dafür nur architektonische Plätze und markierte Anschlusspunkte.

## Wiederholen und öffnen

`Run-GridV3-Checks.ps1` führt Regression, Geometrie, Gameplay und den lokalen Host/Client-Test aus und verlangt frische PASS-Berichte. `Start-GridV3.cmd` startet die Karte; `Open-GridV3-Editor.cmd` öffnet sie im Editor. Die gespeicherte Map liegt unter `Content/Vanthex/Maps/GridV3.umap`.

Startkorrektur: Auch `Start-Vanthex.cmd` startet jetzt ausdrücklich GRID V3. Die Standardkarten für Spiel, Editor und Server stehen auf `/Game/Vanthex/Maps/GridV3`. Ein Start ohne Kartenargument wurde geprüft (`Saved/GridV3-default-start.log`: `LoadMap: /Game/Vanthex/Maps/GridV3`). Die alte Karte bleibt über `Start-Foundry.cmd` erreichbar; ihre bisherigen Tests verwenden einen expliziten Foundry-Pfad.
