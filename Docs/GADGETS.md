# Gadget-Regeln und Prüfungen

Stand: **4. November 2026**. Die fünf Gadget-Wirkungen sind im Code umgesetzt.
Die folgenden Parameter sind Ausgangswerte für Playtests, keine gemessene Balance.
Sechs Automationgruppen, 58 Prüfungen im echten Weltlauf und beide Rollen des
Host-/Client-Tests haben bestanden. Die Berichte sind unter
[Validation/GadgetsWeaponsAim](Validation/GadgetsWeaponsAim) archiviert; die
technischen Tests ersetzen keine Balance- oder visuelle Abnahme.

## Bedienung und gemeinsame Regeln

**4** wirft das erste, **5** das zweite gekaufte Gadget in Blickrichtung. Nach
Verbrauch rückt ein verbleibender Eintrag auf den ersten Platz. Zwei gleiche Typen
sind erlaubt. Käufe erfolgen weiterhin mit **B** in der 25-sekündigen Kaufphase;
Gadgets können erst in der aktiven Runde eingesetzt werden.

Der Server prüft lebenden Besitzer, Team, Phase, Inventarplatz, monoton steigende
Request-ID, eine Wurfsperre von 0,8 Sekunden und freien Platz vor der Kamera. Er
verwendet seine eigene Position und Blickrichtung. Ein fehlgeschlagener Spawn
verbraucht kein Gadget. Erst ein gültig erzeugter Wurf entfernt genau einen
Inventareintrag. Wiederholte und ältere Requests können auch nach einem
Rundenwechsel keine neu gekauften Gadgets verbrauchen.

Würfe verwenden serverseitige Flugbewegung, Gravitation und Kollision mit Abprall;
Actorbewegung und Aktivierungszeit werden repliziert. Die Startgeschwindigkeit
beträgt 1150 cm/s in Blickrichtung plus 180 cm/s nach oben. Die Zündzeit beginnt
beim Spawn. Der Rundenwechsel entfernt aktive und fliegende Gadgets und löscht
vorübergehende Blend-, EMP- und Sensorzustände. Unverbrauchte Gadgets überlebender
Spieler bleiben im Inventar, verbrauchte werden nicht kostenlos ersetzt.

## Wirkungsprofile

| Gadget | Preis | Zündzeit | Reichweite/Radius | Wirkung |
|---|---:|---:|---:|---|
| Flash | 200 CR | 1,6 s | 1800 cm | Bis zu 4 s Blendwirkung, abhängig von Distanz und Blickrichtung |
| Smoke | 300 CR | 1,8 s | 450 cm | 18 s Sichtverdeckung |
| Frag | 350 CR | 2,0 s | 650 cm | Maximal 110 Schaden, linearer Abfall bis 0 am Rand |
| EMP | 350 CR | 1,5 s | 1000 cm | 8 s Störung elektronischer Funktionen |
| Sensor | 250 CR | 1,0 s | 1000 cm | 15 s Einsatzdauer, eine Abfrage pro Sekunde |

**Flash:** Eine feste Sichtblockade oder aktiver Rauch zwischen Detonation und
Augen verhindert die Wirkung. Distanz reduziert die Stärke linear; Wegschauen
reduziert den Blickfaktor bis auf 15 %. Die tatsächliche Dauer beträgt vier
Sekunden mal Stärke. Das lokale Bild blendet aus, Bots unter wirksamer Blendung
stoppen Feuer und Zielverfolgung. Eigenes Team und Werfer können ebenfalls
geblendet werden. Flash verursacht keinen HP-Schaden.

**Smoke:** Der visuelle Effekt verdeckt die Sicht menschlicher Spieler. Dieselbe
aktive Rauchkugel blockiert die Sichtabfrage von Bots, Flash und Sensoren. Ihr
Mittelpunkt liegt 140 cm über dem gelandeten Actor. Rauch ist keine feste Wand:
Bewegung, Kugeln und Explosionsdruck werden dadurch nicht blockiert.
Trennwände begrenzen auch die Gameplay-Sichtprüfung: Eine Rauchkugel im
Nachbarraum verdeckt dort nicht allein durch ihren Radius eine freie Sichtlinie.

**Frag:** Feste Wände verhindern Schaden. Verbündete sind gegen Frag-Schaden
geschützt, der Werfer kann sich selbst verletzen. Der explizite Schadenstyp
`Explosion` hat keinen Kopftreffermultiplikator. Rüstung absorbiert dieselben
25 % beziehungsweise 40 % wie bei Körpertreffern, begrenzt durch verbleibende
Rüstungspunkte. Beispiel: Bei 325 cm Abstand entstehen 55 Rohschaden; volle
schwere Rüstung verliert 22 Punkte, die HP sinken um 33. Bestehende serverseitige
Kill-/Assistregeln verwenden den tatsächlichen HP-Verlust. Selbst- und
Teamverletzungen erzeugen keine Credits.

**EMP:** Feste Sichtblockaden verhindern die Störung. Betroffene Spieler können
währenddessen weder SHIFT bedienen noch ARCs Ferninteraktion auslösen;
Sensoranzeigen werden gelöscht und bis Ablauf nicht erneuert. Getroffene
ausgelegte Sensoren pausieren. Deren ursprüngliche 15-sekündige Einsatzdauer
läuft weiter. EMP trifft auch eigene Elektronik.

Getroffene SHIFT-Türen und Fahrstühle halten am aktuellen Ort; Bedienung und
laufende Hacks werden unterbrochen. Ein getroffenes Hackterminal unterbricht
vorübergehend die zugeordneten Circuit-Lichter. Der gespeicherte Strom- und
Schaltzustand wird nicht überschrieben: Nach Ablauf wirkt der vorherige Zustand
wieder. EMP stellt einen bereits ausgeschalteten Circuit nicht an und schaltet
einen zuvor eingeschalteten Circuit nicht dauerhaft ab. GRID V3 enthält weiterhin
SHIFT-Szenenmarkierungen; funktionierende Geräte sind in den dafür vorgesehenen
Geräte-Fixtures und Karten vorhanden.

**Sensor:** Meldet lebende Gegner innerhalb der Reichweite nur bei freier Sicht
ohne Rauch. Positionen gehen ausschließlich an die eigenen lebenden Teammitglieder;
jeder erhält sie über seine nur für den Besitzer replizierte Komponente. Gegner
erhalten diese Daten nicht. Eine Markierung gilt höchstens 1,25 Sekunden und wird
bei erneuter Erkennung aktualisiert. Es gibt keine Ortung durch Wände und keine
Credit-Belohnung allein für Erkennung, Blendung oder EMP.

## Prüfpfad und Grenzen

Die folgenden frischen Nachweise wurden am 4. November 2026 geprüft. Beide Runner
und sämtliche von ihnen gestarteten Unreal-Prozesse endeten erfolgreich.

| Prüfung | Ergebnis | Nachweis |
|---|---|---|
| Gadget-Automation | Sechs Gruppen erfolgreich | [GadgetAutomation.json](Validation/GadgetsWeaponsAim/GadgetAutomation.json) |
| Gesamte Vanthex-Automation im abschließenden Build | Alle 24 Gruppen erfolgreich, keine Fehler oder Warnungen | [FullAutomation.json](Validation/GadgetsWeaponsAim/FullAutomation.json), [Konsole](Validation/GadgetsWeaponsAim/FullAutomationConsole.txt) |
| Wirkungen, Raumgrenzen, Inventar und Ducken | 58 Prüfungen, keine Fehler; 24,81 s Weltzeit | [GadgetIntegration.txt](Validation/GadgetsWeaponsAim/GadgetIntegration.txt) |
| Echter Host und Client | Beide bestanden, einschließlich privater Sensor-/Flashdaten und Bot-Rückgabe | [Host](Validation/GadgetsWeaponsAim/GadgetNetworkHost.txt), [Client](Validation/GadgetsWeaponsAim/GadgetNetworkClient.txt) |
| Runner-Abschluss | Beide Läufe erfolgreich beendet | [Gameplay](Validation/GadgetsWeaponsAim/GadgetChecksConsole.txt), [Netzwerk](Validation/GadgetsWeaponsAim/GadgetNetworkConsole.txt) |
| Economy-Regression | 45 Prüfungen, keine Fehler | [EconomyIntegration.txt](Validation/GadgetsWeaponsAim/EconomyIntegration.txt) |
| Echte Core-Flüge und Belohnungen | 32 Prüfungen, keine Fehler | [CoreEconomy.txt](Validation/GadgetsWeaponsAim/CoreEconomy.txt) |
| Waffenprofile und endliche Munition | 24 Prüfungen, keine Fehler | [WeaponProfiles.txt](Validation/GadgetsWeaponsAim/WeaponProfiles.txt) |

`Run-Gadget-Checks.ps1` führt sechs Automationgruppen unter `Vanthex.Gadgets` sowie
den opt-in Weltlauf `-GadgetTest` aus. Automation prüft vollständige Profile,
normalisierte Blendstärke, linearen Explosionsabfall, ungültige Zahlen, begrenzte
Sichtsegmente durch Rauch und die Assetdaten der Darstellung. Der Weltlauf
schreibt `Saved/GadgetIntegration.txt` und
hat eine Grenze von 45 Sekunden Weltzeit. Der Runner erlaubt pro Prozess maximal
180 Sekunden einschließlich Start und beendet ausschließlich selbst gestartete
versteckte Prozesse.

Der Weltlauf verwendet echte Käufe, Würfe, Actor-Ticks, Physikkollision, Detonation,
Schadenskomponenten und Rundenübergänge. Kontrollierte Hindernisse isolieren
Sicht-, Reichweiten- und Rüstungseffekte. Er prüft außerdem serverseitigen
Gadgetverbrauch und Replay, Sensor-Teamfilter, Rauch und Wandverdeckung,
EMP-Erholung sowie ablaufende Wirkungen. Er ersetzt keine visuelle Abnahme des Rauchs.

`Run-Gadget-Network-Checks.ps1` ist der getrennte Mehrspieler-Prüfpfad. Er startet
Host und Client auf Port 17785 mit acht Sekunden Abstand und schreibt
`Saved/GadgetNetworkHost.txt` und `Saved/GadgetNetworkClient.txt`. Der Client kauft
Sensor und Smoke über echte Kauf-RPCs, wirft über seine eigene replizierte
Komponente und sendet absichtlich doppelte sowie veraltete Requests. Der Test
prüft 250 CR Restguthaben, Verbrauch genau einmal, Actor-Replikation, echte
Sensorabfragen und nur an den Besitzer gelieferte Markierungen. Eine echte
Flash-Detonation trifft Host und Client; die fremden Blendzustände dürfen auf dem
Client nicht ankommen. Beide Welten erhalten denselben Diagnoseboden für
clientgesteuertes Ducken und Aufstehen. Der Host beendet erst nach dem Client
und prüft die Rückgabe des unveränderten Rosterinventars an den Bot. Die Grenzen
betragen 90 Sekunden Weltzeit und insgesamt 180 Sekunden mit Prozessstart.

Zusätzlich prüft derselbe Lauf das Ducken über `SetCrouching` und echte
CharacterMovement-Ticks: Capsule-Halbhöhe 60 statt 96 cm, Geschwindigkeit
280 cm/s, Kamerahöhe 42 statt 64 cm, verhindertes Aufstehen unter einer niedrigen
Decke und Rückkehr zur stehenden Form in freiem Raum. Der Netzwerkpfad prüft
zusätzlich Capsule und Kamera auf Client und Server; Darstellung muss gesondert
abgenommen werden. Aus diesen technischen Checks folgt keine
Abnahme von Balance, Lesbarkeit unter allen Beleuchtungen oder Produktionsanimationen.
