# ARC und Gewehr/Messer: Zusammenführung

Geprüft am 3. November 2026 mit Unreal Engine 5.8.3 unter Windows.
Der Merge verbindet ARC auf `ae077c4` mit `codex/knife-weapon-switch`
auf `0b83095`. Die ergänzende Integration synchronisiert die Waffenansicht
und ARC-Handposen; Bots wechseln nach der Übernahme einer Figur zum Gewehr.

| Nachweis | Ergebnis |
|---|---|
| `EditorBuild.txt`, `GameBuild.txt` | Beide Win64-Development-Builds erfolgreich |
| `VanthexSelfTest.txt` | 64 PASS, einschließlich 21 Ausrüstungsprüfungen |
| `OperatorValidation.txt` | 34 PASS, einschließlich Bot-Übernahme mit Messer |
| `GridV3Validation.txt` | OVERALL PASS; Berichtskopf: 150 Checks, insgesamt 159 PASS-Einträge |
| `GridV3Gameplay.txt` | 34 PASS, einschließlich Treppen in beiden Richtungen |
| `GridV3NetworkHost.txt`, `GridV3NetworkClient.txt` | Beide PASS |
| `EquipmentNetworkHost.txt`, `EquipmentNetworkClient.txt` | Beide PASS; vom Client ausgelöster Wechsel, Stich und Rückwechsel, keine doppelten Waffen |
| `RenderedViews.txt`, `RenderedViewsLog.txt` | Acht Ansichten gerendert; Messerhaltung, Stich und beide Ego-Waffen kontrolliert |

Die vier beigefügten PNGs dokumentieren die neue Ausrüstung. Die ursprünglichen
ARC-Ansichten sind zusätzlich unter `../OperatorARC` dokumentiert.
Die relativen Pfade im Renderbericht bezeichnen die lokalen Originale in `Saved`.

Die Gameplay- und Netzwerktests liefen vor der abschließenden rein optischen
Korrektur der Klingenrichtung. Danach wurden beide Builds und alle acht
Renderansichten erneut ausgeführt. Die Netzwerkprüfung verwendet zwei lokale
Prozesse; sie belegt keine Internet-Latenz oder vollständige Spielbalance.
Eigene Ego-Arme, Produktionsgrafik für das Messer und Messer-Soundeffekte bleiben
offen. Die übernommenen Wirtschafts- und Waffenklang-Dokumente sind Planung.
