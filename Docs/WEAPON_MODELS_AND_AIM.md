# Weapon models, sights and optional Fab assets

The repository includes a complete project-owned armory: **10 models / 28 meshes** (nine firearms with body, magazine and bolt, plus one knife). Source geometry and provenance are in `SourceArt/Weapons/manifest.json`; `Scripts/generate_weapon_models.py` generates the geometry and `VanthexBuildWeaponModels` creates its Unreal meshes and PBR materials. The ready assets live under `/Game/Vanthex/Weapons`. These remain available without any Fab downloads.

## Optional local models

The [FPS Weapon Bundle by Deadghost Interactive](https://www.fab.com/listings/8aeb9c48-b404-4dcd-9e56-1d0ecedba7f5) supplies these matching replacements:

| Game weapon | Bundle model | Presentation |
| --- | --- | --- |
| M4 | AR4 | Original iron sights; detachable magazine and charging handle |
| AK | KA47 | Original iron sights; detachable magazine and supported bolt geometry |
| Knife | M9 Knife | Blade rotated from source -Z to the game's +X axis |

Pistol, P250, MP5, MP7, AUG, DMR and Shotgun retain their project-owned models. The bundle's other platforms are not relabeled as these weapons. Derived meshes preserve source PBR materials, UVs, normals and separate skin-weighted parts. Gun dimensions are normalized for the existing 1.7 world-mount scale; grip origin, muzzle socket and real sight geometry determine placement. Gameplay damage, ammunition and inventory rules do not change.

## Install or repeat the import

1. Acquire the packs under your own applicable license and add them to this existing Unreal project using Fab. Expected folders are `Content/FPS_Weapon_Bundle` for models and `Content/FreeWeaponSFX` for audio. Either pack is optional independently.
2. Build **VanthexEditor / Development / Win64** with the project's matching Unreal version (currently EngineAssociation `5.8`). Save work and close Unreal Editor; wait for other Unreal commandlets to finish.
3. From the project root, preview or run:

   ```powershell
   .\Import-Optional-Fab-Assets.ps1 -WhatIf
   .\Import-Optional-Fab-Assets.ps1 -EngineRoot 'H:\epic\UE_5.8'
   ```

The script defaults to `H:\epic\UE_5.8`, then checks registered installations if that default is missing. An explicit `-EngineRoot` is used as supplied. It imports each present pack sequentially, rejects a running editor/commandlet, and checks the process exit code and fresh PASS report. It neither builds nor closes processes. Repeating the import updates only derived local assets. Start a new editor/game process afterward because optional presentation data is selected on first use.

The commandlets can also be run directly with the matching `UnrealEditor-Cmd.exe`:

```powershell
& 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' '.\Vanthex.uproject' -run=VanthexInspectWeaponBundle -unattended -nop4 -nosplash -NullRHI
& 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' '.\Vanthex.uproject' -run=VanthexImportWeaponBundle -unattended -nop4 -nosplash -NullRHI
& 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' '.\Vanthex.uproject' -run=VanthexImportFabAudio -unattended -nop4 -nosplash -NullRHI
```

`VanthexInspectWeaponBundle` is read-only and reports original meshes, materials, sizes, bones and sockets. Its reports and geometry snapshots go to `Saved/WeaponBundleInspection*`. Import reports are `Saved/WeaponBundle-import.txt` and `Saved/FabAudioImport.txt`; the runner also retains timestamped logs in `Saved`.

Models and their metadata are written to `/Game/Vanthex/Imported/Weapons`, with catalog `DA_WeaponBundle`. The optional [Free Weapon SFX](https://www.fab.com/listings/1697af22-7e2a-410e-b8c0-88239216520d) profile is `/Game/Vanthex/Imported/WeaponAudio/DA_WeaponAudio_Fab`. It supplements supported firearm events; MP5, MP7, Knife and uncovered events keep project-owned audio. Missing optional catalogs use the public fallback assets automatically. `-VanthexOwnWeaponModels` explicitly selects the original models for comparison/testing.

## Aiming

Hold the **right mouse button** to aim; release it to return to hip fire. The actual selected model's sights align with the camera axis, and movement and mouse sensitivity adapt to aiming. Iron sights use a 72-degree field of view. The authored M4 and MP7 use reflex sights at 64 degrees; AUG and DMR use scope presentation at 45 and 28 degrees respectively. Imported M4/AK use their own physical iron sights, including the measured aperture/post angle. There is no adjustable zoom control. Knife, sprint, reloading, switching weapons and unavailable combat states do not permit ADS; crouched aiming is supported.

For verification, `Run-Aim-Checks.ps1 -CaptureShots` captures the selected models, and `-OwnWeaponModels` compares the fallback. `Run-Aim-Network-Checks.ps1` checks server/client aim state. The automation group `Vanthex.WeaponModels` checks the authored armory, magazine/bolt presentation and the optional licensed catalog. Screenshots still require visual inspection for grip, sight visibility and materials.

If an older `M_VanthexSurface` produces checkerboard first-person arms, the existing material can be repaired without regenerating art or maps. After rebuilding the editor target and closing the editor, run:

```powershell
& 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' '.\Vanthex.uproject' -run=VanthexBuildArt -RepairSurfaceUsage -unattended -nop4 -nosplash -NullRHI
```

This only enables skeletal-mesh usage on the existing shared surface material, preserves its graph and parameters, and writes `Saved/SurfaceUsageRepair.txt`. New surface generations include the flag automatically.

## Provenance and publication

The FPS pack's local manifest identifies the original UE 4.23 content and Fab listing, but does **not** encode the license acquired by this account. Consult the acquired product's terms. The [official Fab Standard License summary](https://www.fab.com/eula) permits project use and sharing with project collaborators, including through private repositories; it prohibits standalone redistribution of source assets. A free price does not itself grant public source redistribution rights.

This repository is public. Consequently `Content/FPS_Weapon_Bundle`, `Content/FreeWeaponSFX`, `Content/Vanthex/Imported` and `VaultCache` are Git-ignored; imported derivatives and their referenced source assets must remain excluded from public commits unless separate applicable rights are established. Do not force-add these folders. Collaborators can install licensed packs locally and run the script, while the public project remains functional with its own models/audio. Packaging a playable game is different from publishing raw assets; review the applicable asset terms before distribution. The importer does not change repository visibility or grant additional rights.
