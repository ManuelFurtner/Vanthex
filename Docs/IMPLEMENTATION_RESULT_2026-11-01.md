# Basic prototype implementation result

Implemented on 2026-11-01 in H:\Vanthex. Original source, configuration and docs
are backed up in `_versions/20261101_before-network`.

## Delivered

- Direct-IP listen-server hosting, joining and leaving from an Escape menu.
- Server input requests for shooting, reload, sprint and core actions; replicated
  health, ammo, objective state, character movement and round records.
- Server-owned shot traces and cosmetic shot multicast; local hit markers and damage feedback.
- Players replace bot roster slots; disconnect handling transfers the pawn to a bot.
- Bots search routes, see and shoot enemies, collect the core and approach extraction zones.
- Additional spawn cover on GRID, preserving the three-route prototype layout.
- Replicated door, light control and lift; local interaction with F.
- ARC remote Shift interaction with C, range/visibility checks and 15-second cooldown.
- Teammate camera after elimination; restoration on the next round.
- Regression checks for foreign core input, death/reload, wall collision and Shift reset/cooldown.
- Start-Vanthex.cmd and Run-Checks.ps1 for starting and repeatable verification.

## Verified

Editor and Game Development targets compile successfully. The latest regression
suite reports OVERALL PASS. Separate localhost host/client processes both report
PASS with six characters, one core and three devices. Both moved and consumed ammo;
the client received the server's 40-damage test and reported 60 HP. The final logs
contain no unsupported geometry-reference warnings or network failure signatures
searched by the audit. The original geometry-reference issue found during testing
was corrected using stable actor/component names.

The game was run with graphics enabled. Bots were observed moving, fighting,
carrying the core and completing rounds. The final Escape menu was visually
inspected at 1280x720. Global combat debug overlays were removed from the player UI.
Reports and detailed logs: `Saved/Audit-2026-11-01`.

## Practical limits

This is an initial playable mechanics prototype, not a production multiplayer release.
Six simultaneous human players, real internet latency/loss, reconnect edge cases,
long-session bot behavior and every moving-device collision case remain to be tested.
There is no lag compensation, public matchmaking or NAT traversal. The existing
runtime GRID arena remains procedurally constructed, not a saved authored map.
Bot aim, routes, cover and ability balance need playtesting. Final art/animation/audio
and a cooked standalone distribution were outside this mechanics implementation.
The local launcher uses the installed Unreal editor runtime.

See PLAYTEST.md for controls and acceptance scenarios.
