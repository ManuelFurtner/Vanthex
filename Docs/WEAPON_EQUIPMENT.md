# Rifle / knife equipment

Controls: **1** selects the rifle, **3** selects the knife. Left mouse fires the
rifle or performs one knife thrust per press. R reloads only the equipped rifle.
There is no automatic attack when drawing a weapon or holding an old input.

The prototype knife has a dark grip, guard and tapered metal blade. It is built
from engine meshes without a new binary asset dependency. `KnifeMesh` on the
Equipment component accepts an authored replacement (centimetres, grip pivot,
blade along local +X). The replacement currently receives the steel material.
The presentation uses animated transforms, not skeletal hands or animation
montages. First-person arms, production knife art and sound effects remain open.

## Rules and presentation

`UVanthexEquipmentComponent` owns selection, a replicated animation record and
server-side melee. Default switch time is 0.55 seconds: lower/rotate the old
weapon for half the time, replace it below the view, then raise the selected
weapon. Animation samples the replicated server clock on every viewer, including
late observers; there is no gameplay-authoritative client prediction. High latency
can delay the response or skip part of an animation. The existing rifle recoil
and sprint pivot remain separate from the holster/draw transform.

Switching cancels rifle fire and reload without refilling its magazine. Both
weapons are locked during the transition; repeated selection requests do not
restart it. Knife recovery cannot be bypassed by switching. The authoritative
rifle shot and reload entry points also reject use while holstered, including
calls from bots or an already-running fire timer.

Knife defaults: 160 cm camera trace, 50 damage, 0.14-second windup and 0.60-second
full thrust/recovery cycle. The server derives origin/direction from the owning
character at contact time. A single visibility trace hits the first surface;
walls and friendly players block it. No headshot bonus, ammo, bullet flyby audio,
or movement-speed bonus is added. Knife damage uses the existing health, team,
hit-marker, death and round-resolution paths.

The server requires a living, possessed character and an active round. Death
cancels a pending strike. New rounds reset to the rifle. Core input, carrying,
throwing, extraction and existing weapon values are unchanged.

## Verification

`-VanthexSelfTest` now includes equipment integration cases after the existing
match checks: reload interruption, switch spam, rifle lockout, completed draw,
delayed single damage, attack spam, recovery lockout, friendly fire, wall
occlusion, range, death during windup, round reset and reverse draw.

Build and run with an actual Unreal 5.8 installation:

```powershell
./Run-Equipment-Checks.ps1 -EngineRoot 'H:\epic\UE_5.8'
```

The runner builds Editor and Game and requires a fresh successful self-test.
Then use two game processes (Host / Join) for these manual acceptance checks:

1. Both players see rifle-to-knife and knife-to-rifle holster/draw, without both
   weapons appearing at once. Join while another player is switching.
2. Fire or reload, then switch. Ammo remains unchanged after cancellation;
   shooting cannot resume automatically after drawing the rifle.
3. Stab a nearby enemy twice, a teammate, a wall with an enemy behind it, and
   an out-of-range enemy. Only valid enemy contacts lose 50 HP each.
4. Die during windup, reset the match and replace a bot by joining. Verify no
   late strike, a usable rifle on reset, and inherited equipment on takeover.
5. Inspect crouching, sprinting, spectator view, rapid inputs and simulated
   network latency. Tune viewmodel framing in a rendered session.

Implementation environment: source-only checkout with LFS pointer assets.
Unreal Engine and its C++ build toolchain were not found locally. These new
engine tests and the rendered/network acceptance checks have **not been run**.
Do not treat this source change as a compiled or visually approved release.
