# Neue Zielrichtung — 2026-11-02

Das hybride Waffen-/Kaufsystem verbindet freie Operator-Fähigkeiten mit gekauften
Waffen, Rüstung und maximal zwei zusätzlichen Gadgets. AUG und MP7 haben Priorität.
Verbindliche Teamvorgaben und ausdrücklich vorläufige Ergänzungen stehen in
[ECONOMY_AND_WEAPONS.md](ECONOMY_AND_WEAPONS.md); Soundplanung und Hörtests in
[WEAPON_AUDIO_DESIGN.md](WEAPON_AUDIO_DESIGN.md).
Dieses System ist eine Design-Spezifikation und noch nicht implementiert.
Die folgenden Implementierungs- und Meilensteinaussagen beschreiben ältere Stände.

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
# VANTHEX — Game Design

Codename: **VANTHEX**

VANTHEX is a competitive 3v3 first-person tactical ability shooter.

## Pillars

1. **Gunplay** — precise FPS combat with a relatively low time to kill.
2. **Core** — both teams fight over a physical energy core that can be picked up, carried, dropped, thrown, caught, and intercepted.
3. **Shift** — players manipulate parts of the arena: doors, lights, lifts, bridges, cameras, and selected destructible or breachable areas.
4. **Synergy** — operator abilities interact with teammates and with the arena itself.

## Match structure

Matches consist of short rounds. Players do not respawn during an active round.

The first map is **GRID**.

## Operators (long-term, not in the current milestone)

| Operator | Role |
| --- | --- |
| VEX | Stealth / tracking |
| BULWARK | Breaching / defense |
| ECHO | Disruption / audio / EMP |
| PHANTOM | Mobility |
| WARDEN | Surveillance / intel |
| ARC | Hacking / map control |

## Current milestone

M3 adds the Energy Core on top of the M2 round. The rifle loop is unchanged. There is still no Shift, operator, or online matchmaking.

A neutral core spawns off the center shot lane. Either team can pick it up with E, carry it, drop it with G, or throw it with Q. A thrown core can be caught by a teammate or intercepted by an enemy. Holding either extraction zone for 15 seconds wins the round through the existing round score. Eliminating a team still wins the round as well. The carrier keeps normal movement and the rifle for this prototype.

A match is first to 7 rounds. Each round lasts 180 seconds. Players do not respawn while the round is active. If both teams are wiped by the same resolution, the round is a draw and the score does not change. If the timer expires, the team with more living players wins. Equal numbers of living players is a draw. F5 restarts the whole match, including the core. It does not respawn one player inside an active round.

The local roster is one human on Alpha and five standing bots so the 3v3 rules can be tested without combat AI.

## Defaults chosen for the prototype

- One temporary rifle. No operator kits.
- Temporary debug visuals only. No final art.
- Single local player. Networking is a design constraint on the code, not a feature of this milestone.
- A simple arena built at runtime, not a finished GRID layout.
- Walk 600, sprint 900, crouch 280, jump Z 520, FOV 90.
- Magazine 30, infinite reserve, fire interval 0.15 s, reload 1.6 s.
- Body damage 40, headshot multiplier 2 (80), player and target health 100.
- Head hit: component tag `Head`, or a bone whose name contains `head`. Otherwise body.
- Teams: Alpha and Bravo, 3 players each. Friendly fire is off.
- Round length 180 seconds. Round-end banner 1 second. Intermission 2 seconds. First to 7 rounds.
- Timeout: more living players wins; a tie is a draw and does not change the score.
- Core home (220, 0, 50). Pickup 200 cm. Throw 2000 cm/s. Catch 180 cm. Extraction hold 15 seconds. See `Docs/CORE_SYSTEM.md`.
