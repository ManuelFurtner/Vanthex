VANTHEX - Extraktion mit losem Core, 4. November 2026

Der Core startet bzw. behaelt die 15-Sekunden-Extraktion innerhalb der Zone auch im Flug oder am Boden. Entscheidend ist seine Position. Ablegen, Hineinwerfen, gleichteaminternes Aufnehmen und der Tod des Traegers erhalten den Fortschritt, sofern der Core in derselben Zone bleibt. Das letzte besitzende Team erhaelt den Sieg. Verlassen, Zonenwechsel oder gegnerischer Besitz setzen den Fortschritt zurueck. Reset/Recovery loeschen die Teamzuordnung; ein unbeanspruchter neutraler Core kann nicht extrahieren. Die bestehenden Rundenende-/Eliminierungsregeln gelten weiterhin.

Die Bewegungszustaende Thrown/AtRest bleiben unabhaengig vom Extraktionsprozess bestehen, damit Flug, Landung, Fang und Aufnahme weiterhin funktionieren. Der replizierte GameState stellt den Countdown auch ohne Traeger dar; LastPossessingTeam wird ebenfalls repliziert. Eine serverseitige Besitzrevision verhindert die Uebernahme alten Fortschritts nach zwischenzeitlichem Gegnerbesitz.

Pruefung:
- Editor und Game Win64 Development: Build erfolgreich.
- 59 gezielte Integrationspruefungen: Alpha/Bravo an A/B, reale Drop-/Wurfbewegung bis zur Landung, Fortschritt beim Ablegen und gleichteaminternen Aufnehmen, Pickup trotz laufender Extraktion, Zonenwechsel, Verlassen, Gegneruebernahme, neutraler Reset und Tod des Traegers. Alle bestanden.
- Die vier gezielten Abschlussfaelle speisen kontrollierte Zeitintervalle in die echte Extraktionslogik ein und pruefen vorzeitigen/korrekten Abschluss. Der zusaetzliche Todesfall laeuft mit dem normalen Runden-Timer durch die verbleibenden zwoelf Sekunden.
- 64 allgemeine Gameplay-Selbsttests bestanden; die bisherigen Abbrucherwartungen fuer Drop/Tod wurden auf Fortsetzung umgestellt.
- Zwei echte Netzwerkprozesse: Host und Client sehen gehaltenen und anschliessend losen Core, fortlaufenden abnehmenden Countdown ohne Neustart, korrekte Teamzuordnung und denselben Extraktionssieg. Der reale 15-Sekunden-Runden-Timer wird benutzt.

Reproduktion:
UnrealEditor-Cmd.exe Vanthex.uproject /Game/Vanthex/Maps/GridV4 -game -nullrhi -nosound -unattended -LooseCoreExtractionTest
Netzwerk: Listen-Host mit /Game/Vanthex/Maps/GridV4?listen -port=17864 -game -nullrhi -LooseCoreNetworkTest, danach separater Client mit 127.0.0.1:17864 und denselben Testschaltern.
Allgemeiner Selbsttest: /Engine/Maps/Entry -game -nullrhi -VanthexSelfTest.

Kein GitHub-Upload in diesem Arbeitsgang. Bestehende Projektarbeiten wurden erhalten.
