# VANTHEX — Asset pipeline

Visual Rebuild 01 stopped here.

`Saved/VanthexShots/02_alpha_approach.png` is still a rectangular hall made of generated boxes, one surface shader, a bitmap font, and a block rifle. Another pass of that generator will not produce the reference image. No new environment meshes were built for this rebuild.

## ASSET PIPELINE BLOCKER

Checked on this machine against Unreal Engine 5.8 at `H:\epic\UE_5.8`.

Present and not a content library:

- `Engine/Plugins/MegascansPlugin` — master materials and placeholder slots (`Albedo`, `Normal`, `Roughness`). No downloaded concrete, metal, or prop scans.
- `Engine/Content/StarterContent` — one moss texture, `T_ground_Moss_D`. No props.
- Geometry Scripting and the Python editor plugin. They can place meshes. They do not contain an industrial kit.
- The engine importer. It accepts FBX, glTF/GLB, and OBJ when a file exists.

Not present:

- A Fab login or any Fab download. The only “Fab” folder in the engine is PlayFab, which is networking, not the marketplace.
- A Quixel or Megascans library under the user profile.
- Authored FBX, glTF, or OBJ files in the Vanthex project.
- Unreal MCP.
- An arm mesh or a rifle mesh.

## What has to be supplied

Put licensed files here, then say they are in place. Do not generate stand-ins.

| Folder | Needed files |
| --- | --- |
| `Content/Vanthex/Import/Environment/` | Modular kit: wall panel with depth, column, beam, ceiling truss, floor plate, grate, door, catwalk, railing, stair. FBX or glTF. |
| `Content/Vanthex/Import/Materials/` | PBR sets for concrete, painted steel, bare steel, rubber, glass. Each set needs base color, normal, roughness. Metal sets also need a metallic map. |
| `Content/Vanthex/Import/Props/` | Pipe with junction, vent, cable tray, conduit, electrical cabinet, server rack, crate, industrial light. |
| `Content/Vanthex/Import/Hero/` | One Core containment device. One transfer machine that can take a cyan material and an amber material. |
| `Content/Vanthex/Import/Decals/` | Original GRID markings only. No copied logos. Hazard, arrows, small wear. |
| `Content/Vanthex/Import/Weapons/` | First-person rifle and arms. This is the highest-priority blocker. The block rifle stays until these files exist. |

A practical source is Fab inside the Unreal Editor, after signing in, filtering for a modular industrial kit and a first-person arms pack whose license allows use in this project. Megascans surfaces can use the master materials already in the engine once the scans themselves are downloaded.

Gameplay collision stays the current shell. Imported meshes are visual and do not replace that collision.

The green hitscan line is already removed. Do not bring it back when the rifle is replaced.
