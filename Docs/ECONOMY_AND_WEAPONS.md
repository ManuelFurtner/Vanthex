# VANTHEX – Waffen und Wirtschaft

Stand: 3. November 2026. **Wirtschaft, Kaufphase, Rüstung und Waffenprofile sind
implementiert; 45 Integrations-, 32 Core- und 24 Waffenprüfungen sowie der
Host-/Client-Ablauf sind bestanden.** Den belegten Umfang und die verbleibenden
Produktionsgrenzen nennt der
[Validierungsstand](ECONOMY_IMPLEMENTATION.md#vorliegende-validierung).
Ergänzung vom 4. November: Die fünf Gadget-Wirkungen und ihre serverseitige
Anwendung sind umgesetzt und mit sechs Automationgruppen, 58 Weltprüfungen sowie
einem echten Host-/Client-Lauf geprüft. Wirkungsprofile und Nachweise stehen in
[Gadget-Regeln und Prüfungen](GADGETS.md).
Die Preise, Credit-Belohnungen und Spielsäulen stammen aus dem neuen Teamkonzept.
Die mit „Startvorschlag“ bezeichneten Regeln konkretisieren offene Entscheidungen
für einen ersten Playtest. Sie sind keine bereits gemessenen Balance-Ergebnisse.
Implementierungsumfang, offene Punkte und Testaufrufe:
[Wirtschaft, Rüstung und Waffen-Audio](ECONOMY_IMPLEMENTATION.md).

## Spielrahmen

Kompetitiver 3v3 Tactical FPS mit realistischen Waffen, präzisem Gunplay,
kontrollierbarem Rückstoß, niedriger Time-to-Kill und kostenlosen
Operator-Fähigkeiten. Spieler wählen einen Operator und kaufen Waffen,
Rüstung sowie höchstens zwei zusätzliche Gadgets.

Eine aktive Runde dauert 180 Sekunden; keine Respawns während dieser Phase.
Sieben Rundensiege gewinnen das Match. Der neutrale Energy Core kann aufgenommen,
getragen, abgelegt, geworfen, gefangen und abgefangen werden. Fünf Sekunden
ununterbrochene Extraction oder die Eliminierung des Gegners gewinnen die Runde.
Beide Teams dürfen beide Extraktionspunkte benutzen. Bestehende Timeout- und
Unentschiedenregeln bleiben bestehen.

## Kaufphase und Inventar – Startvorschlag

- Vor jeder aktiven Runde liegen **25 Sekunden Kaufphase**. Die 180-Sekunden-Uhr
  startet danach. Runde 1 hat dieselbe Kaufzeit.
- Während der Kaufphase bleiben Figuren im jeweiligen Spawn; Bewegung, Schüsse,
  Messerangriffe, Core- und SHIFT-Interaktionen sind gesperrt. Blick und Kaufmenü
  bleiben bedienbar. Noch laufende Angriffs-/Nachladetimer werden abgebrochen.
- Käufe sind ausschließlich in der Kaufphase möglich. Der Server prüft Phase,
  Spieler, Creditstand, Preis und Inventarlimit und führt jeden Kauf atomar aus.
- Ein Primärwaffenplatz, ein Pistolenplatz, ein kostenloses Messer, ein
  Rüstungsplatz und zwei zusätzliche Gadget-Plätze. Gleiche Gadget-Typen dürfen
  beide Plätze belegen. Kostenlose Operator-Fähigkeiten belegen diese Plätze nicht.
- Jeder Spieler startet das Match mit Standardpistole, Messer, 800 CR und ohne
  Rüstung/Gadgets. Der Kauf einer Primärwaffe ersetzt die vorhandene Primärwaffe;
  ein Pistolen-Kauf ersetzt die Pistole. Ersetzte Ausrüstung wird nicht erstattet.
- Überlebende behalten ihre Waffen, verbleibende Rüstungspunkte und unbenutzte
  Gadgets. Waffenmagazine und Reserve werden zum Rundenstart kostenlos aufgefüllt;
  HP werden auf 100 gesetzt. Verwendete Gadgets bleiben verbraucht.
- Wer stirbt, verliert gekaufte Waffen, Rüstung und verbleibende Gadgets. In der
  nächsten Kaufphase erhält er wieder Standardpistole und Messer kostenlos.
  Credits bleiben erhalten und erhalten die normalen Rundenbelohnungen.
- Standardpistole und Messer bleiben beim Waffenkauf verfügbar. Zu Beginn einer
  aktiven Runde wird die Primärwaffe ausgewählt, sonst die Pistole.
- Rundenintermission und Matchende bleiben eigene Phasen. Nach Matchende ist
  keine weitere Kaufphase möglich; ein neues Match setzt Wirtschaft und Inventar zurück.

## Waffenpreise – Teamvorgabe

| Waffe | Preis |
|---|---:|
| Standardpistole | kostenlos |
| P250 | 350 CR |
| MP5 | 1.300 CR |
| MP7 | 1.900 CR |
| M4 | 2.500 CR |
| AK-Plattform | 2.700 CR |
| AUG | 2.900 CR |
| DMR | 3.200 CR |
| Shotgun | 1.600 CR |

AUG: präzise Allround-Rifle für mittlere und größere Distanzen.
MP7: schnelle SMG für enge Räume, Nahkampf und Pushes. Standardpistole, AUG und MP7
bleiben die ersten drei Prioritäten für die weitere Ausarbeitung. Alle neun
aufgeführten Waffen haben inzwischen eigene Startprofile für Schaden, Feuerart,
Feuerrate, Rückstoß, Streuung, Reichweitenabfall, Magazin, endliche Reserve und
Nachladedauer. Diese Parameter sind Playtest-Ausgangswerte, keine abgeschlossene
Balance oder Produktionsfreigabe. Die frühere 40-Schaden-Rifle bleibt ausschließlich
als ausdrücklich ausgewählte Diagnose-Fixture erhalten.

## Gadgets und Rüstung

| Ausrüstung | Preis |
|---|---:|
| Flashbang | 200 CR |
| Smoke | 300 CR |
| Frag | 350 CR |
| EMP | 350 CR |
| Sensor | 250 CR |
| Leichte Rüstung | 400 CR |
| Schwere Rüstung | 750 CR |

**Rüstungswirkung – Startvorschlag:** Leicht: 40 Rüstungspunkte, 25 % Absorption.
Schwer: 80 Rüstungspunkte, 40 % Absorption. Pro Treffer wird nach dem Waffen-
und Distanzschaden `absorbiert = min(Rüstungspunkte, Schaden × Absorptionsfaktor)`
berechnet. HP verlieren den Rest; Rüstung verliert genau den absorbierten Betrag.
Die Anzeige rundet, die Schadensrechnung behält die Bruchteile.

Rüstung schützt Körper und Gliedmaßen; Kopftreffer und Messer umgehen sie.
Schadensquellen kennzeichnen dafür explizit Trefferzone und Schadenstyp.
Kopfschaden verwendet weiterhin den jeweiligen Waffenmultiplikator.
Beide Rüstungen haben zunächst keine Geschwindigkeitsstrafe. Die Rüstungsrechnung
verarbeitet ausdrücklich als Projektil oder Explosion markierten Schaden.
Frag-Explosionen verwenden die gleichen Absorptionsfaktoren und keinen
Kopftreffermultiplikator. Umgebungsschaden bleibt unabsorbiert. Details zu
Sichtblockaden, Selbstschaden, Rauch, EMP und Sensoren sind in den
[Gadget-Regeln](GADGETS.md) festgelegt; die Preise oben bleiben unverändert.

| Testtreffer | HP-Verlust | Restliche Rüstung |
|---|---:|---:|
| 40 Körperschaden gegen volle leichte Rüstung | 30 | 30 |
| 40 Körperschaden gegen volle schwere Rüstung | 24 | 64 |
| 40 Körperschaden bei 5 verbleibenden Rüstungspunkten | 35 | 0 |
| 80 Kopfschaden gegen volle schwere Rüstung | 80 | 80 |
| 50 Messerschaden gegen volle schwere Rüstung | 50 | 80 |

Rüstung stapelt sich nicht. Eine beschädigte Rüstung darf zum vollen Preis durch
eine volle desselben Typs ersetzt werden. Leicht → schwer kostet volle 750 CR;
Restpunkte werden ersetzt. Schwer → leicht wird abgelehnt. Erneuter Kauf einer
bereits vollen Rüstung desselben Typs wird ohne Abbuchung abgelehnt.

## Credits – Teamvorgabe und präzisierte Abrechnung

| Ereignis | Belohnung |
|---|---:|
| Matchstart | 800 CR |
| Rundensieg | +3.000 CR pro Spieler |
| Erste aufeinanderfolgende Niederlage | +1.900 CR pro Spieler |
| Zweite aufeinanderfolgende Niederlage | +2.400 CR pro Spieler |
| Dritte und weitere aufeinanderfolgende Niederlagen | +2.900 CR pro Spieler |
| Kill | +200 CR für den Killer |
| Assist | +100 CR für einen berechtigten Assister |
| Erfolgreiche Core-Extraction | zusätzlich +300 CR pro Mitglied des Siegerteams |
| Core-Intercept | +100 CR, nach den Grenzen unten |
| Sinnvoller Core-Pass | +50 CR für den Werfer, nach den Grenzen unten |

Maximum: **7.000 CR pro Spieler**. Jede Gutschrift wird einzeln gedeckelt; es gibt
keinen Übertrag über das Limit. Preise und Kontostände sind ganzzahlige Credits.
Eine unzulässige oder doppelte Kauftransaktion verändert weder Credits noch Inventar.

**Startvorschlag zur Niederlagenserie:** Die Serie gehört dem Team, nicht einem
wechselnden Spieler. Ein Sieg setzt die eigene Serie auf 0; eine Niederlage
erhöht sie um 1. Unentschieden geben beiden Teams je 1.900 CR und verändern keine
Serie. Die Extraction-Prämie ist zusätzlich zum Siegbonus fällig, auch für bereits
gestorbene Teammitglieder. Bei Eliminierung/Timeout gibt es keine Extraction-Prämie.
Alle Rundenergebnisse werden genau einmal abgerechnet.

Kill-/Assist-/Core-Credits werden beim bestätigten Ereignis gutgeschrieben;
Rundenboni nach dem einmalig festgestellten Ergebnis, vor der nächsten Kaufphase.
Tod nimmt die Ausrüstung, nicht den Kontostand. Selbsttötung, Umwelttod und
freundlicher Schaden geben keine Kill- oder Assist-Credits. Späte Treffer nach
Rundenende erzeugen keine weiteren Wirtschaftsereignisse.

Ein Join übernimmt den vorhandenen Bot-Slot samt Credits und Inventar; Disconnect
übergibt denselben Stand an den Bot. Reconnect erzeugt kein neues Startguthaben.
Auch ein toter Slot erhält seine Team-Rundenbelohnungen. Teamwechsel innerhalb
eines laufenden Matches sind für den ersten Wirtschaftsslice gesperrt.

## Assist-Wertung – Startvorschlag

Ein Spieler qualifiziert sich mit **mindestens 30 tatsächlich abgezogenen HP**
gegen das getötete Ziel innerhalb der **acht Sekunden vor dessen Tod**. Absorbierter
Rüstungsschaden, Overkill, freundlicher Schaden und kosmetische Treffer zählen nicht.
Der Killer erhält ausschließlich den Killbonus für diesen Tod.

Pro Tod wird höchstens ein Assist bezahlt: der qualifizierte Mitspieler mit der
höchsten relevanten HP-Schadenssumme. Bei Gleichstand entscheidet der jüngste
Schadenszeitpunkt, danach der niedrigere stabile Team-Slot. Ein inzwischen
gestorbener Assister bleibt berechtigt. Heilung/Respawn und Rundenreset löschen
das betroffene Schadensprotokoll; ein Tod kann nur einmal vergütet werden.

## Core-Pass und Intercept – Startvorschlag gegen Farming

Ein Pass zählt nur bei serverbestätigtem **direktem Fang durch einen lebenden
Mitspieler** nach einem echten Wurf. Aufnehmen vom Boden, manuelles Ablegen,
Todes-Drop, eigener Wiederfang und ein Wurf vor Beginn/nach Ende der aktiven Runde
geben keine Pass-Belohnung.

Für +50 CR müssen beide Bedingungen erfüllt sein:

1. Zwischen Core-Abwurfposition und Fangposition liegen mindestens **400 cm**.
2. Der kürzeste begehbare Weg von der Fangposition zu einer der beiden Extraktionen
   ist mindestens **400 cm kürzer** als der entsprechende kürzeste Weg ab Abwurf.
   Verglichen wird jeweils das Minimum zu A/B, auf dem aktuellen Kollisionsgraphen
   und der richtigen Etage. Fehlt ein gültiger Weg, gibt es keinen Passbonus.

Pro Werfer gibt es höchstens **zwei bezahlte Pässe pro Runde** (100 CR), pro Team
höchstens **vier** (200 CR). Jedes gerichtete Werfer/Empfänger-Paar wird je Runde
nur einmal belohnt. Hin- und Herwerfen bleibt spielerisch möglich; diese Grenzen
begrenzen den wirtschaftlichen Ertrag. Teleportierte oder zurückgesetzte Würfe
verlieren ihre Bonusberechtigung. Jeder Wurf besitzt eine eindeutige Ereignis-ID.

Ein Intercept ist der direkte Fang eines von einem lebenden Gegner geworfenen
Cores. Der Fang gibt **einmal pro Spieler und Runde +100 CR**. Aufheben eines
gegnerischen Drops oder erneutes Verarbeiten desselben Wurfs zählt nicht.
Ein Intercept erzeugt keinen Passbonus für den gegnerischen Werfer. Das sind
begrenzende Prototypregeln; Kollusion und gewünschte taktische Intercepts werden
im Playtest gesondert beobachtet.

## Kaufbeispiele

| Strategie | Beispiel | Gesamtkosten |
|---|---|---:|
| Full Buy | AUG + schwere Rüstung + Flash + Smoke | 4.150 CR |
| Force Buy | MP7 + leichte Rüstung + Flash | 2.500 CR |
| Eco | Standardpistole + Messer, Credits sparen | 0 CR |
| Runde 1 | P250 + leichte Rüstung | 750 CR |

Diese Beispiele sind Alternativen innerhalb der Regeln. Core-Passing, Flanken und
Extraction müssen auch mit kostenloser Bewaffnung möglich bleiben.

## Umsetzung und Abnahmekriterien

1. Datenkatalog für Waffen, Rüstung und Gadgets mit stabilen IDs. Serverseitiges
   Creditkonto und Inventar am persistenten Spieler-/Rosterzustand; private
   Kontodetails nur an den Besitzer replizieren. Gegner sehen keine Kaufdetails.
2. Kaufphase im autoritativen Rundenablauf, atomare validierte Kaufanforderungen,
   Weitergabe an Bot/Reconnect, Tod-/Überleben-Regeln und idempotente Rundenabrechnung.
3. Standardpistole, AUG und MP7 als getrennte Waffenprofile mit Rückstoß, passenden
   Feuerarten und Waffenwechsel; bestehendes Messer integrieren. Danach weitere Waffen.
4. Rüstungsrechnung und HP-Schadensprotokoll; serverseitige Kill-/Assist-/Core-
   Ereignisse. Wirtschaft darf niemals aus Audio-/VFX-Ereignissen berechnet werden.
5. Kaufoberfläche und Audio nach WEAPON_AUDIO_DESIGN.md; Gadgets anschließend
   entsprechend ihren noch auszuarbeitenden Wirkungsprofilen implementieren.

Erforderliche Tests: Kauf mit exakten/zu wenigen Credits, Creditlimit, ungültige IDs,
gleichzeitige/duplizierte Käufe, zwei Gadget-Plätze, Rüstungsbeispiele oben,
Überleben/Tod, Serienfolge Sieg/Niederlage/Draw, einmalige Extraction-Abrechnung,
Assist-Grenzen 29/30 HP und acht Sekunden, Pass-Grenzen 399/400 cm und alle Caps,
veraltete Wurfereignisse, Disconnect/Join und Host/Client-Konsistenz. Bestehende
Kampf-, Core-, Karten- und Equipment-Regressionen müssen weiter bestehen.

Die Zahlen und Abnahmekriterien bleiben die Grundlage der Implementierung.
Bereits ausgewertete Läufe und ihre Grenzen sind im
[Implementierungsbericht](ECONOMY_IMPLEMENTATION.md#vorliegende-validierung)
aufgeführt. Die zugehörigen Nachweise liegen unter
[Validation/EconomyArmorAudio](Validation/EconomyArmorAudio). Zusätzliche Prüfungen
werden erst nach tatsächlicher Ausführung als bestanden ergänzt.
