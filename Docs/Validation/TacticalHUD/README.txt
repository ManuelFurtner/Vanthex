VANTHEX - Taktisches HUD und Steuerung, 4. November 2026

Umgesetzt:
- Tab halten: Scoreboard mit beiden Teams, Spielername, Kills, Assists, Toden, Ping/Bot und Lebensstatus. Eigene Zeile hervorgehoben. Tab funktioniert auch aus dem fokussierten Kaufmenue; Loslassen schliesst die Anzeige. Ein lokaler Slate-Eingabeprozessor sichert die Tastenfreigabe beim Fokuswechsel und wird beim Verlassen des Controllers entfernt.
- K/A/D werden auf dem Server aus echten Schadens-/Kill-Ereignissen erfasst und oeffentlich repliziert. Rundenwechsel erhalten die Werte, Match-Neustart setzt sie zurueck. Bot-/Spieleruebernahme kopiert die Statistik zusammen mit dem bestehenden Konto. Private Ausruestungs-/Kontodaten bleiben privat.
- Springen ist sowohl als Eingabe entfernt als auch durch CanJumpInternal server-/clientseitig gesperrt. Treppen, Laufen, Sprint und Ducken bleiben erhalten.
- Kaufzeit: 12 Sekunden. Waffenwahl ueber 1/2/3 und Mausrad ist waehrend der Vorbereitung erlaubt; Angriffe, Core-Interaktion und Bewegung bleiben gesperrt. Die gewaehlte Waffe bleibt beim Rundenstart erhalten.
- Mausrad abwaerts: Primaer -> Pistole -> Messer -> Primaer; aufwaerts umgekehrt. Fehlende Slots werden uebersprungen. Auswahl und Richtung werden serverseitig validiert; bestehende Wechselzeiten gelten weiterhin. Im offenen Kaufmenue scrollt das Mausrad die Einkaufsliste; B schliesst es fuer den Waffenwechsel.
- Minimap auf GRID V3/V4: Grundriss der aktuellen Ebene, Blickrichtung, lebende Teammitglieder derselben Ebene, Treppen und Extraktion A/B auf ihrer Ebene. Keine Gegner, keine Sensor-Marken, keine Verfolgung eines feindlichen Core-Traegers. Die Karte ist eine schematische Darstellung der begehbaren Raum-/Korridortopologie, kein Bild saemtlicher Dekoration oder Deckung.

Ergebnisse:
- Editor und Game Win64 Development: erfolgreich gebaut.
- 20 Steuerungs-/HUD-Integrationspruefungen bestanden, einschliesslich echter Slate-Tab-Tastendruecke und -freigaben aus dem Kaufmenue, realer 12-Sekunden-Countdown, Waffenwechsel in beide Richtungen, fehlender Primaerslot, gesperrte Angriffe, gesperrtes Springen und K/A/D bei echten Schadensereignissen.
- 26 Vanthex-Automation-Tests bestanden, darunter Minimap-Filter fuer alle Team-/Lebensstatus-/Selbst-Kombinationen und die vorige Regression fuer mehrere Kills hintereinander.
- 64 allgemeine Gameplay-Pruefungen bestanden.
- 45 Wirtschafts-Integrationspruefungen bestanden.
- Getrennter Host und Client: Kauf, Datenschutz, Bewegungssperre, 12 Sekunden Kaufzeit, Mausradwechsel zu Messer/zurueck zur Pistole innerhalb der Kaufphase, Schuss/Bewegung nach Freigabe und Bot-Rueckuebernahme bestanden. Der Gesamterfolg im Netzwerkbericht setzt beide beobachteten Waffenwechsel voraus.
- 41 GRID-V4-Gameplay-Pruefungen bestanden: Core-Interaktionen, vier 15-Sekunden-Extraktionen, zwoelf Treppenlaeufe und geschlossene Grenzen.
- Sieben echte gerenderte HUD-Zustaende erfasst; Minimap und Scoreboard auf 1280 x 720 visuell kontrolliert. Beispielbilder beiliegend.

Die frueheren zwei nach unten gerichteten Sprungwurf-Fixtures wurden an die neue Regel angepasst: Die stehenden Wuerfe werden korrekt von den Gelaendern gestoppt. Die Tests pruefen dies jetzt explizit; fuer den Ebenenwechsel bleiben die normalen unteren/oberen Laufwege. Die Karte selbst wurde hierbei nicht umgebaut. Die allgemeinen/Netzwerk-/Kartenpruefungen erfolgten vor der letzten ausschliesslich lokalen Tab-Freigabekorrektur; danach wurden beide Builds und alle 20 Steuerungspruefungen erneut erfolgreich ausgefuehrt.

Reproduktion:
UnrealEditor-Cmd.exe Vanthex.uproject /Game/Vanthex/Maps/GridV4 -game -nullrhi -nosound -unattended -ControlsTest
UnrealEditor-Cmd.exe Vanthex.uproject /Engine/Maps/Entry -nullrhi -unattended -ExecCmds="Automation RunTests Vanthex." -TestExit="Automation Test Queue Empty"
Weitere Schalter: -VanthexSelfTest auf /Engine/Maps/Entry; -EconomyTest und -GridV4GameTest auf GridV4; -EconomyNetworkTest auf Listen-Host und separatem Client.
Renderansichten: -HUDShots -game -RenderOffscreen -ResX=1280 -ResY=720 auf GridV4.

Kein GitHub-Upload in diesem Arbeitsgang. Fab-Assets bleiben lokal. Vorhandene, noch nicht eingecheckte Projekt-/Kartenarbeiten wurden erhalten.
