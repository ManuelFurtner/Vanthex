# VANTHEX — Core system

The Energy Core is the first signature objective. It is a physical object in the arena, not an inventory flag. `AVanthexCore` owns the object. `AVanthexGameMode` owns the round result. The HUD only reads the game state.

## States

`EVanthexCoreState`:

| State | Meaning |
| --- | --- |
| AtRest | On the ground or at the home spawn. Either team may pick it up. |
| Carried | Attached to one living carrier. |
| Thrown | In flight after a throw, a manual drop, or a death drop. |
| Extracting | Still held by the carrier, inside an extraction zone, timer running. |
| Extracted | The hold finished. The carrier's team has already been sent through the normal round win. |

The core also stores the current carrier, the thrower, a short pickup lock, the home location, and the current velocity.

## Pickup

E picks up the core. The player must be alive, the round must be active, the core must be `AtRest`, and the player must be within 200 cm. A player who just dropped it cannot pick it up again for 0.45 s. Only one carrier exists: pickup is rejected unless the core is free.

While carried, the core is attached to the carrier's capsule at a local offset of `(50, 0, 30)`. Its mesh does not collide, so it does not block movement or the rifle trace.

The carrier keeps normal movement: walk, sprint, and crouch. The carrier also keeps the rifle. That is a prototype choice so gunplay and the objective can be compared in the same round. It may change after playtesting.

## Drop

G drops the core. Ownership clears, the core is placed in front of the carrier, and it falls with a short forward velocity. Catching is disabled for a manual drop, so it settles on the floor and returns to `AtRest`.

## Death drop

If the carrier dies, the existing character death path calls `NotifyCombatantDied`. The game mode drops the core there. The core also drops on its own tick if the carrier is dead or destroyed. There is no second death system. A death drop can be caught. Either team can recover it.

## Throwing

Q throws the core along the carrier's view direction at a fixed speed of 2000 cm/s.

The flight is a deterministic kinematic projectile, not a Chaos physics body. Gravity is 980 cm/s². Each frame is substepped at 20 ms so a long frame cannot skip a catch. The core stops at floor height 40, loses horizontal speed, and becomes `AtRest` when that speed drops below 60. It bounces lightly off the arena bounds at ±2100.

This is the network-friendly choice: a later server can replicate state, carrier, velocity, and a timestamp without simulating a physics asset.

## Catching and interception

Catching is automatic. The first prototype does not require E. A living player within 180 cm of a thrown core becomes the carrier, and the flight stops. The closest player inside that radius wins if two overlap. Team does not matter, so a teammate can catch a pass and an enemy can intercept the same throw.

The thrower is ignored until the core has traveled 250 cm from the release point, so the throw does not snap straight back. Dead players cannot catch. A manual drop cannot be caught; it has to land first.

## Extraction

Two zones, A and B, sit in different parts of the greybox. Both teams may use either zone. There is no attacker and no defender.

During `RoundActive`, a Core inside either extraction volume starts a 15-second countdown for the team that last possessed it. The actual Core position is used, whether carried, flying or at rest. Dropping or throwing it inside the zone preserves progress; throwing it into the zone starts extraction. A same-team pickup preserves progress too. A carrier death preserves the dropped Core attribution and countdown if it remains inside.

Leaving the zone, changing extraction zones, or opponent possession resets progress. An opponent who picks it up starts a new countdown for their own team. The last possession team is replicated, survives release/disconnection, and is cleared by reset/recovery. An unclaimed neutral Core cannot extract. A server-side possession revision also prevents progress carrying across an intervening enemy possession. The HUD shows `EXTRACTING A` or `EXTRACTING B` and the remaining seconds for loose Cores as well.

The physical states remain `Thrown` and `AtRest` for a loose Core. Extraction does not freeze flight, disable catches, or prevent pickup. The held Core may still use `Extracting`; the objective timer and replicated GameState readout are independent of its physical state.

When the hold completes, the core becomes `Extracted` and the game mode calls the existing `EndRound` for the last possessing team. Score, round reset, and first-to-7 are unchanged. A deposited, team-owned Core inside a zone defers elimination and round timeout: its countdown continues even if the depositing team, the opposing team, or both teams are dead. Opponents must recover the Core to cancel that team's countdown. After recovery or departure from the zone, any deferred elimination is resolved on the next server clock tick. Without a deposited Core, normal elimination and timeout rules still apply.

Bots carry the Core toward a zone and prioritize nearby delivery over combat. They turn toward a short downward throw and check its swept flight and landing margin against map geometry before releasing it. If no safe landing is available, they move closer. Bots defend a friendly deposit without picking it up again, and prioritize recovering an enemy deposit.

The extraction timer is not updated unless the round is active, so a hold cannot start or finish during `RoundEnding`.

## Reset

Every new round, and F5, calls the arena reset. That puts the core back on its home at `(220, 0, 50)`, clears the carrier, last possessing team, velocity, throw, pickup lock, and extraction timer, and sets the state to `AtRest`. Home is offset from X = 0 so the core does not sit on the M1 shot lane. The core is reset before combatants are moved back to their spawns.

## HUD

The game mode writes `EVanthexCoreReadout` onto the game state: `CORE: CENTER`, `CORE: ALPHA`, `CORE: BRAVO`, `CORE: DROPPED`, `CORE: THROWN`, `EXTRACTING A`, `EXTRACTING B`. The local carrier also sees `YOU HAVE THE CORE`, `CORE CARRIED`, and their team. The HUD does not decide who scores.

## Edge cases

- A destroyed carrier is treated like a death drop.
- If the core leaves the arena (`|X|` or `|Y|` above 2300, or Z outside -100 to 2500) for 1 second while it is not being carried, it returns to center.
- If the round ends while the core is in the air or during a hold, the next round reset restores it.
- Two players touching a thrown core resolve to the closest living player.
- A dead player cannot pick up or catch.
- F5 while the core is carried clears ownership and restores the home spawn.

## Multiplayer authority

The game mode is the future server for objective results. It is the only writer of extraction progress and of the round win. The core actor is marked to replicate. The readout and the extraction time remaining are replicated on the game state. The carrier pointer is still a local reference; a later listen-server milestone should replicate the carrier as a player id rather than letting clients decide the catch or the score. No matchmaking and no RPC layer were added.

## Prototype tuning

| Value | Amount |
| --- | --- |
| Home | (220, 0, 50) |
| Pickup radius | 200 cm |
| Drop re-pickup lock | 0.45 s |
| Throw speed | 2000 cm/s |
| Gravity | 980 cm/s² |
| Catch radius | 180 cm |
| Thrower ignore distance | 250 cm |
| Floor rest | Z = 40 |
| Extraction hold | 15 s |
| Out-of-bounds recovery | 1 s |
| Zone A | (-850, 850), half extent (250, 250, 220) |
| Zone B | (850, -750), half extent (250, 250, 220) |
| Pickup / drop / throw | E / G / Q |
