# VANTHEX – Waffen- und Wirtschaftsaudio

Stand: 2. November 2026. **Geplantes Sounddesign, noch keine neuen Audioassets.**
Waffen-/Wirtschaftsregeln: ECONOMY_AND_WEAPONS.md. Bestehender Vorbeiflug und
Materialeinschlag: BULLET_AUDIO.md.

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

Noch nicht nachgewiesen: neue Waffenklänge, Raum-/Portalmodell, Höhenortung,
finaler Mix und belastbares Audio-Performancebudget.
