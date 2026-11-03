# Wirtschaft, Rüstung und Waffen-Audio: Prüfnachweise

Ausgeführt am 3. November 2026 auf Windows mit Unreal Engine 5.8.3.
Die Berichte gehören zur Implementierung auf Basis von `e85c8d8`.
Der Commit, der dieses Verzeichnis hinzufügt, enthält den geprüften Stand.

## Neue Systeme

| Prüfung | Ergebnis | Beleg |
|---|---|---|
| Editor und Game, Development Win64 | beide Builds erfolgreich | [BuildAndImport.txt](BuildAndImport.txt) |
| Audioimport | 119 WAVs, zehn Waffenbänke; keine Importfehler | [BuildAndImport.txt](BuildAndImport.txt) |
| Vanthex Automation | 14 Gruppen erfolgreich | [Automation.json](Automation.json) |
| Wirtschaft und Rüstung | 45 Prüfungen, einschließlich exakter Assist-Grenze bei acht Sekunden | [EconomyIntegration.txt](EconomyIntegration.txt) |
| Core-Belohnungen | 32 Prüfungen mit tatsächlichen Flügen, Fängen und World-Ticks | [CoreEconomy.txt](CoreEconomy.txt) |
| Waffenprofile | 24 Prüfungen zu Feuerart, Munition, Nachladen und Wechsel | [WeaponProfiles.txt](WeaponProfiles.txt) |
| Wirtschaft im Netzwerk | zwei getrennte Prozesse, beide erfolgreich | [Host](EconomyNetworkHost.txt), [Client](EconomyNetworkClient.txt) |
| Echter Audiomixer | sieben gerenderte Ereignisse, 13 erfolgreiche Prüfzeilen | [Playback](WeaponAudioPlayback.txt), [Aufnahme](WeaponAudioPlayback.wav) |
| Wellenformanalyse | Stereo 48 kHz, Peak −14,85 dBFS, keine Clipping-Samples | [Analyse](WeaponAudioPlayback-analysis.json) |
| Darstellung | vier frische Ansichten geprüft | [Aufnahmeprotokoll](EconomyShots/capture.txt) |

Der Netzwerktest übernimmt ein Botkonto mit 1577 CR, kauft P250 und leichte Rüstung
für zusammen 750 CR, prüft 827 verbleibende CR und private Replikation. Die Kaufphase
blockiert Bewegung und Schüsse. Danach bewegt sich der Client tatsächlich um
412,7 cm und schießt mit der P250; der getrennte Schuss-Audiozähler steigt von 0 auf 1.
Beim Disconnect übernimmt ein Bot denselben Pawn und das erhaltene Konto.

Die UI-Aufnahmen zeigen [800 CR zum Start](EconomyShots/01_buy_start.png) und
[AUG mit schwerer Rüstung](EconomyShots/02_buy_aug.png). Preise und Schließen-Schaltfläche
sind bei 1280 × 720 vollständig sichtbar. [AUG](EconomyShots/03_aug_first_person.png)
und [MP7](EconomyShots/04_mp7_first_person.png) sind mit den richtigen Magazin-/Reserve-
und Kontoständen aus der eigenen Kamera gerendert. Die Waffenmodelle sind Prototypen.

## Bestehende Spielmechaniken

| Prüfung | Ergebnis | Beleg |
|---|---|---|
| Gameplay-Selbsttest | 64 Prüfungen erfolgreich | [VanthexSelfTest.txt](VanthexSelfTest.txt) |
| GRID-Geometrie | OVERALL PASS, 150 gemeldete Checks; Bericht enthält 159 PASS-Zeilen | [GridV3Validation.txt](GridV3Validation.txt) |
| GRID-Spielablauf | 34 Prüfungen erfolgreich | [GridV3Gameplay.txt](GridV3Gameplay.txt) |
| GRID-Netzwerk | Host und Client erfolgreich | [Host](GridV3NetworkHost.txt), [Client](GridV3NetworkClient.txt) |
| ARC/Operator | 34 Prüfungen erfolgreich | [OperatorValidation.txt](OperatorValidation.txt) |
| Equipment-Netzwerk | Messer, Schwung und Waffenrückkehr in beiden Prozessen erfolgreich | [Host](EquipmentNetworkHost.txt), [Client](EquipmentNetworkClient.txt) |
| Bullet-Audio-Netzwerk | drei Prozesse erfolgreich; Reichweitenfilter und fehlende Sounds geprüft | [Host](BulletAudioNetwork-Host.txt), [Client 1](BulletAudioNetwork-Client1.txt), [Client 2](BulletAudioNetwork-Client2.txt) |

Bestehende Kampfdiagnosen verwenden bewusst ihre historische `LegacyRifle`-Fixture.
Die neuen Wirtschafts- und Waffenprüfungen laufen zusätzlich mit den neuen Regeln;
Legacy-Erfolge allein werden nicht als Beweis für neue Kauf- oder Rüstungsregeln verwendet.

## Reproduktion und Grenzen

Nach einem aktuellen Editor-Build führen `Run-Economy-Checks.ps1` und
`Run-Economy-Network-Checks.ps1` die neuen Gameplay- und Netzwerkprüfungen aus.
Import und Mixeraufrufe stehen in [der Audio-Dokumentation](../../../AudioSource/Weapons/README.md).
Die tatsächlichen Schallquellen sind originale prozedurale Synthese.

Die physische Portal-/Wandprüfung ist Teil der Automation; die Mixeraufnahme prüft
die eingespeisten Richtungs- und Dämpfungswerte. Sie beweist keine zuverlässige
Front/Back- oder Höhenwahrnehmung. Menschliche Hörabnahme, endgültiger Mix,
Waffenbalance und Belastung mit Netzlatenz/Paketverlust bleiben offen.
Gadgets haben Preise und Inventarplätze; ihre Anwendungswirkungen sind noch nicht implementiert.
