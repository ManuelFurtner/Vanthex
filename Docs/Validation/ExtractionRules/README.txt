VANTHEX: Messerbonus, Bot-Core-Wuerfe und Extraktion nach Eliminierung
Validiert am 04.11.2026 mit Unreal Engine 5.8.3, bestehendem Projekt H:\Vanthex.

Ergebnis:
- Editor Development und Game Development erfolgreich kompiliert.
- ExtractionRules: 110/110 PASS. Beide Teams, beide Zonen, eigenes/gegnerisches/beide Teams ausgeschaltet; 15 Sekunden, abgelaufene Rundenzeit, gegnerische Wiederaufnahme, Reset, normale Eliminierung, Messerwechsel und Tempo.
- Echte Bot-Ticks in GridV4 werfen und landen den Core bei beiden Teams an beiden Zonen. Eigene Ablage bleibt liegen; gegnerischer Bot nimmt sie auf und unterbricht den bisherigen Countdown.
- LooseCoreExtraction: 59/59 PASS, bestehende Drop-/Wurf-/Pickup-/Todesfallregression.
- VanthexSelfTest: 64/64 PASS, bestehende allgemeine Gameplay-Regressionspruefungen.
- Zwei getrennte Netzwerkprozesse: Host und Client PASS. Client-waehlt-Messer-RPC, 690 cm/s auf beiden Seiten, replizierte Core-Teamzuordnung, monotoner Countdown, alle sechs Spieler tot, abschliessender Extraktionssieg.
- Keine Fatal-/Assertion-/Ensure-/Error-Meldungen in den fuenf Testlogs. git diff --check ohne Fehler.

Aenderungen:
- Vollstaendig gezogenes Messer: Gehen 600 -> 690, Sprint 900 -> 1035 cm/s; Ducken bleibt 280. Kein Bonus waehrend des Waffenwechsels.
- Bots pruefen einen kurzen Wurf samt Kollisionsbahn und Landungsabstand zum Zonenrand, drehen sich zum Ziel und werfen. Unsichere Wuerfe werden nicht abgegeben; sie gehen naeher heran. Freundliche Ablagen werden verteidigt; gegnerische Ablagen priorisiert aufgenommen.
- Abgelegter, einem Team zugeordneter Core in der Zone verschiebt Eliminierung und Timeout. Der normale Rundentimer verarbeitet weiter die 15-Sekunden-Extraktion. Gegnerische Aufnahme oder Verlassen/Reset der Zone hebt den Aufschub auf.

Reproduzieren (UnrealEditor-Cmd.exe, Projektargument H:\Vanthex\Vanthex.uproject):
/Game/Vanthex/Maps/GridV4 -game -nullrhi -nosound -unattended -ExtractionRulesTest
/Game/Vanthex/Maps/GridV4 -game -nullrhi -nosound -unattended -LooseCoreExtractionTest
/Engine/Maps/Entry -game -nullrhi -nosound -unattended -VanthexSelfTest
Netzwerkhost: /Game/Vanthex/Maps/GridV4?listen -port=17864 -game -nullrhi -nosound -unattended -LooseCoreNetworkTest -EliminationNetworkTest
Netzwerkclient: 127.0.0.1:17864 -game -nullrhi -nosound -unattended -LooseCoreNetworkTest -EliminationNetworkTest

Die Pruefungen laufen ohne Grafikrenderer, aber mit Spielwelt, Kollision, Core-Physik, Controller-Ticks und Netzwerkreplikation. Kein neuer visueller Review und kein Test ueber eine WAN-Verbindung. Die Bot-Wurf-Fixtures beginnen in Zonennähe; keine vollstaendige autonome Match-Balance-Pruefung.
Keine Map-Neugenerierung, kein Commit und kein GitHub-Push in diesem Auftrag.
