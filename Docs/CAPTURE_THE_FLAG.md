# Capture the Flag / Crossroads CTF

## Spielen

Im Spielmenue unter Spielen **CAPTURE THE FLAG / 5 GEGEN 5** zweimal waehlen (Sitzungswechsel bestaetigen). Die Karte liegt unter `/Game/Vanthex/Maps/CrossroadsCTF`. Fuer LAN die Karte zuerst auswaehlen und dann LAN-Sitzung hosten; Mitspieler treten per Host-Adresse bei. Freie Plaetze werden von Bots besetzt. Der Core-Modus bleibt ueber die separate GRID-V4-Auswahl erreichbar.

## Regeln

- Fuenf Spieler pro Team, zwei Flaggen in den eigenen Basen.
- 15 Minuten aktive Spielzeit nach der Vorbereitung. Jede gegnerische Flagge, die in die eigene Basis gebracht wird, zaehlt einen Punkt. Die eigene Flagge muss dort stehen.
- Nach einem Punkt geht die eroberte Flagge zurueck zur gegnerischen Basis. Das Match und die Uhr laufen weiter.
- Bei Gleichstand nach 15 Minuten gibt es einmalig volle drei Minuten Nachspielzeit. Danach gewinnt das Team mit mehr Punkten; bei erneutem Gleichstand endet das Match unentschieden.
- Flaggen werden beim Beruehren automatisch aufgenommen. Stirbt der Traeger, faellt die Flagge auf den Boden. Ein Mitspieler kann sie weitertragen; ein Angehoeriger des Flaggen-Teams bringt seine eigene Flagge durch Beruehren sofort zurueck.
- Eine liegengelassene Flagge kehrt nach 30 Sekunden automatisch zur Basis zurueck.
- Respawn acht Sekunden nach dem Tod in der eigenen Basis, mit M4, Pistole, Messer, voller Gesundheit und leichter Ruestung. Punkte und persoenliche K/A/D bleiben erhalten.
- Ein vollstaendig ausgeschaltetes Team verliert nicht automatisch: Respawns laufen waehrend der aktiven Spielzeit weiter.

## Karte

Symmetrische Arena (52 x 88 Meter) mit farbigen Alpha-/Bravo-Basen. Die Mitte enthaelt ein Kampfareal mit mehreren Deckungsinseln. Links und rechts fuehren getrennte Flankenwege zur anderen Basis; Querverbindungen an der Mitte und den Basiseingaengen erlauben Routenwechsel. Zehn getrennte Spawnpunkte, Deckung vor den Basen und unterbrochene Sichtlinien.

## Technische Pruefung

`VanthexBuildArt -BakeCTF` erstellt die gespeicherte Unreal-Map. `-game -CTFTest -nullrhi -unattended` auf der CTF-Karte prueft im laufenden Spiel 20 Bedingungen zu Teamgroesse, Navigation, Aufnahme, Tod, Uebergabe, Respawn, Punktestand und Zeitregeln. Bericht: `Saved/CTFTest.txt`.

Der Server entscheidet ueber Flaggenbesitz, Punkte und Respawns. Flaggenbewegung/-status, Countdown und Nachspielzeit werden repliziert. Ein separater Mehrrechner-LAN-Playtest steht noch aus. Die erste Kartenfassung verwendet vorhandene Materialien und geometrische Deckung; Balancing und Ausstattung koennen durch Playtests verfeinert werden.
