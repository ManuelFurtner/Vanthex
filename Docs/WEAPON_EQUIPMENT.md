# Rifle / knife equipment

Controls: **1** selects the rifle, **3** selects the knife. Left mouse fires the
rifle or performs one knife thrust per press. R reloads only the equipped rifle.
There is no automatic attack when drawing a weapon or holding an old input.

The prototype knife has a dark grip, guard and tapered metal blade. It is built
from engine meshes without a new binary asset dependency. `KnifeMesh` on the
Equipment component accepts an authored replacement (centimetres, grip pivot,
blade along local +X). The replacement currently receives the steel material.
First-person equipment uses animated transforms. ARC's third-person skeletal
hands follow the selected weapon through native IK, including the knife stance,
thrust and weapon change. Separate first-person arms, production knife art and
dedicated knife sound effects remain open.

## Rules and presentation

`UVanthexEquipmentComponent` owns selection, a replicated animation record and
server-side melee. Default switch time is 0.55 seconds: lower/rotate the old
weapon for half the time, replace it below the view, then raise the selected
weapon. Animation samples the replicated server clock on every viewer, including
late observers; there is no gameplay-authoritative client prediction. High latency
can delay the response or skip part of an animation. The existing rifle recoil
and sprint pivot remain separate from the holster/draw transform.

`GetPresentation()` samples that shared clock into `VisibleSlot`, `Lower` and
`Swing`. First-person equipment, ARC's world weapons and hand IK use this same
state. Camera-mounted knife parts use `OnlyOwnerSee`; the mesh-mounted world
knife and rifle use `OwnerNoSee`. Selection therefore shows one weapon per view,
with matching holster/draw poses and no hidden-weapon shadows.

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
When a bot takes over a human-controlled pawn holding a knife, it returns to the
rifle so its existing combat AI can continue firing.

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
`Run-Equipment-Network-Checks.ps1` additionally checks client-driven knife
selection, swing, restored rifle selection and absence of double weapons in
separate host/client processes.

The merged ARC/equipment implementation was verified with Unreal Engine 5.8.3:

| Check | Result |
|---|---|
| Editor and Game, Win64 Development | Both builds passed |
| Gameplay/equipment self-test | 64 PASS |
| Operator and bot integration | 34 PASS |
| GRID V3 geometry/navigation | PASS |
| GRID V3 gameplay | 34 PASS |
| GRID V3 host/client | Both PASS |
| Equipment host/client | Both PASS |
| Rendered inspection | Eight views completed; world knife ready/strike and first-person knife/rifle inspected |

Evidence is in [Validation/EquipmentMerge](Validation/EquipmentMerge), including
[equipment network host](Validation/EquipmentMerge/EquipmentNetworkHost.txt),
[client](Validation/EquipmentMerge/EquipmentNetworkClient.txt),
[world knife](Validation/EquipmentMerge/05_knife_ready.png) and
[first-person knife](Validation/EquipmentMerge/07_first_person_knife.png).
Network verification uses local processes; internet latency, late joins during
every animation phase and competitive balance remain separate acceptance work.
The economy and expanded weapon-audio documents describe planned systems.
