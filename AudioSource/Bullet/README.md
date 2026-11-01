# Bullet Audio M1 source sounds

These 16 short effects are original deterministic synthesis created for this project. They are audible prototype defaults, not microphone recordings or a physical simulation. Replace the SoundWave references in `DA_BulletAudio` with production recordings when available.

| Family | Variants | Duration | Purpose |
| --- | ---: | ---: | --- |
| Whiz | 3 | 145 ms | Same family for all near-miss bands; continuous distance gain is applied by the router. |
| Flyby | 2 | 190 ms | Air sound fallback when the selected near-miss family is unassigned. |
| Crack | 2 | 55 ms | Optional supersonic transient. |
| Concrete | 3 | 165 ms | Short gritty impact. |
| Steel | 3 | 240 ms | Damped metal impact. |
| MetalGrate | 3 | 270 ms | Damped metal impact with a short rattle. |

All files are mono PCM, 44.1 kHz, 16-bit. Spatial position is supplied by Unreal; the source files contain no baked stereo direction. Variant RMS is matched within each family, and peaks are bounded to 0.64 full scale. The profile controls volume; no random base-volume variation is used.

Regenerate the source files with `py H:\Vanthex\AudioSource\Bullet\generate_bullet_audio.py`. Build the editor target, then import/create assets with:

```powershell
& 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'H:\Vanthex\Vanthex.uproject' -run=VanthexBuildBulletAudio -unattended -nop4 -nosplash -nullrhi
```

The commandlet defaults to preserving existing assets and only creates missing ones. Explicit `-ReplacePrototypeAssets` rebuilds the generated profile, attenuation, concurrency and SoundWaves from these sources; it should only be used when intentionally replacing prototype edits. `manifest.json` records source duration, levels, and reproducibility hashes.
