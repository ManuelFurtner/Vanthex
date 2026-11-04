# GRID V4 — AEGIS

New playable level: `/Game/Vanthex/Maps/GridV4`. This plan precedes the geometry build. GRID V3, GRID and Foundry are retained unchanged. Coordinates and topology live in `VanthexGridV4Layout.h`.

## Design

A branching underground containment facility, with a double-height central reactor, west security/data/freight wing, east cryogenics/power wing, lower coolant/service circuit and upper observation/transfer circuit. 25 distinct spaces, three elevations (-600, 0, +600 cm), six independent stair connections. No shared rectangular foundation. Narrow corridors and broad, individually furnished rooms create different combat rhythms.

Alpha and Bravo each approach the Core through a dogleg and a screening room. Extra flank routes allow either side to rotate. Main routes retain standing clearance; substantial machinery provides real collision cover, while small decorative objects do not block bullets or passage.

## Extraction is earned through movement and interception

A is in the upper armored transfer room; B is in the lower isolation room. Both are shared by both teams. Neither is adjacent to the Core or connected by a single main-level corridor. Each site has two separate doors, a checkpoint approach and a service/gantry flank; neither can be defended from one invulnerable corner. The active extraction footprint is recessed inside the room. The 15-second hold remains unchanged.

Target: at least 75 metres of measured supported walking route from Core to either site, blocked direct Core-to-site sightlines, several interception corners and a mandatory floor transition on walking routes. The open Core pass well deliberately permits a risky lower-level drop/pass. Shorter physical drop routes must be evaluated separately, not misrepresented as walking routes. Spawn-to-Core mean route difference target: at most 10 percent. Values are design targets until actual collision tests pass.

## Spaces

| ID | Room | Floor | Center X,Y | Size X,Y |
|---|---|---|---|---|
|0|ALPHA / SERVICE AIRLOCK|0|-5000, -1200|1200, 1200|
|1|SECURITY SCREENING|0|-3000, -1200|1400, 1200|
|2|DATA VAULT|0|-2400, 1000|1400, 1600|
|3|AEGIS / CORE CONTAINMENT|0|0, 0|2400, 2400|
|4|CRYOGENIC RESEARCH|0|2800, -1000|1600, 1400|
|5|BRAVO / SECURITY CONTROL|0|3000, 1200|1400, 1000|
|6|BRAVO / DEPLOYMENT|0|5000, 1200|1200, 1200|
|7|FREIGHT PROCESSING|0|-4800, 1500|1200, 1600|
|8|TURBINE POWER|0|3000, 3000|1800, 1200|
|9|DECONTAMINATION|0|-1800, -3300|1600, 1000|
|10|WEST SERVICE|-600|-4800, -2800|1400, 1000|
|11|PUMP STATION|-600|-2200, -1600|1600, 1000|
|12|CORE UNDERCROFT|-600|0, 0|1800, 1800|
|13|COOLANT TRANSFER|-600|1900, -2200|1400, 800|
|14|B / QUARANTINE CHECKPOINT|-600|3600, -3600|1200, 800|
|15|B / ISOLATION EXTRACTION|-600|5400, -4100|1600, 1200|
|16|EAST SERVICE BYPASS|-600|5200, -2000|1600, 800|
|17|OBSERVATION|600|-400, 400|800, 800|
|18|SUSPENDED BRIDGE|600|1100, -500|1400, 600|
|19|RESEARCH CONTROL|600|2800, 1000|1400, 800|
|20|NORTH MANIFOLD|600|600, 3000|1600, 1000|
|21|A / TRANSFER CHECKPOINT|600|-2000, 4000|1200, 1000|
|22|A / ARMORED TRANSFER|600|-4200, 4800|1600, 1200|
|23|WEST GANTRY|600|-4300, 2800|1400, 1000|
|24|UPPER ROTUNDA|600|-2000, 1600|1400, 1000|

## Connections

All corridors are 400 cm wide. Exact bends are recorded in the C++ topology. The list below includes the collision-validation refinements to the original plan.

- 00: 2 → 24: 
- 01: 8 → 20: 
- 02: 4 → 18: 
- 03: 10 → 7: 
- 04: 13 → 9: 
- 05: 16 → 6: 
- 06: 3 → 4: (0, 0, 0) → (0, -600, 0) → (1800, -600, 0) → (1800, -1000, 0) → (2800, -1000, 0)
- 07: 3 → 5: (0, 0, 0) → (0, 400, 0) → (2200, 400, 0) → (2200, 1200, 0) → (3000, 1200, 0)
- 08: 4 → 5: (2800, -1000, 0) → (3000, -1000, 0) → (3000, 1200, 0)
- 09: 5 → 6: (3000, 1200, 0) → (5000, 1200, 0)
- 10: 5 → 8: (3000, 1200, 0) → (3000, 2200, 0) → (3000, 3000, 0)
- 11: 6 → 8: (5000, 1200, 0) → (5000, 3000, 0) → (3000, 3000, 0)
- 12: 1 → 9: (-3000, -1200, 0) → (-3000, -3300, 0) → (-1800, -3300, 0)
- 13: 9 → 4: (-1800, -3300, 0) → (-1800, -4200, 0) → (2800, -4200, 0) → (2800, -1000, 0)
- 14: 10 → 11: (-4800, -2800, -600) → (-4800, -1600, -600) → (-2200, -1600, -600)
- 15: 11 → 12: (-2200, -1600, -600) → (-2200, 0, -600) → (0, 0, -600)
- 16: 12 → 13: (0, 0, -600) → (1900, 0, -600) → (1900, -2200, -600)
- 17: 10 → 12: (-4800, -2800, -600) → (-4800, 500, -600) → (-2600, 500, -600) → (-2600, 0, -600) → (0, 0, -600)
- 18: 11 → 13: (-2200, -1600, -600) → (-2200, -2700, -600) → (1900, -2700, -600) → (1900, -2200, -600)
- 19: 13 → 14: (1900, -2200, -600) → (3600, -2200, -600) → (3600, -3600, -600)
- 20: 14 → 15: (3600, -3600, -600) → (4400, -3600, -600) → (4400, -4500, -600) → (5400, -4500, -600) → (5400, -4100, -600)
- 21: 13 → 16: (1900, -2200, -600) → (1900, -1300, -600) → (5200, -1300, -600) → (5200, -2000, -600)
- 22: 16 → 15: (5200, -2000, -600) → (5800, -2000, -600) → (5800, -4100, -600) → (5400, -4100, -600)
- 23: 14 → 16: (3600, -3600, -600) → (4400, -3600, -600) → (4400, -2000, -600) → (5200, -2000, -600)
- 24: 17 → 18: (-400, 400, 600) → (-600, 400, 600) → (-600, -500, 600) → (1100, -500, 600)
- 25: 18 → 19: (1100, -500, 600) → (2800, -500, 600) → (2800, 1000, 600)
- 26: 19 → 20: (2800, 1000, 600) → (2800, 3000, 600) → (600, 3000, 600)
- 27: 20 → 24: (600, 3000, 600) → (600, 2000, 600) → (-2000, 2000, 600) → (-2000, 1600, 600)
- 28: 24 → 17: (-2000, 1600, 600) → (-2000, 400, 600) → (-400, 400, 600)
- 29: 24 → 21: (-2000, 1600, 600) → (-2000, 3000, 600) → (-2000, 4000, 600)
- 30: 21 → 22: (-2000, 4000, 600) → (-2000, 5000, 600) → (-4200, 5000, 600) → (-4200, 4800, 600)
- 31: 24 → 23: (-2000, 1600, 600) → (-4300, 1600, 600) → (-4300, 2800, 600)
- 32: 23 → 22: (-4300, 2800, 600) → (-5000, 2800, 600) → (-5000, 4000, 600) → (-4600, 4000, 600) → (-4600, 4800, 600) → (-4200, 4800, 600)
- 33: 23 → 21: (-4300, 2800, 600) → (-3300, 2800, 600) → (-3300, 4000, 600) → (-2000, 4000, 600)
- 34: 2 → 2: (-2400, 1000, 0) → (-2600, 1000, 0) → (-2600, -800, 0) → (-3300, -800, 0) → (-3300, -600, 0)
- 35: 24 → 24: (-2000, 1600, 600) → (-3300, 1600, 600) → (-3300, 1400, 600)
- 36: 8 → 8: (3000, 3000, 0) → (2200, 3000, 0) → (2200, 700, 0) → (1700, 700, 0) → (1700, 900, 0)
- 37: 20 → 20: (600, 3000, 600) → (1700, 3000, 600) → (1700, 2900, 600)
- 38: 4 → 4: (2800, -1000, 0) → (2800, -2800, 0) → (1000, -2800, 0)
- 39: 18 → 18: (1100, -500, 600) → (1000, -500, 600) → (1000, -800, 600)
- 40: 10 → 10: (-4800, -2800, -600) → (-5800, -2800, -600) → (-5800, -2100, -600)
- 41: 7 → 7: (-4800, 1500, 0) → (-5800, 1500, 0) → (-5800, -100, 0)
- 42: 13 → 13: (1900, -2200, -600) → (1900, -3900, -600) → (-700, -3900, -600) → (-700, -3600, -600)
- 43: 9 → 9: (-1800, -3300, 0) → (-1800, -1600, 0) → (-700, -1600, 0)
- 44: 16 → 16: (5200, -2000, -600) → (6200, -2000, -600) → (6200, -1800, -600)
- 45: 6 → 6: (5000, 1200, 0) → (6200, 1200, 0) → (6200, 200, 0)

## Vertical links

- S1: 2 → 24, (-3300, -600, 0) → (-3300, 1400, 600); 20 m horizontal run, 6 m rise, three properly oriented visible stair flights.
- S2: 8 → 20, (1700, 900, 0) → (1700, 2900, 600); 20 m horizontal run, 6 m rise, three properly oriented visible stair flights.
- S3: 4 → 18, (1000, -2800, 0) → (1000, -800, 600); 20 m horizontal run, 6 m rise, three properly oriented visible stair flights.
- S4: 10 → 7, (-5800, -2100, -600) → (-5800, -100, 0); 20 m horizontal run, 6 m rise, three properly oriented visible stair flights.
- S5: 13 → 9, (-700, -3600, -600) → (-700, -1600, 0); 20 m horizontal run, 6 m rise, three properly oriented visible stair flights.
- S6: 16 → 6, (6200, -1800, -600) → (6200, 200, 0); 20 m horizontal run, 6 m rise, three properly oriented visible stair flights.

## Art direction and construction

PBR factory kit meshes form the visible shell: darker slate floors, warm off-white concrete, charcoal steel framing, restrained cyan containment light, amber utility light and green/violet extraction identifiers. Native material texture/normal/roughness maps are retained through material-instance colour adjustments. The Core volume gets diagonal corner bulkheads, deep ribs, suspended machinery and an upper bridge. Labs receive separated work cells and robotic equipment; data rooms receive rack banks, cooling and monitor faces; freight and extraction areas use believable handling machinery and cover. Corridors get portal frames, cable trays, service bands and directional wayfinding.

Lighting will emphasize pools around functional fixtures rather than uniformly bleaching every surface. Shadowed key lights establish depth; smaller fills preserve opponent readability. Instancing, bounded light radii and LODs limit cost. Rendered screenshots and measured frame intervals will be recorded; no unmeasured FPS promise.

Future SHIFT locations (two lift landings, a power terminal, a breach panel and per-room lighting sectors) remain integration sockets, as in the original brief. Existing Core, combat, equipment, armor, gadgets, crouch, ADS, bots and HUD remain operational.

## Acceptance

Build and bake the new level; test both spawns, all rooms and corridor bends, every stair up/down, 15-second extraction at both elevations for both teams, Core pickup/drop/throw/catch/death-drop, no direct spawn sightlines, actual route lengths and return routes. Capture Core, lab, data vault, power, maintenance, observation, both extraction sites, both approaches and first-person views. Perform a correction pass from the renders. Make the new map the default only after validation; preserve launchers for older levels.

## Repair pass — 4 November 2026

The room coordinates and corridor graph above remain unchanged. The Core now has a compact inner containment bay: staggered 420 cm opaque baffles, four offset entrances and a supported canopy at 420–444 cm. The intended lower pass aperture remains open onto playable floor. Stair mouths alone open the outer shell; walls below ramps and bounded side collision close the former void exits. Continuous sloped cladding replaces the temporary stepped side panels. Upper rails require actual safe floor below.

Furniture placement now requires floor support and a clear footprint. Ceiling services and the ring have structural attachments. Room/corridor lighting is brighter, emissive intensity and bloom are reduced, and fixtures attach to real ceilings. Final bake: 11,802 instances, 27 kit groups, 1,753 collision proxies, 194 lights, no missing meshes. See GRID_V4_VALIDATION.md for current evidence and test limits.
