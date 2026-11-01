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
# VANTHEX — Architecture

Target: **Unreal Engine 5.x**, core gameplay in **C++**.

Blueprints are reserved for configuration, presentation, and content authoring. The first prototype should run from C++ classes and config, with temporary debug meshes.

## Status

Engine: **Unreal Engine 5.8.3** at `H:\epic\UE_5.8` (changelist 58210709, branch `++UE5+Release-5.8`). Editor binary: `H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe`. Project association is `5.8`.

Toolchain accepted by Unreal Build Tool: Visual Studio 2022 Build Tools 17.14 at `H:\VS2022BuildTools`, MSVC product version 14.44.35229, Windows SDK 10.0.22621.0, .NET Framework 4.8.1 developer pack. No Unreal MCP or editor-automation server is available. The test arena is built in C++ at runtime.

Both the game target and the editor target compile. Launch verification uses `UnrealEditor.exe -game -nullrhi`, not the monolithic `Vanthex.exe`. The cooked-style game executable cannot load the installed engine's asset registry and crashes on startup.

## Module

One game module, `Vanthex`, until a second module removes a real dependency cycle. Do not split plugins for systems that do not exist yet.

Implemented layout:

```
Source/Vanthex/
  Vanthex.Build.cs
  Vanthex.h/.cpp
  VanthexTypes.h
  VanthexDamage.h
  VanthexCharacter.h/.cpp
  VanthexPlayerController.h/.cpp
  VanthexGameMode.h/.cpp
  VanthexGameState.h/.cpp
  VanthexPlayerState.h/.cpp
  VanthexBotController.h/.cpp
  VanthexHUD.h/.cpp
  VanthexTestArena.h/.cpp
  VanthexTestTarget.h/.cpp
  VanthexCore.h/.cpp
  VanthexExtractionZone.h/.cpp
  HealthComponent.h/.cpp
  WeaponBase.h/.cpp
  HitscanWeapon.h/.cpp
```

`VanthexGameMode` owns the authoritative match: roster, round phase, timeout, elimination, extraction, and reset. It does not own health or weapon math. `VanthexGameState` is the match record the HUD reads: phase, score, round index, time remaining, round winner, match winner, core readout, and extraction time remaining. `VanthexPlayerState` is the player record: team, spawn slot, and alive flag. Those fields are marked replicated so a later listen server can carry them. There are no RPCs yet.

`AVanthexCore` is the physical objective. It stores state, carrier, and throw velocity. `AVanthexExtractionZone` is a box the game mode queries. The game mode decides when a hold completes and then calls the same `EndRound` used by elimination. See `Docs/CORE_SYSTEM.md`.

The local human is Alpha slot 0. `AVanthexBotController` fills the other five slots. Bots do not think or shoot. They exist so elimination and reset can be tested.

`VanthexPlayerController` owns player input routing and restart/test commands. It does not own movement simulation.

`VanthexCharacter` owns first-person presentation and movement: camera, WASD, mouse look, jump, crouch, sprint. It holds a `HealthComponent` and equips one weapon. It does not calculate weapon damage.

`HealthComponent` is the only place that stores health, applies damage, and broadcasts death. Actors that can be shot (the player and the test target) use this component instead of private health variables.

`WeaponBase` owns shared weapon state: magazine size, ammo in magazine, fire rate, reload time, and damage values. `HitscanWeapon` performs the trace and asks the damage path to resolve the hit. A second weapon type should be a new subclass, not a branch inside the rifle.

## Damage

One damage request type, resolved by `HealthComponent`:

- instigator
- damage amount
- hit result (location, component, bone or tagged shape when available)
- head/body classification

The weapon computes the final amount (`BodyDamage`, times `HeadshotMultiplier` for a head hit) and submits it. `HealthComponent` applies that number and does not multiply again. Before that submit, `VanthexGameMode::CanDamage` rejects friendly fire. Friendly fire is off. A target with no team, including the M1 practice dummy, can still be damaged. A hit is a head hit when the component is tagged `Head` or the bone name contains `head`. Anything else is body. The trace uses `ECC_Visibility`, ignores the owner pawn and the weapon, and has a range of 15000.

Death is health reaching zero. `OnDeath` is a non-dynamic multicast. Dead combatants stop moving, shooting, and reloading, and they stay that way until the next round. F5 calls `RestartTest`, which restarts the match in place. It does not reload the map and it does not respawn a single player during an active round. The engine's development binding of F5 to `viewmode shadercomplexity` stays removed in `Config/DefaultInput.ini`.

## Round flow

Phases are `Waiting`, `RoundStarting`, `RoundActive`, `RoundEnding`, `Intermission`, and `MatchEnded`. The game mode is the only writer. Each transition is published to the game state.

A round lasts 180 seconds. Ending a round by elimination, extraction, or timeout waits 1 second, then a 2 second intermission, then the next round. `PrepareForRound` restores health, movement, ammo, and spawn for every combatant, and the arena reset returns the core to center. First team to 7 round wins ends the match.

On timeout, the team with more living players wins the round. If the living counts are equal, the round is a draw and neither score changes. If both teams are fully dead, that is also a draw.

## Multiplayer constraint

Do not implement matchmaking, a dedicated server, or prediction in this milestone.

The gameplay is shaped so a later server can own it:

- Health changes go through `HealthComponent`, not scattered `SetHealth` calls.
- Hitscan confirms the hit, asks the game mode whether the teams allow it, then submits damage.
- Round score, phase, team identity, and the core readout are written by the game mode onto game state and player state.
- The core actor is the future replicated objective. Clients must not decide a catch or an extraction win.
- Input functions on the character and weapon are the future client entry points. Their bodies stay local for now.
- Tuning values live as editable properties, not magic numbers buried in trace code.

## Test environment

There is no saved gameplay map. `GameDefaultMap` is `/Engine/Maps/Entry`. `VanthexGameMode` builds the arena in `InitGame`. Alpha slot 0 stays at `(0, -1400, 200)` facing `+Y`, which is the original M1 spawn. The other two Alpha spawns are beside it. Bravo's three spawns are on the opposite side. The center shot lane is open for the practice dummy at `(0, 1200, 0)`. The core home is `(220, 0, 50)`, off that lane. Extraction A is around `(-850, 850)` and extraction B is around `(850, -750)`. Left and right walls leave a middle gap, and a raised catwalk runs along the left side. Cover sits off the center lane.

Visual Pass 03 keeps that rule. The hitscan debug line is not drawn. New dress is still no-collision. `-VanthexVisualShots` writes eighteen 1920×1080 frames. Limits and the missing external assets are in `Docs/HIGH_FIDELITY_PIPELINE.md`.

Input actions and the mapping context are created in C++ at runtime. Defaults: walk 600, sprint 900, crouch 280, jump Z 520, FOV 90, magazine 30 with an infinite reserve, fire interval 0.15 s, reload 1.6 s, body damage 40, headshot multiplier 2, max health 100. Controls: WASD, mouse, Shift sprint, Ctrl crouch (crouch wins), Space jump, left mouse fire, R reload, E pickup, G drop, Q throw, F5 match restart.

`-VanthexSelfTest` runs the M1 combat checks, the M2 roster, friendly fire, enemy damage, timeout, elimination, reset, score, and match checks, and the M3 core checks: spawn, pickup, single carrier, drop, death drop, throw, teammate catch, enemy intercept, settle, both extractions, extraction cancel, round reset, and F5. It writes `Saved/VanthexSelfTest.txt` and exits. The self-test shortens the round-end and intermission waits. It does not shorten the 180 second rule or the 5 second extraction hold. The timeout check calls the same timeout resolution the clock uses.

## Explicit non-goals

Operators, abilities, Shift, cosmetics, progression, ranked systems, spectator mode, and final art are not part of this milestone. The visual prototype is a presentation pass on the existing arena, not a new gameplay system. Spectator mode can later attach to a dead player who is already prevented from moving and shooting. Add the rest when its milestone starts.

## Correctness fixes — 2026-11-01

Drop and throw input now require the requesting character to be the current core carrier. Death cancels the weapon reload timer without refilling ammo. Thrown core movement sweeps a 20 cm sphere against WorldStatic geometry on each integration step and projects/damps velocity at impact; pawn catching remains handled separately. Editor build and existing headless M1/M2/M3 self-test passed after these changes. Visual collision edge cases still require playtesting.

