# Waffen ablegen und aufnehmen

- **X** wirft die ausgewaehlte Schusswaffe nach vorne. Das Messer bleibt beim Spieler.
- **E** nimmt eine nahe Waffe auf, auf die der Spieler schaut. Ein erreichbarer
  Core hat bei dieser Taste Vorrang; G und Q bleiben fuer den Core reserviert.
- Das sind die Standardtasten. Aufnehmen und Waffenablegen lassen sich unter
  **Maus & Steuerung** umbelegen.
- Beim Tod fallen Primaerwaffe und Pistole zu Boden. Gegner und Mitspieler
  koennen beide Waffen aufnehmen. Es gibt keine Team- oder Besitzerbindung.
- Ist der passende Slot bereits belegt, wird dessen bisherige Waffe fallengelassen.
  Magazin und Reservemunition bleiben bei jeder einzelnen Waffe erhalten, auch
  bei leerer Waffe oder abgebrochenem Nachladen.
- Wegwerfen und Aufnehmen sind in Kaufphase und aktiver Spielzeit erlaubt.
  Tote Spieler koennen keine Waffen aufnehmen. Abstand, Blickrichtung und freie
  Sicht werden auf dem Server geprueft; gleichzeitige Ansprueche duplizieren nichts.
- Liegengebliebene Waffen verschwinden nach drei Minuten oder beim Rundenwechsel.
  CTF-Respawns erhalten das regulaere Modus-Loadout; die Drops des Toten bleiben
  bis zum Ablauf bzw. Rundenwechsel fuer andere Spieler verfuegbar.

## Implementierung

`AVanthexWeaponPickup` repliziert Waffenkennung, Munition und Position. Nur der
Server bewegt den Drop und uebertraegt Inventar. Der Client benutzt die bereits
vorhandenen Waffenmodelle und zeigt den Interaktionshinweis im HUD. Drop-Kollision
blockiert keine Spieler oder Schuesse. Inventarwechsel brechen Nachladen, Feuer
und ADS ab und verwenden die bestehende Wechselverzoegerung.

Der gemeinsame Stand enthaelt auch die sieben Partner-Commits bis `e0e4a1a`:
Maus-Empfindlichkeit, Waffen-Bewegungstempo, Killfeed, CTF mit Respawns, Springen
und die roemische Arena sowie das eigene Hauptmenue mit Modusauswahl. Der
automatische Match-Neustart bleibt erhalten.

## Nachgewiesene Pruefungen am 6. November 2026

- Editor-Build des zusammengefuehrten Stands erfolgreich.
- `Build/Test-WeaponDrops.ps1`: je 29 Gameplay-Pruefungen auf GRID V4 und
  Crossroads CTF bestanden. Darunter Wand-/Distanzpruefung, Tod beim Waffenwechsel,
  Munitionserhalt, Tauschen, leere Slots, doppelte Anfragen und Bodenlandung.
- Separater Listen-Server und Client: Besitzer-RPCs, replizierter Drop samt
  Modell, Aufnahme, Inventar und exakte Munition auf beiden Seiten bestaetigt.
- CTF-Regressionssuite: 24 Pruefungen bestanden, einschliesslich Flaggen und Respawn.
- Steuerungssuite: 31 Pruefungen bestanden, einschliesslich Springen und Killfeed.
- Waffenprofil-Suite: 24 Pruefungen bestanden, einschliesslich Nachladen und Slot-Munition.
- Nach Aufnahme des neuen Hauptmenues erneut beide Drop-Suiten und den
  Zwei-Prozess-Netzwerktest bestanden.
- Windows-Development-Paket erfolgreich erstellt. Die fertige EXE startet im
  gerenderten Hauptmenue; Screenshot lokal unter `Saved/WeaponDrops/packaged-mainmenu.png`.
  Auch in der gepackten Anwendung bestehen je 29 Drop-Pruefungen auf beiden Karten.

Die neue Anwendung liegt unter `Saved/Standalone-WeaponDrops/Windows/Vanthex.exe`.
Zum Weitergeben wird der gesamte Windows-Ordner benoetigt, nicht nur die EXE.

Die Berichte und Logs liegen lokal unter `Saved/WeaponDrops/`. Die Netzwerktests
laufen mit getrennten Prozessen auf einem PC; ein Test mit zwei entfernten PCs
oder dieses Updates auf Edgegap wird dadurch nicht behauptet.

**Client und Server muessen aktualisiert werden.** Die aeltere verteilte EXE/ZIP
kennt die neuen Drop-Objekte und Tasten nicht. Ein GitHub-Push aktualisiert keine
bereits laufende Edgegap-Instanz. Lizenzierte lokale Fab-Assets werden weiterhin
nicht in das oeffentliche Repository aufgenommen.
