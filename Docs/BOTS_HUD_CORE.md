# Bots, owner body, HUD and optional Energy Core

The HUD uses a 1280 x 720 reference scaled to the viewport. The middle remains clear for aiming. It reads replicated game state and the local player's actual health, armor, equipment, ammo and gadget inventory. Round clock and alive team pips sit at the top; health/armor and ammo anchor opposite lower corners. Inventory slots, credits, buy phase, low health, reload, crouch, EMP, core possession, extraction and nearby device interactions have dedicated presentations. Extraction progress uses the server-published duration (15 seconds by default). Existing ADS reticles, hit feedback and gadget effects remain active.

Bots turn the pawn as well as their control rotation. The old AController implementation updated aim without rotating the visible operator. New target acquisition requires a forward 120-degree field and unobstructed line of sight, with smoke respected. Bots turn at up to 150 degrees/second, react after one second, use short automatic bursts with pauses and aim error, and keep combat distance instead of blindly chasing the objective through enemies. Local steering avoids walls, drops and squad crowding. These are testable prototype opponents, not a completed tactical squad AI.

The local player has a separate owner-only skeletal body and legs, animated from the same movement and crouch state. Head and upper arms are hidden in this view to avoid camera intersection; the complete world mesh and shadow remain available to other players. This presentation adds no duplicate collision.

**Hit detection remains simplified body/head proxies.** It is not per-bone collision matching the entire animated silhouette. Crouch adjusts the proxies; precise limb hitboxes remain a separate task.

## Energy capsule (local only)

The optional source is `Energy-542e5bc7/blender/source/p_146_MechanicalDoodle_004.blend` plus its adjacent textures. This licensed package and all derived UE meshes/materials/textures are excluded from public Git. Obtain it through your own entitlement on each workstation.

With the editor closed and the C++ editor target built, run `Import-Energy-Core.ps1 -Blender <path-to-blender.exe>`. The exporter disables embedded Blender scripts, reads the original file without saving it, and selects capsule objects only. The display hand, moths and unused decal templates are excluded. Intermediate exports go to `Saved/EnergyImport`; Unreal output goes to `Content/Vanthex/Imported/Core`.

The capsule is 48 cm tall, preserves its metallic surfaces, transparent glass, orange emission, information decals and normal maps, and has three detail levels. Pickup, carry, drop, throwing, catches and server extraction rules use the same Core actor as before. If the optional mesh is absent, the authored Core kit loads automatically. `-VanthexOwnCoreModel` explicitly selects the authored fallback for comparisons.

Development capture flags: `-HUDShots` (six HUD states), `-BodyViewShots` (standing, crouching, walking, forward), and `-CoreShots` (pickup, carry, drop). Run each separately with `-game -RenderOffscreen -ResX=1280 -ResY=720 -nosound -unattended`; results and completion reports appear in matching `Saved` folders. HUD fixtures publish controlled UI states; the timed extraction gameplay test separately exercises the actual 15-second rule.
