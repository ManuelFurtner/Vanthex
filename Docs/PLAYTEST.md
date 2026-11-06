# VANTHEX prototype controls and verification

The November 2026 implementation adds direct-IP listen-server play, server-owned
combat and objectives, objective bots, a GRID cover pass, three Shift devices and
the first ARC ability. This is a prototype slice, not matchmaking or a finished game.

## Start and connect

Open Vanthex.uproject with the installed Unreal Engine 5.8 and play, or launch
`H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe H:\Vanthex\Vanthex.uproject -game`.
VANTHEX opens the main menu. Select Core or Capture the Flag under Spielen,
then start a local match. Einstellungen opens video, audio, controls and crosshair
options. Netzwerk hosts the selected mode or joins by IP and optional port
(default 7777). Escape opens the session menu during a match. Direct internet play needs a reachable
host; no NAT traversal, account service or public matchmaking is implemented.
Joining players replace bots and inherit their current health and round state.
Disconnected players are replaced by bots. Leaving the host ends that session.

## Controls

- WASD / mouse: move and aim; Shift: sprint; Ctrl: crouch; mouse wheel: weapons; Tab (hold): scoreboard; Space: jump.
- Left mouse: fire; R: reload.
- E: pick up the core; G: drop; Q: throw.
- F: operate a nearby Shift device while looking toward it (250 cm).
- C: ARC remote operation (1200 cm, 15 second cooldown, requires line of sight).
- Escape: session menu. Online matches continue while the menu is open.
- F5: host/local match restart.

Shift: door at the right central route, light control near the center-left,
lift at the south end of the left catwalk. The lift and door move over time;
closing movement stops at a blocking actor. Devices and ability cooldowns reset
each round. Dead players automatically watch a living teammate when available.

## Automated checks

`-game -nullrhi -nosound -unattended -VanthexSelfTest` runs the original M1–M3
suite plus ownership, death/reload, wall collision and Shift regression checks.
It writes `Saved/VanthexSelfTest.txt` and exits with a failure status on failed checks.
The deterministic suite disables live bot behavior and extra combat cover.

`-VanthexNetworkProbe` is a separate development smoke test. Run one process with
`/Engine/Maps/Entry?listen -port=17777` and another with `127.0.0.1:17777`.
Each writes its own `Saved/NetworkHost.txt` or `Saved/NetworkClient.txt` after
checking roster, objective/device presence, team assignment, movement and ammo.
It disables bot decisions. It is not a substitute for a latency or six-player test.

## Manual acceptance checks

1. Host and join from separate machines; both see the same arena and six combatants.
2. Check movement, sprint/crouch transitions, jumping/landing and aiming at ordinary latency.
3. Shoot enemies and teammates: enemy health changes, friendly damage is rejected.
4. Die during reload; observe a teammate; return with full health on the next round.
5. Pass/intercept the core, throw against walls and platforms, extract at either zone.
6. Operate each Shift device locally and remotely; check range, visibility and cooldown.
7. Disconnect/rejoin during a round and after match end; check roster and score.
8. Let bots play several rounds; inspect navigation, combat and extraction behavior.
9. Verify the session menu, cursor capture, health/ammo HUD and feedback with graphics enabled.

Build and runtime results are recorded separately under `Saved/Audit-2026-11-01`.

## Expanded GRID (2026-11-01)

The default map is now 200 x 240 m with four outer power sectors, 24 additional
maintenance hideouts and five lifts. Use F at a power terminal to start a
four-second hack; remain nearby with line of sight. Damage or leaving cancels
it. The next completed hack restores power. C uses ARC remotely. Sector power
controls linked lifts and lighting. Each outer upper platform also has a ramp.
F operates an occupied lift while looking ahead and calls it from either landing.
See Docs/GRID_MAP.md for layout, controls, limitations and verification.


Space / Leertaste is the default jump key in Core and Capture the Flag. One grounded jump is allowed; release to clear the input. Jumping is blocked while crouched, dead, or outside active play.
