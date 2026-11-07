# VANTHEX — High-fidelity pipeline

Current material/construction refinement (2026-11-07):
[PHOTOREALISM_UPGRADE_2026-11-07.txt](PHOTOREALISM_UPGRADE_2026-11-07.txt).
`Build/Blender/Build-Photoreal.ps1` defaults to a separate review map; `-FullMap`
updates the existing map. `Restore-PhotorealFallback.ps1` restores the saved art.

## Current GRID V4 Blender pipeline — 2026-11-06

The sections below describe an older procedural art pass. GRID V4 now uses original
Blender-authored Industrial/Refinery modules, including baked PBR maps. The current
implementation and measured limits are recorded in
[GRID_V4_REFINERY_2026-11-06.txt](GRID_V4_REFINERY_2026-11-06.txt).

Reproduction: `Build/Blender/Build-Refinery.ps1` defaults to the separate
`RefineryLab_GridV4` review level. `-FullMap` explicitly replaces the GRID V4 art.
`-ExportEditedBlend` exports editable source collections without regenerating them.
Blender 4.5 LTS and Unreal Engine 5.8.3 are used. Physics remains in the separate
original engine-cube proxies; new visual meshes have no collision.

These changes increase industrial detail; they do not establish visual equivalence
to the supplied reference images or a finished AAA environment.

Visual Pass 03. This is the honest limit of what the project can render without outside assets.

The supplied GRID image is art direction: structural depth, practical light, a heavy core, large transfer machines, and a ballistic rifle. It is not a layout to copy, and this pass does not reproduce it.

## What caused the prototype look

Pass 02 was still a box kit plus one world-space shader. The largest single arcade tell was the green hitscan debug line (`bDrawDebugTrace`, default on). After that, every silhouette was a rectangle: walls, core, transfer stations, rifle, ceiling.

Adding more of those rectangles does not close the gap. Pass 03 changed the modules instead.

## What is actually available

Used:

- Engine cube, cylinder, sphere, and plane for gameplay collision and for effects that have no mesh of their own.
- `VanthexBuildArt` (`-run=VanthexBuildArt -AllowCommandletRendering`) writes materials and Nanite meshes with `FMeshDescription`.
- Runtime instancing. New dress is `NoCollision`.
- Deferred decals.
- The bitmap sign font. Pass 03 adds the letter X so `VANTHEX` can be spelled.

On disk, not used to author GRID:

- Geometry Scripting plugin.
- Python editor plugin.

Not available. Nothing in this pass pretends otherwise:

- Fab, Megascans, Quixel Bridge content, Starter Content props.
- Unreal MCP or an editor-automation server.
- A licensed arm mesh, rifle mesh, or texture library.
- An authored FBX, OBJ, or glTF file to import.

The engine can import those formats later. There is no file here to import. The commandlet is the only generator that runs without a manual modeling session.

## What Pass 03 built

Kit meshes, still boxes with real depth rather than a single slab:

- `SM_WallPanel` and `SM_WallPanelB` — frame, recessed bays, ribs or slats, a conduit, bolts. Alternated on the walls.
- `SM_FloorPlate` — bordered access plate. Instanced across the hall, skipped under the core and both extraction machines.
- `SM_Grate` — catwalk grating, no collision, sitting on the existing catwalk slab.
- `SM_Duct` and `SM_Pipe` — overhead runs in some bays, not a uniform lid. The long stretched cylinders are gone.
- `SM_CoreDevice` — cage, lock bars, side handles, status lamps. Outer size stays readable.
- `SM_TransferBase` — thick base, rear housing, two inward arms, a cradle, a small status lamp. Cyan and amber stay on the pylon caps.
- `SM_Rifle` — stock, receiver, grip, magazine, ribbed handguard, barrel, muzzle device, warm optic lamp.

Floor shader repeat was opened up so it no longer draws a tight tile grid under the plates.

## Ballistic firing

The debug line is deleted. A shot does not draw green or red.

On fire, with no change to damage, interval, ammo, or the trace:

- a short warm muzzle flash, shown the same frame it is fired
- a small gray puff
- a brass shell with no collision, thrown to the right
- concrete: dull dust and a dark decal
- metal: a few warm sparks and a darker decal

There is no tracer. Cyan is not used on the rifle.

## What the screenshots still show

1920×1080, `Saved/VanthexShots`, `01_alpha_spawn` through `18_combat`.

The hall has plates, ducts, a ring, chamber columns, and machines. It still reads as a constructed kit. The rifle has a silhouette and a warm dot, and it is still made of boxes. One wide view has a chamber column in front of the left side of `CORE CONTAINMENT`. The other signs that were checked (`GRID / ENERGY`, `SECURITY`) read forward.

`Saved/VanthexSelfTest.txt` after this pass is OVERALL PASS, including throw speed 2000, both extractions, and match alpha 7 bravo 0.

## Performance

Instanced kit, one surface material, Nanite on the generated meshes, the existing spot set, no volumetric fog, no collision on the dress. Shells and impact specks are short-lived and do not block traces.

## What the next jump requires

These are external. Procedural boxes will not become the reference image.

1. A licensed tiling set: concrete, painted steel, bare metal, and floor, each with base color, normal, and roughness. Not a noise texture.
2. An authored modular mesh kit in FBX or glTF: wall with real bevels, I-beam, duct, grate, railing, door. Import through the normal Unreal importer.
3. A sculpted core and two machined transfer stations.
4. First-person arms and a rifle mesh with a legal license. Gunplay numbers stay as they are.
5. A signage font, and a small Niagara or mesh set for muzzle smoke and impact dust if the current spheres are not enough.

Until those exist, GRID can get cleaner, and it cannot honestly match the reference.
