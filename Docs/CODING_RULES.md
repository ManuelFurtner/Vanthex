# VANTHEX — Coding Rules

## Structure

- Keep systems in small types with one responsibility. `HealthComponent` owns health. Weapons own firing. The character owns movement and presentation. The game mode owns match/test flow.
- Do not grow a god class that owns input, weapons, health, and rules together.
- Prefer one game module until a split removes a real dependency problem.
- Gameplay code depends on engine modules it uses, not on future features. Do not include headers for operators, objectives, or networking layers that are not part of the current milestone.
- Name types and files after what they are: `HitscanWeapon`, not `Manager` or `System`.

## Multiplayer

- Design gameplay state so a later server can own it. Damage, death, ammo changes, and round state go through a single function each.
- Do not apply gameplay-authoritative changes from cosmetic or debug code.
- Client input may run locally in the prototype. Leave a clear function boundary where the server will later validate the action.
- Replication and RPCs are now in scope. Validate changes with the local regression suite and separate host/client checks.

## Data

- Put tunable combat and movement values on `UPROPERTY` (EditDefaultsOnly / config) so they can be changed without hunting through trace math.
- Keep content references (mesh, map, weapon class) configurable on the game mode or weapon class. No final assets required.

## Decisions

- When a choice affects later milestones (trace channel, damage entry point, where input lives), record it in `Docs/ARCHITECTURE.md` in a few sentences.
- Do not add speculative types, stubs, or "for later" branches. The roadmap is the list of later work.

## Build

- Compile after each meaningful gameplay change.
- Do not hide compiler errors with disabled warnings, stubbed-out calls, or commented-out code that leaves the feature looking finished.
- Fix errors caused by the current change before adding the next system.
- Do not claim a behavior works unless it was compiled and, where tools allow, actually run. Separate verified results from steps that still need a manual playtest.

## Scope

- The current authorized scope is the basic M4–M8 prototype slice documented at the top of `Docs/ROADMAP.md`.
- Blueprints only when they are the practical place for a tunable, a widget, or a placed content reference.
- Debug meshes, debug draws, and log lines are the correct presentation until an art milestone exists.
