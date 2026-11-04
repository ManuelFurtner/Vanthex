# GRID V3 — MERIDIAN research facility

This topology was authored before the new geometry builder. It defines a new 23-space underground facility, not a modification of GRID or Foundry. The authoritative machine-readable coordinates are in `Source/Vanthex/VanthexGridV3Layout.h`. Units are centimetres. Room centers reference their walking floor. The build uses a separate map asset controlled by the V3 bake command.

## Architecture and circulation

The irregular building has twelve main-floor rooms, six lower maintenance spaces and five upper rooms/platforms. Rooms are separated by narrow, doglegged 400 cm corridors. There is no rectangular shared foundation. A room/corridor occupancy union creates its exterior shell and door openings. Intersecting routes join deliberately instead of producing overlapping sealed boxes. Main/lower/upper floor elevations are 0/-600/+600. Most ceilings are 420 cm above their floor; the Core containment volume rises to1320 cm. Corridor/room widths allow two players to pass.

| ID | Space | Center X,Y | Floor | Plan size X×Y |
|---|---|---|---|---|
|0|Alpha deployment|-3600,-2400|0|1200×1000|
|1|Security checkpoint|-1900,-2400|0|1200×1000|
|2|Server vault|-1800,-600|0|1200×1200|
|3|Transfer storage|-3600,0|0|1200×1400|
|4|Extraction A / reinforced transfer|-3200,2200|0|1600×1200|
|5|Medical analysis|-1300,2200|0|1000×1000|
|6|Core containment|0,0|0|1800×1800|
|7|Research laboratory|2200,-600|0|1600×1200|
|8|Extraction B / quarantine|3800,-2200|0|1400×1200|
|9|Power distribution|2200,1100|0|1200×800|
|10|Bravo checkpoint|2200,2100|0|1200×800|
|11|Bravo deployment|4000,2100|0|1200×1000|
|12|West maintenance|-3600,-1600|-600|1000×800|
|13|Generator hall|-1700,-2400|-600|1200×1000|
|14|Cable infrastructure|600,-2400|-600|1400×1000|
|15|Lower access junction|3600,-1000|-600|1000×1000|
|16|North service route|1200,1800|-600|1600×800|
|17|Lower Core access|0,0|-600|1600×1600|
|18|Observation room|-400,0|600|800×1000|
|19|North catwalk|0,3200|600|1400×600|
|20|Control gallery|1700,1600|600|1200×800|
|21|Containment bridge|1000,0|600|1400×600|
|22|Upper maintenance|2700,-1900|600|1200×800|

## Doorways and horizontal connections

All following connections are bidirectional, 400cm-wide floor corridors, with full-height passage openings where they meet a room. Exact centerline bend coordinates are stored in `Connections()`; walls derive from the combined footprint and therefore cannot seal these doorways. IDs0–15 form the main floor: Alpha–Security; Alpha–Storage; Security–Server; Server–Storage; Server–Core; Storage–Extraction A; Extraction A–Medical; Medical–Core; Security–Core south access; Core–Lab; Lab–Extraction B; Lab–Power; Core–Power; Power–Bravo checkpoint; Bravo checkpoint–Bravo; Extraction B–Bravo via east dogleg.

Lower IDs16–22 form a maintenance circuit: West maintenance–Generator–Cable–Lower junction–North service–Lower Core–West maintenance. Cable has an extra direct Lower Core connection. Upper IDs23–27 form the Observation–Bridge–Control gallery–North catwalk–Observation loop. Upper maintenance connects to the bridge via its north and west entrances (IDs27 and36), providing a small additional circulation loop instead of a dead end. IDs28–35 connect ordinary floor routes to each vertical landing and are not additional rooms.

The south security approach and east power approach are long enough to prevent direct spawn-to-Core sightlines. Each spawn also has a secondary outward rotation. Extraction A can be approached from storage or medical; Extraction B from lab or the Bravo-side east circuit. Neither extraction is team restricted.

## Independent vertical routes

Four stair/ramp corridors have400cm clear width, 2000cm horizontal run, 600cm rise (16.7 degrees). Hidden smooth ramps support character movement; modular kit stair treads and metal framing provide the visible stair architecture. Top-floor geometry is removed over the ramp until the top landing so there is no accidental blocking slab. The stairwell is enclosed with side walls and a roof at top-floor+420.

| Link | From → To | Bottom X,Y,Z | Top X,Y,Z |
|---|---|---|---|
|0|West maintenance → Transfer storage|-4700,-1600,-600|-4700,400,0|
|1|Lower access → Power|4700,-1000,-600|4700,1000,0|
|2|Core north access → North catwalk|-600,1200,0|-600,3200,600|
|3|Core south access → Containment bridge|1200,-2200,0|1200,-200,600|

These are separate circulation links, not elevators required for navigation. Upper sightlines are restricted to the containment volume and local upper routes. Observation opens east into the double-height Core chamber; the bridge supplies a second upper contest point. An intentionally marked400×400 floor opening at X[-200,200],Y[200,600] joins main and lower Core for passing/interception. Lower ceiling is omitted above this aperture. The Core pickup itself stays on solid floor at(300,-300,80). Observation/bridge floor has a selected open edge and guardrail rather than invisible full-height glass.

## Gameplay anchors and rotation intent

Alpha three-player positions are(-3700,-2500,110),(-3600,-2500,110),(-3500,-2500,110); Bravo positions are(3750,2200,110),(3850,2200,110),(3950,2200,110). Core is(300,-300,80); extraction centers are(-3200,2200,20) and(3800,-2200,20). Existing pickup, carry, drop, throw, catch, interception, death-drop and fifteen-second extraction remain gameplay-owned. The Bravo wing was moved700cm toward the objective following a preliminary topology route comparison; the deployment positions were then moved150cm west following actual baked collision validation.

The first collision pass reached every floor and all four stairs, but exposed a catwalk anchor on the stairwell wall, a medical route endpoint on the Core well guardrail, and an electrical cabinet across the Power entrance. These were corrected by moving the catwalk landing route north around the stairwell, keeping medical route waypoints west of the well, and moving the electrical bank to the east perimeter. The subsequent report records the final results.

Primary fights occur at the Server/Core door, Lab/Core door, Core southern throat, Power/Core corner and the extraction entrances. Walls and doglegs prevent a continuous spawn-to-spawn shot. The lower ring trades distance for avoidance; its Cable–Lower Core cross-link enables interception beneath the objective. The upper loop offers short local overlooks with two independent exits, and cannot see through the other rooms' ceilings. Center navigation samples and all corridor centerlines are reserved from dense props. Route lengths and actual movement/extraction results belong in the generated validation report, not unmeasured claims in this plan.

## Environment and future SHIFT sockets

The supplied industrial kit provides concrete floor/wall modules, metal wall skins, beams, columns, stairs, platforms, rails, laboratory conveyors/workstations, server-like electrical cabinets, control desks, storage shelves/boxes, boilers, forklifts, pipes and luminaires. Material variations are the kit's retained PBR materials. Architecture is instanced by mesh; collision uses hidden simple shapes matching floors, walls, stairs and substantial props. There is no visible primitive-cube architecture.

Alpha uses cyan deployment markings; Bravo amber/red; laboratories cool white/cyan; power and lower service amber; extraction A green and B violet. Room labels, numbered bulkheads, containment framing, ceiling lamps, cable infrastructure and localized machinery make rooms recognizable. Machinery sits at perimeters or explicitly placed combat cover, preserving routes.

Persistent named `ATargetPoint` markers tagged `GridV3FutureShift` reserve two security doors, two service-lift landings, the containment bridge, two hack terminals, a laboratory breach panel, four security cameras, and23 independently named lighting sectors. The typed tags are `SHIFT_SecurityDoor`, `SHIFT_LiftLanding`, `SHIFT_MovableBridge`, `SHIFT_HackTerminal`, `SHIFT_BreachableWall`, `SHIFT_SecurityCamera`, and `SHIFT_LightingSector`. These actors carry no gameplay or collision logic. Power-room cabinets represent future distribution controls; terminals and security stations are architectural dressing only. No M4 or hackable-system implementation is added by this map builder.

## Validation boundaries

Compile, collision/route checks, actual ramp movement, rendered room viewpoints and correction pass are required after construction. The separate builder is deterministic and uses the topology header for validation. A functional rendered level and an honest validation report are deliverables; this document alone is not completion. Frame rate and route balance are not claimed without measurements.

## Rendered correction pass

The first rendered set exposed washed-out room labels, excessive cyan floor reflections, generic electrical cabinets in the server room, an apparently unsupported bridge, and a roof opening above the intentional Core passing well. The correction adds backed hanging signs with bounded text width; reduced local lamp intensity; server cooling/monitor equipment; bridge bearers and roof suspension rods; and a roof patch at1320cm that preserves all three-floor gameplay openings. The second rendered review rejected a gray floor-material substitution and intersecting wall skins because they lost texture detail and caused visible artifacts. The final version retains each floor mesh's authored PBR material, uses separate matte ceiling modules, removes the intersecting skins, turns monitor screens toward players, and raises the server label above the racks. The final rendered set is in `Saved/GridV3Shots/final`; Core/server inspection confirms restored floor detail, corrected monitor fronts, readable dark-backed signs and removed intersecting skins.

The geometry report is `Saved/GridV3Validation.txt`; actual pickup, passing, interception, extraction and stair movement results are in `Saved/GridV3Gameplay.txt`. The final bake contains23 rooms,37 horizontal centerline paths (including8 landing connectors),4 independent slopes,29 instanced kit mesh groups,8,338 visible mesh instances and35 inert SHIFT preparation markers. The old GRID and Foundry builders are not called by this architecture builder. Performance and competitive balance still require sustained playtesting; those are not established by a single rendered capture.
