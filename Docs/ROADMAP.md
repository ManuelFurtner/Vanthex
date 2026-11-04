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
