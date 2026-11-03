# VANTHEX – Waffen- und Wirtschaftsaudio

Stand: 3. November 2026. **Audio-Router, Ereignisprofile und GRID-Raummodell sind
implementiert; 119 originale synthetische Mono-WAVs liegen als Quelldateien vor.**
Import, automatisierte Prüfungen und echte Stereo-Mixeraufnahme sind bestanden;
die Messwerte stehen unten. Produktionsmix und menschliche Hörabnahme bleiben offen.
Waffen-/Wirtschaftsregeln: ECONOMY_AND_WEAPONS.md. Bestehender Vorbeiflug und
Materialeinschlag: BULLET_AUDIO.md.

Der Import-Commandlet erzeugt editierbare Unreal-Ereignisbänke, Attenuation und
Concurrency-Konfiguration. Herkunft, Generator, Manifest und Import stehen in
[AudioSource/Weapons/README.md](../AudioSource/Weapons/README.md).
[Implementierung und Prüfaufrufe](ECONOMY_IMPLEMENTATION.md) unterscheiden den
vorliegenden Prototyp vom nachstehenden gestalterischen Abnahmeziel.

## Erste Priorität: AUG und MP7

| Ebene | AUG | MP7 |
|---|---|---|
| Schuss | kräftiger, definierter Rifle-Transient; kontrollierter Körper | kurzer, kompakter SMG-Transient; prägnante schnelle Schussfolge |
| Mechanik | erkennbarer Verschluss mit zurückhaltendem metallischem Detail | trockenes, kurzes mechanisches Klicken |
| Nachladen | getrennte Magazin-entnehmen/-einsetzen- und Verschlussaktionen | eigene kompakte Magazin- und Verschlussaktionen |
| Raumantwort | zum Raum passende frühe Reflexionen und Ausklang | dieselbe Raumlogik mit eigenem Waffensignal |
| Entfernung | Waffenidentität auch im gedämpften Fernklang erhalten | schnelle Kadenz und kompakter Charakter bleiben erkennbar |

Die Feuerrate folgt dem tatsächlichen Waffenprofil. Sound darf keine zusätzlichen
Schüsse suggerieren. Mehrere abgestimmte Varianten verhindern hörbare Schleifen,
ohne die Lautstärke oder Ortbarkeit eines Gegners zufällig zu verändern.

## Ereignisse und Autorität

- Bestätigter Schuss: ein räumlicher Mündungsklang, optional lokales Mechanikdetail.
  Den bisherigen Near-Miss-/Impact-Router weiterverwenden, ohne doppelte Einschläge.
- Nachladen: Start, Magazin heraus, Magazin hinein, Verschluss, abgeschlossen und
  Abbruch. Abbruch durch Waffenwechsel/Tod stoppt geplante Folgegeräusche.
- Leerschuss: nur bei tatsächlich leerem Magazin und gültiger Betätigung; begrenzen,
  damit gehaltenes Feuer keinen übermäßigen Klick-Loop erzeugt.
- Waffenwechsel: Holster/Draw folgen dem tatsächlichen Wechselzustand, nicht allein
  einem Tastendruck. Messer erhält eigene Zieh-, Rücksteck- und Schwungklänge.
- Kauf: bestätigter Kauf, Ablehnung und Kaufphasenbeginn/-ende als kurze lokale UI-
  Sounds. Erst nach Serverantwort bestätigen. Kein öffentliches Geräusch verrät,
  welche Waffe ein Gegner gekauft hat. Credits entstehen ausschließlich im Gameplay.
- Gadgets: getrennte Aktivierungs-, Flug-, Einschlag-, Wirkungs- und Endereignisse,
  soweit der Gadget-Typ sie tatsächlich besitzt. Keine unimplementierte Wirkung vertonen.

Nachladephasen brauchen explizite Zeitmarken im Waffenprofil beziehungsweise später
Animations-Notifies. Ein generischer langer Sound darf nach einem Abbruch nicht
weiterlaufen. Vorhersage und Serverbestätigung benötigen dieselbe Ereignis-ID,
damit der Besitzer keinen doppelten Schuss hört. Für den ersten Slice ist derselbe
bestätigte Serverpfad wie beim bestehenden Hitscan die einfachere Grundlage.

## GRID: Richtung, Etagen und Räume

Räumliche Quellen verwenden echte Weltpositionen und Mono-Quelldateien. Die
Hörposition folgt der aktiven Spiel-/Zuschauerkamera. Keine vorbereitete Stereo-
Richtung in den Quelldateien; Höhenortung muss mit der gewählten Unreal-Spatialization
und dem Ausgabeprofil tatsächlich geprüft werden.

Direktschall wird nach Entfernung abgeschwächt. Blockierende Wände/Decken dämpfen
und filtern ihn. Ein Nachbarraum darf durch eine Tür einen anderen Hörweg besitzen
als durch eine geschlossene Betonwand. Dafür ist eine raumbezogene Verbindung von
Zonen und Tür-/Portalzuständen vorzusehen; eine einzelne Sichtlinienabfrage allein
beweist keine korrekte Raumakustik. Den strengen bestehenden Near-Miss-Wandtest
für Projektilereignisse beibehalten; er ersetzt keine Schussausbreitung.

Kurze frühe Reflexionen vermitteln Raumgröße; Hall bleibt so zurückhaltend, dass
Schritte, Core und Schussrichtung lesbar bleiben. Schüsse oberhalb benötigen die
korrekte Quellenhöhe und Deckendämpfung. Ausschließlich Stereo-Panning links/rechts
reicht als Höhenprüfung nicht aus.

## Benötigte Klanggruppen

Je Waffe: Schussvarianten, Mechanik, Nachladephasen, Leerschuss, Holster/Draw.
Zusätzlich: konkrete Materialeinschläge (Beton, Stahl, Gitter, später weitere
zugeordnete Oberflächen), Flash, Smoke, Frag, EMP, Sensor, Rüstungs-/Equipmentbewegung
und lokale Kaufhinweise. Die vorhandenen synthetischen Bullet-WAVs bleiben bis
zum gezielten Ersatz Prototypen. Neue Aufnahmen benötigen dokumentierte Herkunft
und Nutzungsrechte.

Priorisierung im Mix: unmittelbare Gefahr und gegnerische Aktionen, Core-Feedback,
eigene Waffe, Umgebung; Kauf-UI bleibt kurz und verdeckt keinen Rundenbeginn.
Rüstungskleidung darf keine zusätzliche Aussage über gegnerische Rüstungspunkte
vermitteln. Schritt- und Equipmentgeräusche werden als zusammengehörige Aktion
gemischt, statt unbegrenzt überlagert.

## Abnahme im gerenderten Mehrspieler-Spieltest

1. AUG/MP7 ohne Sichtkontakt zuverlässig unterscheiden: gleicher Raum, Nachbarraum,
   Gang und obere Plattform. Lautstärkevergleich bei gleichem Abstand durchführen.
2. Quellen links/rechts/vorne/hinten/oben/unten prüfen, mit Kopfhörer und Stereo-
   Lautsprechern. Höhen-Ergebnisse getrennt dokumentieren.
3. Tür offen/geschlossen, massive Wand und Deckentrennung vergleichen. Gedämpfter
   Schall darf keine präziseren Informationen liefern als das gewählte Hörmodell.
4. Einzel-/Dauerfeuer, gleichzeitiges 3v3, Reload-Abbruch, Tod, Waffenwechsel und
   Zuschauerübergänge prüfen; keine doppelten oder weiterlaufenden Ereignisse.
5. Fehlende Sounds, Concurrency-Grenzen, Netzlatenz und Paketverlust testen.
   Gameplay, Munition und Credits bleiben bei ausgelassenem Audio korrekt.
6. Mixeraufnahmen auf Richtung, Pegel und Clipping prüfen; anschließend menschliche
   Hörtests auf Waffenidentität und taktische Verständlichkeit durchführen.

## Aktueller Implementierungsstand und Grenzen

Die neun Feuerwaffen besitzen eigene Schussvarianten, Mechanik, Nachladephasen,
Leerschuss und Holster/Draw. Messer und lokale Kauf-/Phasenhinweise haben eigene
Ereignisse. Die 119 Quelldateien sind reproduzierbare prozedurale Synthese in
Mono, 48 kHz PCM16; sie sind keine authentischen Waffenaufnahmen. Für die noch
fehlenden Gadget-Wirkungen werden keine Wirkungsgeräusche behauptet.

Drei gemeinsame Bewegungsvarianten mischen Fußkontakt und leise Ausrüstung in
einer Aktion. Klangwahl und Klangfarbe lesen weder Rüstungstyp noch Rüstungspunkte.
Die tatsächlich zurückgelegte Bodenstrecke bestimmt serverseitig die Schritte;
Stillstand, Luftbewegung, Tod, inaktive Runde und Teleports setzen den Schrittgeber
zurück. Geduckte Bewegung hat 32 % Lautstärke. Die Bewegung liegt im Stimmenbudget
unter Schüssen und über Mechanikdetails und benutzt denselben räumlichen Router.

Der Server löst Weltgeräusche aus und adressiert hörbare Besitzer-Verbindungen.
Ereignis-IDs begrenzen Doppelwiedergabe; Reload-Abbruch stoppt laufende und verspätete
Phasen. Die vorhandenen Projektil-Vorbeiflug- und Einschlagereignisse bleiben getrennt.

GRID verwendet einen Graphen aus Korridor- und Treppenverbindungen, aktuelle
Blockierungstests einschließlich beweglicher Türen, Weglänge, Tiefpassfilterung und
eine leise Transmissionsersatzlösung bei fehlendem offenen Weg. Die Prüfung der
Graphkanten wird höchstens alle 100 ms erneuert. Frühe Reflexionen sind kurz und
vereinfacht. Das ist eine begrenzte Portal-/Raum-Näherung, keine Simulation der
Schallwellenausbreitung.

## Belegte Prüfungen vom 3. November 2026

Der Import meldet **119 WAVs und zehn Waffenbänke**. Alle 14 Automation-Gruppen
des gemeinsamen Laufs bestehen, darunter die vier Waffen-Audio-Gruppen
`CompleteMonoBanks`, `EquipmentMovementCadence`, `MixPriorities` und
`PhysicalPortalAndWall`. Die letzte Gruppe prüft physische Tür-/Wand-/Deckenblockierung
und alternative GRID-Hörwege in einer Kollisions-Testwelt.
[Import-/Buildnachweis](Validation/EconomyArmorAudio/BuildAndImport.txt),
[Automation-Ergebnisse](Validation/EconomyArmorAudio/Automation.json).

Die echte Mixeraufnahme mit `-WeaponAudioPlaybackTest` besteht alle **13 Prüfungen**:
sieben geplante Ereignisse, keine Doppelwiedergabe, Reload-Abbruch und Unterdrückung
verspäteter Phasen, fehlende Assets, gemeinsamer Fuß-/Equipmentklang, getrennte
Schusszähler, Stimmenlimit und Prioritätsverdrängung, autorisierter Dispatch,
Reichweite und frische Aufnahme. [Laufzeitbericht](Validation/EconomyArmorAudio/WeaponAudioPlayback.txt).

Die [aufgenommene WAV](Validation/EconomyArmorAudio/WeaponAudioPlayback.wav) enthält
**428.032 Stereo-Frames bei 48 kHz** (8,917 Sekunden). Der gemessene Spitzenwert
beträgt **0,180939 beziehungsweise −14,85 dBFS**; **kein Sample erreicht die
Clipping-Prüfschwelle von 0,999**. Alle sieben Ereignisfenster enthalten ein
Signal. [Wellenformanalyse](Validation/EconomyArmorAudio/WeaponAudioPlayback-analysis.json).

| Probe | RMS links | RMS rechts |
|---|---:|---:|
| AUG links | 0,0177533 | 0 |
| MP7 rechts | 0 | 0,0116994 |
| Pistole vorne | 0,0076927 | 0,0076927 |
| AUG hinten | 0,0088577 | 0,0088577 |
| AUG oben | 0,0088638 | 0,0088638 |
| AUG unten | 0,0088763 | 0,0088763 |
| AUG vorne, vorgegeben gedämpft/gefiltert | 0,0008826 | 0,0008826 |

Links-/Rechts-Verteilung und Wiedergabe der vorgegebenen Dämpfung sind damit
gemessen. Die physische Portalberechnung wird separat durch die Automation geprüft;
die Mixerprobe ersetzt keinen Hörtest durch Türen in einer laufenden Partie.
Gleiche Stereopegel für vorne/hinten/oben/unten beweisen weder deren wahrnehmbare
Unterscheidung noch HRTF oder zuverlässige Höhenortung. Höhenwahrnehmung,
Waffenunterscheidbarkeit, finaler Mix, Paketverlustverhalten und belastbare
Performancegrenzen benötigen weiterhin dokumentierte Hör- und Belastungstests.
Der obige gestalterische Abnahmekatalog bleibt für diese Punkte offen.
