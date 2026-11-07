# Capture the Flag / Crossroads CTF

## Spielen

VANTHEX startet im Hauptmenue. Unter **Spielen** die Karte **Capture the Flag** waehlen und **Partie starten** klicken. Fuer LAN den Modus waehlen, zu **Netzwerk** wechseln und **LAN-Partie hosten** waehlen. Mitspieler verbinden sich dort ueber die Host-Adresse. Freie Plaetze werden mit Bots besetzt.

## Regeln

- Fuenf Spieler pro Team, zwei Flaggen in den eigenen Basen.
- Springen mit der Leertaste waehrend der aktiven Spielzeit, auch mit getragener Flagge.
- 15 Minuten aktive Spielzeit nach der Vorbereitung. Jede gegnerische Flagge, die in die eigene Basis gebracht wird, zaehlt einen Punkt. Die eigene Flagge muss dort stehen.
- Nach einem Punkt geht die eroberte Flagge zurueck zur gegnerischen Basis. Das Match und die Uhr laufen weiter.
- Bei Gleichstand nach 15 Minuten gibt es einmalig volle drei Minuten Nachspielzeit. Danach gewinnt das Team mit mehr Punkten; bei erneutem Gleichstand endet das Match unentschieden.
- Flaggen werden beim Beruehren automatisch aufgenommen. Stirbt der Traeger, faellt die Flagge auf den Boden. Ein Mitspieler kann sie weitertragen; ein Angehoeriger des Flaggen-Teams bringt seine eigene Flagge durch Beruehren sofort zurueck.
- Eine liegengelassene Flagge kehrt nach 30 Sekunden automatisch zur Basis zurueck.
- Zehn Sekunden nach Matchende startet die naechste Partie. Punkte, Nachspielzeit und Flaggen werden zurueckgesetzt.
- Respawn acht Sekunden nach dem Tod in der eigenen Basis, mit M4, Pistole, Messer, voller Gesundheit und leichter Ruestung. Punkte und persoenliche K/A/D bleiben erhalten.
- Ein vollstaendig ausgeschaltetes Team verliert nicht automatisch: Respawns laufen waehrend der aktiven Spielzeit weiter.

## Karte

Symmetrische roemische Hofarena (52 x 88 Meter) mit Alpha-/Bravo-Basen. Travertin-Mauern, Marmorsaeulen, echte Torboegen, Tempelfassaden mit Terrakotta-Giebeln und eine Siegessaeule praegen die Architektur. Instanziertes Steinpflaster und gedimmte roemische Pigmente markieren die Basen. Zypressen stehen ausserhalb des Spielfelds. Die Mitte enthaelt ein Kampfareal mit mehreren Deckungsinseln. Links und rechts fuehren getrennte Flankenwege zur anderen Basis; Querverbindungen an der Mitte und den Basiseingaengen erlauben Routenwechsel. Zehn getrennte Spawnpunkte, Deckung vor den Basen und unterbrochene Sichtlinien.

## Technische Pruefung

`VanthexBuildArt -BakeCTF` erstellt die gespeicherte Unreal-Map. `-game -CTFTest -nullrhi -unattended` auf der CTF-Karte prueft im laufenden Spiel 28 Bedingungen zu Teamgroesse, roemischer Architektur, Pflaster, Himmel, Sonnen-Schatten, Navigation, Aufnahme, Tod, Uebergabe, Respawn, Punktestand, Zeitregeln und automatischem Neustart. Bericht: `Saved/CTFTest.txt`.

Der Server entscheidet ueber Flaggenbesitz, Punkte und Respawns. Flaggenbewegung/-status, Countdown und Nachspielzeit werden repliziert. Ein separater Mehrrechner-LAN-Playtest steht noch aus. Ein eigener mediterraner Himmel mit sichtbarer Atmosphaeren-Sonnenscheibe, warmem Nachmittagslicht, dynamischen Sonnenschatten und leichter Fernhaze beleuchtet die Aussenarena. Der Bake erhaelt auch dekorative Meshes ohne Kollision und gemeinsam verwendete Steinmaterialien. Balancing und Ausstattung koennen durch Playtests verfeinert werden.
