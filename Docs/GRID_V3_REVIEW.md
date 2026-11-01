# GRID V3 — rendered review

## Capture protocol

Use the separate `/Game/Vanthex/Maps/GridV3` level with `-GridV3Shots` in a rendered game session. The capture callback positions the real player camera at a height of 165 cm above the relevant floor, waits two 4-second timer intervals, and then requests a PNG. The first callback starts 12 seconds after BeginPlay. A complete 15-view tour takes approximately 3 minutes 15 seconds; allow at least 300 seconds, and more on the first shader/texture preparation run.

The screenshot harness disables movement/collision for camera placement and suppresses bots. It is a visual inspection tool, not evidence of navigability, collision, combat balance or multiplayer performance. Those require the separate gameplay checks. Scenic views hide the rifle/HUD; views 13 and 14 retain the actual first-person weapon and HUD.

Output: `Saved/GridV3Shots/`. Stable filenames are listed below. `-GridV3Shot=N` captures only one of the numbered views for a correction check. `capture_timing.csv` records the elapsed wall time and actual engine frame count during each camera's settling window. These measurements include streaming and camera changes, and do not establish a combat FPS target or GPU percentile result.

Do not judge the first uncached run while shader compilation, distance-field generation or texture streaming is still active. Preserve each reviewed pass in a separate directory; use unique copied image paths when viewing an overwritten capture.

## Required views and inspection criteria

| File | View | Inspect |
| --- | --- | --- |
| `01_core_chamber.png` | Main Core chamber | Containment machinery, objective readability, distinct openings, real room volume, upper/lower sightlines |
| `02_server_room.png` | Server vault | Rack/cooling identity, readable access aisles, cable detail, moderate emissive lighting |
| `03_research_laboratory.png` | Laboratory | Equipment/workstations/partitions, plausible human scale, clear door openings |
| `04_power_distribution.png` | Electrical control | Cabinets/machinery/cable logic, warm sector identity, no props across route |
| `05_maintenance_tunnel.png` | Lower maintenance | Continuous floor and ceiling, pipes off the player silhouette, readable turns/exits |
| `06_upper_catwalk.png` | North catwalk | Supported platform, rail height, connections, cover, usable passage width |
| `07_observation_room.png` | Observation | Window/balcony outlook, control equipment, no wall clipping, high-ground counterplay |
| `08_extraction_a.png` | Reinforced transfer A | Industrial transfer identity, readable extraction pad, at least two approaches |
| `09_extraction_b.png` | Technical containment B | Different identity from A, restrained color, defensible approaches with counterplay |
| `10_alpha_approach.png` | Alpha deployment approach | Clear route choices, scale, direction cues, no direct spawn-to-spawn view |
| `11_bravo_approach.png` | Bravo deployment approach | Comparable route readability and cover, sector distinction |
| `12_connected_rooms.png` | Server-to-Core connection | Actual doorway sequence and distinct rooms, no accidental sealed opening |
| `13_first_person_core.png` | First-person Core fight | Rifle/HUD present, player-height cover, clear objective and enemy-height contrast |
| `14_first_person_bridge.png` | First-person vertical fight | Rifle/HUD present, rail/throw opening, upper-to-lower visibility |
| `15_generator_hall.png` | Lower generator | Functional machinery, distinct lower-level identity, supported architecture |

For every view check blown-out lights, crushed dark areas, default checker materials, floating/intersecting props, blank walls, repetitive floor tiling, awkward scale, blocked doors, missing walls/ceilings and confusing navigation. A camera in a solid prop is a capture defect and must be repositioned before evaluating that room.

## Kit integration checks

The source kit remains in `FactoryEnvironmentCollect`; project references use the original `/Game/Meshes`, `/Game/Materials` and `/Game/Textures` paths. Mesh bounds were read from `Saved/FactoryBounds.tsv` rather than assuming centered pivots.

- `SM_ControlRoom` starts 944 cm above its pivot and `SM_ControlRoomInterior` starts 981 cm above it. Bounds correction is essential.
- `SM_LargeRoofMerged01` extends 527 cm below its pivot and spans 2400 cm on its long axis; do not apply a generic flat-roof transform.
- `SM_Stair_01` rises 455 cm over about 526 cm, with a roughly 212 cm width. Scaling to the 600 cm storey height must preserve usable width and a collision surface that the character can climb.
- `SM_ElectricalPanel_01` has a vertically centered pivot. Place its lower bound on the intended support height.
- Instanced meshes need their materials' instancing usage compiled. Inspect the rendered session log for `missing usage flag`, `Failed to compile`, material fallback, texture-pool or video-memory warnings.
- The player camera explicitly clamps exposure to 1.0 in `VanthexCharacter.cpp`; review the effective in-game image, not only scene post-process settings.
- Signs need to survive the map bake. Avoid mirrored two-sided duplicate text on one plate.

## Evidence and correction ledger

Status: rendered construction and correction passes completed. All 15 first-pass images were inspected; the final set contains all 15 requested views. Corrected Core/server views and both final extraction close-ups were inspected again. Images are in `Saved/GridV3Shots/final/` and the gallery is `Docs/GRID_V3_GALLERY.html`.

Before the first rendered pass, camera positions were reconciled with the final prop placement: Power and Bravo follow their current room centers; Server/Lab/Generator use clear aisles; the catwalk camera clears the stairwell side wall; the Observation and first-person bridge views aim over the guardrail rather than into the platform floor. This static placement review does not replace inspection of the resulting PNGs.

Evidence: `Saved/GridV3-render-pass1.log`, `GridV3-render-pass2.log`, `GridV3-render-final.log`, and `GridV3-extraction-final-8.log` / `-9.log`. Each launch used `/Game/Vanthex/Maps/GridV3 -game -GridV3Shots`; the two final objective captures additionally used `-GridV3Shot=8` or `9`. Before-images remain in `Saved/GridV3Shots/pass1` and `pass2`.

Corrections verified in rendered images: reduced excessive lighting, bounded signs on contrasting backplates, added bridge bearers, repaired the roof above the passing opening, added server cooling/screens and corrected their facing. A trial gray floor material and intersecting wall skins were rejected after rendering; authored PBR floors were restored and the interfering skins removed. Scenic deployment captures hide diagnostic roster characters. First-person views retain the rifle/HUD, with inactive SHIFT controls removed for V3. Extraction uprights now use matte world-space coated steel instead of a stretched kit atlas.

Remaining visual limitations: legacy extraction bases and some gate faces remain very dark; old character/weapon models are still prototypes, some industrial wall repetition remains, and sign visibility depends on the viewing angle. These are recorded cosmetic limitations, not claims of finished commercial art. Capture timing is an uncontested, capped camera tour and is not a combat performance benchmark.

Gameplay automation and route measurements are recorded separately; a screenshot does not prove an objective can be reached or an extraction can complete.
