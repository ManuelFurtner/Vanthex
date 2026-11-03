# Wirtschaft, Rüstung und Waffen-Audio

Stand: **3. November 2026**. Wirtschaft, Rüstung, Waffenprofile und der zugehörige
Host-/Client-Ablauf sind implementiert und in den unten aufgeführten Läufen geprüft.
Editor und Game bauen erfolgreich mit UE 5.8.3; 119 Audioquellen sind importiert.
Der Validierungsabschnitt nennt die vorliegenden Berichte unter
[Validation/EconomyArmorAudio](Validation/EconomyArmorAudio). Netzwerk- und
Mixermessungen ersetzen keine menschliche Hörabnahme.

## Spielbarer Umfang

Jedes Match beginnt mit Standardpistole, Messer und 800 CR. Jede Runde hat eine
25-sekündige Kaufphase vor der 180-sekündigen aktiven Phase. B bedient das Kaufmenü;
1, 2 und 3 wählen Primärwaffe, Pistole und Messer. Bewegung, Schüsse, Messer-, Core-
und SHIFT-Aktionen sind während der Kaufphase gesperrt. Blicksteuerung bleibt nach
Schließen des Kaufmenüs verfügbar.

Der Server führt Käufe atomar aus: Phase, lebender eigener Pawn, Team, ID, Preis,
Credits und Inventargrenzen werden geprüft. Monotone Request-IDs verhindern erneute
Abbuchungen durch doppelte oder veraltete Kaufanforderungen. Kontostand und Inventar
liegen im PlayerState und werden ausschließlich an dessen Besitzer repliziert.
Join und Disconnect übertragen den vorhandenen Rosterstand zwischen Mensch und Bot;
sie erzeugen kein neues Startguthaben. Teamwechsel im laufenden Match sind gesperrt.

Das Kaufmenü prüft eine lokale Inventarkopie und deaktiviert bereits erkennbare
ungültige Käufe. Ein Tooltip erklärt die Sperre. Die endgültige Entscheidung trifft
weiterhin der Server; konkrete Ablehnungsgründe werden zuverlässig nur an den Käufer
übermittelt und im Menü angezeigt.

Überlebende behalten ihre Primärwaffe, Pistole, verbleibende Rüstung und Gadgets.
Zum Rundenstart werden HP, Magazine und endliche Reserve aufgefüllt. Innerhalb der
Runde bleiben Munition und Nachladeverbrauch pro Waffenplatz erhalten. Tod entfernt
gekaufte Ausrüstung, erhält aber Credits; zur nächsten Runde stehen die kostenlose
Standardpistole und das Messer bereit. Ein neues Match setzt den Stand zurück.

Die im [Regelentwurf](ECONOMY_AND_WEAPONS.md) festgelegten Preise und Belohnungen
gelten unverändert. Rundenabrechnung erfolgt einmal je Runde, einschließlich
Niederlagenserie und zusätzlicher Extraction-Prämie. Kill/Assist verwenden tatsächlichen
HP-Verlust vor dem Todesereignis, ohne absorbierten Schaden oder Overkill. Core-Credits
folgen bestätigten Fängen, Routenfortschritt, Wurf-ID und den festgelegten Spieler-,
Paar- und Teamgrenzen. Reset oder externe Versetzung eines fliegenden Cores entziehen
dem Wurf die Bonusberechtigung. Dynamische Türblockaden werden bei der für Credits
verwendeten Routenlänge berücksichtigt.

Leichte Rüstung hat 40 Punkte und 25 % Absorption, schwere 80 Punkte und 40 %.
Nur explizit als Projektil markierte Körpertreffer verbrauchen Rüstung. Kopf,
Messer und Umgebungsschaden umgehen sie. Bruchteile bleiben in der Rechnung erhalten.
Vollständige Rüstung desselben Typs kann nicht erneut berechnet werden; beschädigte
Rüstung wird zum vollen Preis ersetzt. Leicht → schwer kostet den vollen Preis,
schwer → leicht wird abgelehnt.

## Waffen und noch fehlende Ausstattung

Neun getrennte Startprofile sind vorhanden: **Standardpistole, P250, MP5, MP7, M4,
AK, AUG, DMR und Shotgun**. Sie definieren Feuerart, Schaden, Reichweitenabfall,
Rückstoßfolge, Bewegungsstreuung, Magazin, endliche Reserve und Nachladezeiten.
Die Shotgun verarbeitet mehrere Projektile je Betätigung. Standardpistole, AUG
und MP7 bleiben die ersten drei Prioritäten für weitere Ausarbeitung und Playtests.
Die Werte sind Startparameter; daraus folgt keine nachgewiesene Waffenbalance.

Die vorhandene ARC-Handführung und der Waffenwechsel verwenden die tatsächlichen
Equipment-Zustände. Waffenformen sind weiterhin Prototypdarstellungen. Separate
Ego-Arme, ausgearbeitete Nachladeanimationen, endgültige Waffenmodelle und weitere
Operatoren bleiben offen.

**Flash, Smoke, Frag, EMP und Sensor haben Preise und kaufbare Inventarplätze;
ihre Anwendung und Wirkung fehlen.** Zwei Plätze sind erlaubt, auch zweimal
derselbe Typ. Es gibt keine behauptete Blendwirkung, Rauchsimulation, Explosion,
EMP-Störung oder Sensorerkennung. Kostenlose Operatorfähigkeiten belegen diese
Plätze nicht. Die Wirkungsprofile müssen vor ihrer Implementierung festgelegt werden.

## Waffen-Audio

`AudioSource/Weapons` enthält **119 originale prozedural erzeugte Mono-WAVs**
(48 kHz PCM16) samt Generator und Manifest. Es handelt sich um entworfene
Spielklänge, nicht um authentische Waffenaufnahmen. Der Import-Commandlet
`VanthexBuildWeaponAudio` erzeugt die Unreal-Profile und ihre Audioassets.

Schüsse, Mechanik, einzelne Nachladephasen, Abbruch, Leerschuss, Holster/Draw und
Messeraktionen folgen bestätigten Gameplay-Ereignissen. Kaufantworten und
Phasenhinweise bleiben lokal. Ereignis-IDs, begrenzte Stimmenzahl und Abbruchlogik
verhindern unkontrollierte Doppelwiedergabe und nachlaufende Reload-Phasen.
Vorbeiflug und Materialeinschlag bleiben beim bestehenden Bullet-Audio-System.

Die GRID-Ausbreitung nutzt Korridor-/Treppenverbindungen, aktuelle Sichtlinien und
Türblockaden, Weglänge, Tiefpassfilter und kurze Reflexionen. Eine leise gedämpfte
Transmission dient als Ersatz bei fehlendem offenen Weg. Das Portal-/Raummodell
ist eine begrenzte Näherung und keine physikalische Schallwellensimulation. Stereo-
Panning und korrekte Quellhöhe belegen keine HRTF- oder Höhenortungsqualität.
Produktionsmix, menschliche Hörtests und Belastung unter Paketverlust bleiben offen.

## Vorliegende Validierung

Die folgenden Berichte wurden am 3. November 2026 gelesen. Alle genannten Läufe
melden Erfolg und enthalten keine fehlgeschlagenen Einzelprüfungen. Die verlinkten
Kopien liegen unter `Docs/Validation/EconomyArmorAudio`; neue lokale Läufe schreiben
ihre Berichte weiterhin nach `Saved`.

| Bereich | Belegter Lauf | Bericht |
|---|---|---|
| Editor- und Game-Build, UE 5.8.3 | Beide erfolgreich | [BuildAndImport.txt](Validation/EconomyArmorAudio/BuildAndImport.txt) |
| Audioimport | 119 WAVs importiert, keine Fehler oder Warnungen | [BuildAndImport.txt](Validation/EconomyArmorAudio/BuildAndImport.txt) |
| Automation | 14 Gruppen bestanden, keine Warnungen/Fehler/ausgelassenen Tests | [Automation.json](Validation/EconomyArmorAudio/Automation.json) |
| Economy-/Rüstungsintegration | 45 Prüfungen bestanden, einschließlich exakt acht Sekunden Assist-Alter | [EconomyIntegration.txt](Validation/EconomyArmorAudio/EconomyIntegration.txt) |
| Echte Core-Flüge und Auszahlungen | 32 Prüfungen bestanden | [CoreEconomy.txt](Validation/EconomyArmorAudio/CoreEconomy.txt) |
| Waffenprofile, Magazin/Reserve und Wechsel | 24 Prüfungen bestanden | [WeaponProfiles.txt](Validation/EconomyArmorAudio/WeaponProfiles.txt) |
| Economy Host/Client | Beide Rollen bestanden, einschließlich Bot-Rückgabe | [Host](Validation/EconomyArmorAudio/EconomyNetworkHost.txt), [Client](Validation/EconomyArmorAudio/EconomyNetworkClient.txt) |
| Echter Audio-Mixer | 13 PASS-Einträge, sieben gerenderte Ereignisse | [WeaponAudioPlayback.txt](Validation/EconomyArmorAudio/WeaponAudioPlayback.txt) |
| Mixer-Wellenform | Stereo, 48 kHz, Peak −14,85 dBFS, keine geclippten Samples | [Analyse](Validation/EconomyArmorAudio/WeaponAudioPlayback-analysis.json), [WAV](Validation/EconomyArmorAudio/WeaponAudioPlayback.wav) |
| Kaufmenü und Ego-Waffenansichten | Vier frische Renderbilder geprüft | [EconomyShots](Validation/EconomyArmorAudio/EconomyShots), [Capture-Protokoll](Validation/EconomyArmorAudio/EconomyShots/capture.txt) |
| ARC-Regression | 34 Prüfungen bestanden | [OperatorValidation.txt](Validation/EconomyArmorAudio/OperatorValidation.txt) |
| Gameplay-Selbsttest | 64 Prüfungen bestanden | [VanthexSelfTest.txt](Validation/EconomyArmorAudio/VanthexSelfTest.txt) |
| GRID-Geometrie/Navigation | Bestanden; Berichtskopf 150 Checks, insgesamt 159 PASS-Einträge einschließlich weiterer Routenprüfungen | [GridV3Validation.txt](Validation/EconomyArmorAudio/GridV3Validation.txt) |
| GRID-Gameplay | 34 Prüfungen bestanden | [GridV3Gameplay.txt](Validation/EconomyArmorAudio/GridV3Gameplay.txt) |
| GRID Host/Client | Beide Rollen bestanden | [Host](Validation/EconomyArmorAudio/GridV3NetworkHost.txt), [Client](Validation/EconomyArmorAudio/GridV3NetworkClient.txt) |

Der Economy-Netzwerklauf bestätigt die Übernahme von 1.577 CR aus einem Bot-Slot,
serverseitige Käufe bis auf 827 CR, P250 und 40 Rüstungspunkte, die Ablehnung alter
Kauf-IDs und dass fremde Kontodetails nicht an den Client gelangen. Beide Rollen beobachten
412,7 cm echte Bewegung nach der gesperrten Kaufphase und genau einen Schuss; der
gesonderte bestätigte Schusszähler steigt auf beiden Rollen von 0 auf 1. Beim
Disconnect übernimmt ein Bot denselben Pawn samt unverändertem Konto und Inventar.
Der Lauf umfasst die Korrekturen an
Unreals Remote-Restart und der Übergabe vor `PawnLeavingGame`.

Der Core-Lauf prüft echte Welt-Ticks und Fänge: +50 für einen qualifizierten Pass,
+100 für einen Intercept, keine doppelte Auszahlung und keine Prämie bei Eigenfang,
Bodenpickup, Todes-Drop, Reset oder einer externen Core-Verschiebung um 10 cm.
Die Waffenprüfung enthält endliche Reserve, Nachladeabbruch ohne Munitionsgewinn,
getrennte Magazine beim Waffenwechsel und tatsächliche Shotgun-Treffer.

Der abschließende Economy-Lauf bestätigt auch den Grenzfall bei exakt acht Sekunden:
Der Assist bleibt gültig; ältere Schadensereignisse werden ausgeschlossen. Die
Mixermessung bestätigt Ereigniswiedergabe, Links/Rechts-Verteilung, begrenzten Pegel
und fehlendes Clipping in der aufgezeichneten Sequenz. Oben/Unten-Ereignisse sind
gerendert, ihre Stereoenergie ist jedoch kein HRTF- oder Höhenortungsnachweis.

Die Renderprüfung bei 1280 × 720 zeigt ein vollständig lesbares Kaufmenü einschließlich
Preisen und Schließen-Schaltfläche, den Start mit 800 CR sowie AUG und schwere Rüstung
mit 3.350 CR/80 Punkten. Die aktiven Ego-Ansichten zeigen AUG mit 30/90 Munition und
MP7 mit 40/160 bei 1.450 CR. Diese Aufnahmen belegen den aktuellen Prototyp; sie sind
keine Abnahme von Produktionsgrafik. Waffenbalance und menschliche Hörabnahme bleiben offen.

## Prüfungen ausführen

Zuerst den aktuellen Editor und das Game bauen; laufende Editorinstanzen vorher
speichern und schließen. Die folgenden Skripte starten die Diagnosen, sie ersetzen
keinen C++-Build:

```powershell
.\Run-Economy-Checks.ps1 -EngineRoot 'H:\epic\UE_5.8'
.\Run-Economy-Network-Checks.ps1 -EngineRoot 'H:\epic\UE_5.8'
```

`Run-Economy-Checks.ps1` führt die Vanthex-Automation sowie die Diagnosen
`-EconomyTest` und `-CoreEconomyTest` aus. Die erste prüft Käufe, HP/Rüstung,
Belohnungen und Lebenszyklus. Die zweite benutzt echte Core-Flüge und World-Ticks
für Pass, Eigenfang, Bodenpickup, Todes-Drop, Teleport, Reset und Intercept.
Berichte: `Saved/EconomyIntegration.txt`, `Saved/CoreEconomy.txt` und ein eigener
Automation-Exportordner.

`Run-Economy-Network-Checks.ps1` startet getrennte Host-/Client-Prozesse auf Port
17784. Es prüft echte Kauf-RPCs und Replay-Schutz, die Übernahme eines vorbefüllten
Botkontos, private Economy-Replikation, Kaufphasensperren, Bewegung und einen
bestätigten Schuss samt Audio-Paketen. Nach Client-Disconnect muss derselbe Pawn
mit unverändertem Konto an einen Bot zurückgehen. Berichte:
`Saved/EconomyNetworkHost.txt` und `Saved/EconomyNetworkClient.txt`.

Zusätzliche Diagnosen auf `/Game/Vanthex/Maps/GridV3`:

| Flag | Zweck |
|---|---|
| `-WeaponProfileTest` | Reale Waffen-/Munitions-/Nachladetimer und Waffenwechsel mit den neuen Profilen |
| `-WeaponAudioPlaybackTest` | Wiedergabe über den echten Unreal-Mixer, Aufnahme, Duplikat-/Abbruch- und Stimmenbegrenzungsprüfungen |

Den Mixer-Test mit Audioausgabe starten, also ohne `-nosound`; Grafikdiagnosen
benötigen entsprechend eine aktivierte Darstellung. Die Audioaufnahme
`Saved/WeaponAudioPlayback.wav` wird anschließend mit
`AudioSource/Weapons/analyze_playback.py` untersucht. Aus einer erfolgreichen
Links/Rechts-Messung darf kein Höhenortungsnachweis abgeleitet werden.

## Bewusst erhaltene Diagnose-Fixture

`AVanthexGameMode::UsesLegacyCombatFixture()` wählt für bestehende Kampf-, GRID-,
Operator-, Equipment- und Bullet-Audio-Diagnoseflags ausdrücklich `LegacyRifle`:
40 Körperschaden, ursprüngliches Zeitverhalten und keine neue Kaufphase. Dieses
Profil steht nicht im Kaufkatalog und wird in normalen Matches nicht ausgewählt.
So prüfen vorhandene Regressionen weiterhin dieselbe historische Ausgangslage.

Die neuen Economy-, Core-Economy-, Economy-Netzwerk- und Waffenprofiltests verwenden
den normalen Wirtschafts-/Profilpfad. Bestehende erfolgreiche Legacy-Berichte allein
sind deshalb kein Nachweis für die neue Kaufphase, Rüstung oder Waffenbalance.
Die oben aufgeführten neuen Berichte ergänzen diese Legacy-Regressionen; neue
Detailprüfungen müssen weiterhin separat ausgeführt und bewertet werden.
