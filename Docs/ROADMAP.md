# VANTHEX – Aktiver Plan

Stand 6. November 2026. Die zwölf Punkte werden in dieser Reihenfolge bearbeitet. Änderungen einzeln umsetzen, prüfen und pushen. Ein Punkt ist abgeschlossen, wenn die passende Abnahme belegt ist. Die Partien mit sechs bzw. zehn Menschen stehen aus, bis eine Testgruppe verfügbar ist.

1. [ ] **Echte Playtests.** Eine vollständige Core-Partie mit sechs und eine CTF-Partie mit zehn Menschen spielen. Todesorte, oft genutzte Waffen und wenig genutzte Wege protokollieren.
2. [ ] **Treffer bei höherem Ping.** Dieselben Schussserien mit bewegten Zielen und Dauerfeuer bei normaler und erhöhter Latenz sowie Paketverlust vergleichen. Ein begrenztes serverseitiges Zurückrechnen der Zielpositionen erst anhand der Messwerte planen.
3. [ ] **Waffenbalance.** Rückstoß, Streuung, Schadensabfall und Rüstung auf kurzen, mittleren und langen Distanzen vergleichen. Jede Waffe bekommt eine erkennbare Rolle.
4. [ ] **Bewegung und Animationen.** Sprint, Ducken, Sprung und Zielen mit Visier in Ego- und Fremdansicht prüfen. Nachladen, Landen und Waffenwechsel sollen sauber ineinander übergehen.
5. [ ] **Karten.** Beide GRID-Extraktionswege und die Zugänge zu beiden Flaggen auf Crossroads ablaufen. Engpässe, ungenutzte Wege und Tode direkt am Spawn erfassen.
6. [ ] **Räumlicher Ton.** Richtung, Entfernung und Stockwerk ohne Sichtkontakt erkennen. Schritte, Gadgets und Teamstimmen sollen im Kampf unterscheidbar bleiben.
7. [x] **Training.** Unter **Spielen → Solo-Training** stehen Ziele auf 10, 25 und 50 Metern, ein bewegliches Ziel und eine wiederholbare Core-Extraktion bereit.
8. [x] **Steuerung.** **Maus & Steuerung** bietet einen ADS-Faktor, 22 frei belegbare Aktionen, Konfliktprüfung und Rücksetzen auf Standard. Der Konfigurationstest prüft Speichern und Laden.
9. [x] **Team und Auswertung.** Per Mausrad-Klick den lebenden Mitspielern eine Fläche markieren. Nach dem Tod per Pfeiltasten das beobachtete Teammitglied wechseln. **Tab** zeigt Kills, Tode, Assists und gewertete Objektivaktionen.
10. [ ] **Verbindungen.** Einer privaten Lobby beitreten, Serverliste mit Ping und Modus nutzen und nach Abbruch denselben Platz wieder übernehmen.
11. [ ] **Leistung und Builds.** Bildraten und Frametimes auf beiden Karten mit gleicher Konfiguration vergleichen. Windows-Client und Linux-Server aus demselben Commit reproduzieren und zusammen verbinden.
12. [ ] **Serverprüfung und Moderation.** Ungültige Kampf- und Objektivaktionen serverseitig ablehnen. Meldungen absenden und zur Moderation prüfen können. Rangliste, Killcam und weitere Operatoren folgen danach.

## Fortschritt der ersten Etappe

- Der lokale Testlauf prüft Bot-Partien und Treffer auf ein bewegtes Ziel bei 0, 50, 100 und 150 ms simulierter RTT. Ein separater Lauf hat 100 ms RTT mit 2 % ausgehendem Paketverlust pro Seite geprüft. Das HUD kann die Netzwerkdaten unter **Grafik** einblenden.
- Der Waffenvergleich ermittelt ideale Körpertreffer auf 5, 20 und 45 Metern mit keiner, leichter und schwerer Rüstung. Die Werte dienen als Ausgangspunkt für das Balancing mit Menschen.
- Kleine Stufen lösen eine schwächere Landepose aus. Die CTF-Respawnwahl prüft auf belegte Startplätze und die Nähe von Gegnern. Die Lautstärke von Schritten und Ausrüstung hat einen eigenen Regler.
- Mit den Pfeiltasten links und rechts können ausgeschiedene Spieler lebende Teammitglieder beobachten. Die Teamübersicht zeigt außerdem Flaggenaktionen und gewertete Core-Pässe sowie Abfangaktionen.
- In einer Mehrspielersitzung können Spieler unter **Netzwerk → Spieler melden** einen Grund auswählen. Bestätigte Meldungen liegen zur manuellen Prüfung auf dem Spielserver unter `Saved/Moderation`.
- Der erste gerenderte Messlauf erfasste je 600 Frames mit fester Spawnansicht bei 1280 × 720, Grafikstufe Hoch und Direct3D 12 auf einer RTX 4070 SUPER. GRID V4 lag bei 10,36 ms im Median und 11,82 ms im 95. Perzentil. Crossroads CTF lag bei 6,08 ms und 6,86 ms.

Für denselben Grafiktest `$Map` auf `GridV4` oder `CrossroadsCTF` setzen und den Entwicklungsbuild mit den folgenden Argumenten starten. Der CSV-Profiler schreibt unter `Saved/Profiling/CSV`.

```powershell
& $Editor $Project "/Game/Vanthex/Maps/$Map" -game -d3d12 -RenderOffscreen -windowed -ResX=1280 -ResY=720 -ForceRes -unattended -nosplash -nosound -VanthexPerfCapture -ExitAfterCsvProfiling -csvCompression=0 -seconds=180
```

## Phase 1 – Anleitung für den Playtest

**Vorbereiten und verbinden.** Alle starten denselben Build. Für LAN im Hauptmenü unter **Spielen** den Modus wählen und unter **Netzwerk** auf **LAN-Partie hosten** klicken. Mitspieler tragen dort unter **Direkt beitreten** die erreichbare Adresse ein und klicken **Verbinden**. Bei abweichendem Port die Form `IP:Port` verwenden. Mit einer bereitgestellten Serveradresse direkt beitreten. Vor dem Start im Scoreboard prüfen, ob alle Plätze durch Menschen besetzt sind. Bot-Partien separat kennzeichnen.

| Partie | Ablauf und Beobachtungen |
|---|---|
| Aufwärmen, fünf Minuten | Laufen, Springen, Zielen, Nachladen und Waffenauswahl ausprobieren. Je eine kurze Schussserie auf ein stehendes und ein bewegtes Ziel. Probleme mit Eingabe, Trefferfeedback oder Ton notieren. |
| Core auf GRID V4, 3v3 | Eine vollständige Partie spielen. Beide Extraktionswege versuchen, den Core passen und abfangen. Bei einer Extraktion die 15 Sekunden beobachten. Tode nach Kartenbereich und gewählte Waffen zählen. |
| CTF auf Crossroads, 5v5 | Eine vollständige Partie spielen. Die Mitte und beide Flanken testen. Flaggenaufnahme, Übergabe nach Tod und Rückkehr zur eigenen Basis beobachten. Route jeder Eroberung und Tode nahe den Spawns zählen. Reguläre Spielzeit 15 Minuten, bei Gleichstand drei Minuten Verlängerung. |

Nach dem Match den automatischen Neustart beobachten. Bei einem geplanten Abbruch zwischen zwei Runden notieren, ob der Beitritt erneut klappt und welcher Platz zugeteilt wird.

**Protokoll je Partie.** Build, Karte, Modus, menschliche Spielerzahl, Dauer, Ergebnis und Objektivaktionen festhalten. Todesorte, Waffenwahl, Abschüsse pro Waffe, Routen und Tode nahe dem Spawn nach Team zählen. Bei Auffälligkeiten Runde oder Spielzeit, beteiligte Waffe, Entfernung, beobachtetes Verhalten und erwartetes Ergebnis notieren. Ping, Paketverlust und Frametimes ergänzen, falls gemessen. Jede Person nennt danach die stärkste und die unklarste Situation.

**Technische Vorprüfung.** Nach dem Build im Projektverzeichnis in PowerShell ausführen.

```powershell
.\Run-Playtest-Checks.ps1 -Scope All
```

Der Runner prüft Core, CTF, Solo-Training und die Trefferbestätigung ohne Spielfenster. `-Scope Core`, `-Scope CTF`, `-Scope Training` und `-Scope DamagePrediction` wählen einzelne Prüfungen aus. Die Ergebnisse stehen pro Lauf in `Saved/PlaytestChecks/<run>/Summary.json`. Spielbalance und Kartenfluss werden in den menschlichen Partien abgenommen.

**Matchbericht.** Mit `-VanthexPlaytestReport` auf dem Host oder Server schreibt ein Entwicklungsbuild am Matchende eine CSV-Datei unter `Saved/Playtests`. Sie enthält Ergebnis, Dauer, belegte Spielerplätze, serverseitig bestätigten Waffenschaden, Abschüsse und Todesorte. Positions- und Ausrüstungsproben werden einmal pro Sekunde für lebende Figuren erfasst. Die Positionszellen messen auf jeder Achse vier Meter und verwenden Weltkoordinaten. Proben und Abschüsse umfassen auch Bots. Der Bericht erfasst keine Namen oder IP-Adressen.

## Historische Entwicklungsnotizen

# Neue Planung — 2026-11-02

Nächster geplanter Slice: hybrides Kaufsystem, Standardpistole/AUG/MP7,
Rüstung und serverseitige Wirtschaftsereignisse nach
[ECONOMY_AND_WEAPONS.md](ECONOMY_AND_WEAPONS.md). Danach Gadget-Wirkungsprofile
und Umsetzung. Soundentwicklung folgt [WEAPON_AUDIO_DESIGN.md](WEAPON_AUDIO_DESIGN.md).
Die Spezifikation ist vorhanden; diese Funktionen sind noch nicht implementiert.
Die darunterstehenden Meilensteine sind historische Entwicklungsnotizen.

# Current implementation — 2026-11-01

The user authorized the complete basic prototype slice. The historical milestone
notes below describe the earlier local prototype. Current implementation includes:

- Listen-server direct-IP sessions, server-routed combat/core input, replicated health,
  ammo, core state and existing round records; server-owned shot origin and damage.
- Six roster slots with bot replacement on join and bot takeover on disconnect.
- Objective bots with grid route search, line-of-sight combat and extraction goals.
- GRID spawn cover and three routes, plus a door, light control and lift.
- ARC remote Shift activation with range, line of sight and cooldown validation.
- Escape session menu, teammate view after death, hit markers and damage feedback.

See PLAYTEST.md for controls, verification and limitations. This is a prototype:
no matchmaking, NAT traversal, lag compensation, polished operators/art or packaged
release is claimed. Two-process localhost checks do not certify internet latency
or six-human balance. Original M1–M3 tests and new regressions run through Run-Checks.ps1.

---
# VANTHEX — Roadmap

Milestones are small and testable. Do not start a milestone until the previous one compiles and its loop has been checked.

## M0 — Engine project

**Done** for compile and headless launch. Unreal Engine 5.8.3 is installed. The C++ project `Vanthex` is in this workspace. `VanthexEditor` Win64 Development links, and `UnrealEditor.exe -game -nullrhi` loads `/Engine/Maps/Entry` and spawns the prototype. A windowed editor session without `-nullrhi` has not been opened.

## M1 — First playable combat loop

**In-engine self-test still passes** after M3, including the F5 match restart. Keyboard movement, mouse look, and the on-screen HUD still need a manual playtest.

## M2 — 3v3 round system

**In-engine self-test still passes** after M3. Local 3v3 roster, friendly fire off, enemy damage, elimination, round reset, score, timeout draw, and first to 7.

## M3 — Energy Core

**In-engine self-test passed** (`Saved/VanthexSelfTest.txt`, 2026-10-03). The core can be picked up, carried, dropped, thrown, caught, and intercepted. Extraction at either zone wins the round through the existing score path. Elimination still wins a round as well. The next round and F5 put the core back at center.

This milestone was pulled forward. Listen-server movement was the previous M3 and is now M4. It has not been started.

Done when:

- The core spawns at center at the start of every round.
- Either team can carry it, and only one player can hold it.
- Death drops it. A throw can be caught by a teammate or intercepted by an enemy.
- Holding extraction A or B for 15 seconds scores the round.
- The full M1 and M2 checks still pass.

## Visual Prototype Pass 01 — GRID presentation

Presentation only. Not a gameplay milestone, and not M4.

The test arena was dressed as GRID with lighting, three materials, a simple core cage, and two extraction stations, all in C++ around the existing collision. Gameplay numbers and block transforms were not retuned. The full M1/M2/M3 self-test passed. The result was a readable industrial prototype made of engine primitives.

## Visual Pass 02 — modular facility kit

Presentation only. Not a gameplay milestone, and not M4.

GRID gained a generated modular kit, a shared surface shader, floor decals, a core cage, and transfer machines. Dress meshes do not collide. The full M1/M2/M3 self-test passed. The hall was still obviously built from boxes.

## Visual Pass 03 — high-fidelity push (current)

Presentation only. Not a gameplay milestone, and not M4.

The green hitscan line is gone. Firing feedback is ballistic. The kit modules are deeper, the floor has metal plates, the ceiling has duct and pipe runs, and the transfer bases have arms. Gameplay collision and the M1/M2/M3 numbers are unchanged. The full self-test still passes. 1920×1080 captures are in `Saved/VanthexShots` (`01_alpha_spawn` through `18_combat`). The gap list is `Saved/VisualPass03_GapAnalysis.txt`. What the project can and cannot build is in `Docs/HIGH_FIDELITY_PIPELINE.md`.

## M4 — Listen-server movement

Character movement, look, stance, and sprint replicate on a listen server between two clients. No new weapons. Do not start this until the current prototype has been playtested. This milestone has not been started.

## M5 — Authoritative hitscan

Server confirms the shot and applies damage. Clients see debug hit feedback. A client cannot set its own health or the target's health directly.

## M6 — GRID blockout

Replace the test box arena with a playable GRID blockout: spawns for two sides of three, cover, lanes, a core spawn, and two extraction zones. Still no Shift.

## M7 — Shift, first verbs

A small set of arena controls on GRID (door, light, one lift or bridge). Networked and usable without operators.

## M8 — First operator slice

One operator only, chosen when the core loop is stable. Ability must interact with a teammate or with Shift. Do not add the full roster in one step.

Later, still unordered until the slice above is fun: remaining operators, ranked, matchmaking, cosmetics, progression, final UI, animation, and audio.
