# VANTHEX — Environment pipeline

Visual Pass 02. This is how GRID is built today, and what that build cannot do.

Art follows the gameplay skeleton. Team spawns, the core home, both extraction volumes, cover, route walls, and the catwalk collision are not moved to suit a mesh. New geometry is `NoCollision`.

## What is actually available

Checked against the Unreal Engine 5.8 install used by this project.

Available and used:

- Engine basic shapes: cube, cylinder, sphere, plane. These remain the gameplay collision and the first-person rifle.
- C++ `FMeshDescription` mesh generation in the `VanthexBuildArt` commandlet (`-run=VanthexBuildArt -AllowCommandletRendering`). The editor module links `MeshDescription`, `StaticMeshDescription`, and `AssetRegistry`.
- Runtime instancing through `UInstancedStaticMeshComponent`. The arena actor places the kit in `DressPass02`.
- Material expressions authored in that same commandlet, including one custom HLSL node for the surface shader.
- Deferred decals (`MD_DeferredDecal`, translucent). Floor decals use a -90 degree pitch. Impact decals are parented to the hit component, not the weapon.
- Nanite on the generated kit meshes. The surface, energy, and glass materials set `bUsedWithNanite` and `bUsedWithInstancedStaticMeshes`. Without those flags Unreal draws the default material.
- A 5×7 bitmap sign font drawn to a texture at runtime. Facing uses `FRotationMatrix::MakeFromXZ` so the text is not mirrored.

Present on disk and not used for this pass:

- The Geometry Scripting plugin.
- The Python editor plugin.
- Engine debug materials and a few utility textures (`WhiteSquareTexture`, default normals). The high-frequency engine noise texture was tried in Pass 01 and reads as a checker. Do not use it as albedo.

Not available. Do not invent references to these:

- Megascans, Fab, and a Starter Content prop pack.
- An Unreal MCP or editor-automation server.
- A character arm mesh, a weapon mesh, or a scanned material library.

## Modular kit

Generated under `/Game/Vanthex/Meshes/`. Each mesh has a Structure slot and an Emissive slot. Pivot and scale conventions:

| Mesh | Role |
| --- | --- |
| `SM_WallPanel` | 200 cm wide panel. Local +X is the thickness into the room. Placed on the inner face of the gameplay walls. |
| `SM_Column` | Structural post, pivot at the floor. Also used as catwalk supports. |
| `SM_Beam` | 100 cm I-beam along local X. Scale X to the span. |
| `SM_Light` | Housing plus an emissive diffuser. |
| `SM_Rack` | Server row on the west side. |
| `SM_Cabinet` | Electrical cabinets on the east side. |
| `SM_Vent` | Wall vent above the door gaps. |
| `SM_Door` | Security door visual in the north and south gaps. The window uses glass. |
| `SM_Railing` | 200 cm rail run for the catwalk. |
| `SM_Crate` | Maintenance crate, kept off the center shot lane. |
| `SM_Tray` | Cable tray under the ceiling. |
| `SM_Pylon` | Extraction post. Cap is the energy material. |
| `SM_TransferBase` | Extraction floor machine. Replaces the visible pad. The zone volume is unchanged. |
| `SM_CoreDevice` | Hero core cage. Gameplay collision and throw behavior stay on the core actor. |
| `SM_Ring` | Overhead containment ring above the core home. |
| `SM_SignPlate` | Dark plate behind bitmap text. Front is local +X. |
| `SM_Fan` | Ceiling fan. It spins. It does not collide. |

Pipes are engine cylinders. The catwalk floor and the ramp stay the gameplay slabs, with rails and supports added beside them.

## Materials

`M_VanthexSurface` is the only lit family. Instances choose the look:

| Style | Use | Notes |
| --- | --- | --- |
| Concrete | Gameplay walls, ceiling, wall panels | Large panels, high roughness |
| Painted steel | Cover, columns, doors, cabinets | Mid roughness, some metal |
| Bare steel | Catwalk, rails, pipes | Clear metal, fine brush |
| Dark metal | Route walls, beams, racks | Dark, moderate metal |
| Floor | The hall floor | 200 cm seams, more wear |
| Rubber | Rifle grip and similar small parts | Soft, low relief |

Wear is a darker seam, not abandoned-facility grime. GRID is operational.

`M_VanthexDecal` draws the center line, the core pad, and hazard stripes at the extraction approaches. `M_VanthexEnergy` is the only colored light language: cyan for the core and Transfer A, amber for Transfer B, warm white on practical fixtures.

The custom surface HLSL is inlined. Nested functions are illegal inside Unreal's generated material function and will fail the shader even when the asset saves. After every commandlet run, confirm the log has no `Failed to compile` and no `missing usage flag`. `saved 1` alone is not a successful shader.

## Lighting

Cool industrial sun and skylight. Practical spots mark Alpha, Bravo, the core, both extractions, and the catwalk. Auto exposure is clamped so a dark ceiling does not blow the floor to white. Bloom stays low. No motion blur, chromatic aberration, or heavy grain.

## Prop rules

Props explain a zone. They are not scattered.

- West wall: server racks.
- East wall: electrical cabinets.
- Core: containment ring, core device, core floor pad, CORE CONTAINMENT sign.
- Extractions: transfer base, pylons, cable, hazard stripes, TRANSFER sign.
- Catwalk: rails, supports, tray, a few crates off the lane.
- Spawns: the existing team pads plus SECTOR / SECURITY signs.

Do not add bullet-blocking decoration on the chest-height X=0 lane. Do not extend cover silhouettes past the collision the bullets already use.

## Signage

Words the bitmap font can draw today include GRID, ENERGY, CORE, CONTAINMENT, SECTOR, POWER, SECURITY, and TRANSFER, plus A, B, and the digits 0 and 1. Many other letters and digits are missing. Do not place a word the font cannot spell. Plates face the approach that should read them. Validate from a gameplay camera, not from the editor top view.

## Performance strategy

- Instance the kit. Do not spawn one actor per panel.
- Share the surface material. Do not make a material per prop.
- Nanite on the kit. Gameplay cubes stay simple.
- Keep the spot count at the current objective set.
- Keep combat VFX to a short muzzle flash, a small spark, and a short-lived impact decal. No opaque smoke in a fight.
- The commandlet is an offline build step. The match does not generate meshes.

## Asset limitations

The kit is constructed from boxes. At rifle distance the silhouettes are still obviously modular and low-detail. The floor and wall variation is a math shader, so it repeats. The rifle has no hands. Impact effects are one spark and one decal, not material-specific particle systems. There is no steam, dust volume, or monitor texture.

## What a later pass needs

In this order:

1. Authored tiling textures for concrete, painted steel, and floor, so the large surfaces stop reading as a repeated shader.
2. A sculpted core and two machined transfer stations. These are the objects a screenshot is judged on.
3. First-person arms and a real rifle mesh. Gunplay numbers stay as they are.

Until those exist, do not claim GRID is finished commercial art.
