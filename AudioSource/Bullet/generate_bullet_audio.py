"""Create original, deterministic AUDIO M1 prototype sounds using Python's stdlib.

These are synthesized placeholders, not field recordings or a ballistic simulation.
Run: py H:\\Vanthex\\AudioSource\\Bullet\\generate_bullet_audio.py
"""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
import random
import struct
import wave

RATE = 44100
ROOT = Path(__file__).resolve().parent
TAU = math.tau


def generate(family: str, variant: int, duration: float) -> list[float]:
    rng = random.Random(f"Vanthex.AudioM1.{family}.{variant}")
    count = round(duration * RATE)
    output: list[float] = []
    low = 0.0
    mid = 0.0
    phase = 0.0
    tuning = 1.0 + (variant - 2) * 0.035
    for i in range(count):
        t = i / RATE
        u = i / max(1, count - 1)
        noise = rng.uniform(-1.0, 1.0)
        low += 0.075 * (noise - low)
        mid += 0.34 * (noise - mid)
        band = mid - low
        attack = min(1.0, t / 0.0015)
        tail = min(1.0, (duration - t) / 0.012)

        if family == "Whiz":
            phase += TAU * tuning * (2700.0 - 1700.0 * u) / RATE
            envelope = math.sin(math.pi * u) ** 2.4
            value = envelope * (0.82 * band + 0.095 * math.sin(phase))
        elif family == "Flyby":
            phase += TAU * tuning * (1400.0 - 900.0 * u) / RATE
            envelope = math.sin(math.pi * u) ** 2.0
            value = envelope * (0.9 * band + 0.12 * low + 0.035 * math.sin(phase))
        elif family == "Crack":
            envelope = math.exp(-t * 180.0) * attack
            value = envelope * ((noise - low) * 0.85 + 0.18 * math.sin(TAU * 3100 * tuning * t))
        elif family == "Concrete":
            body = math.exp(-t * 47) * (0.65 * band + 0.45 * low)
            grit = sum(math.exp(-((t - offset) / 0.004) ** 2) for offset in (0.028, 0.047, 0.075))
            value = attack * (body + 0.06 * grit * noise)
        elif family == "Steel":
            modes = sum(
                amplitude * math.sin(TAU * frequency * tuning * t) * math.exp(-t * decay)
                for frequency, amplitude, decay in ((1450, 0.26, 28), (2483, 0.18, 33), (3860, 0.12, 42))
            )
            value = attack * (0.65 * noise * math.exp(-t * 140) + modes)
        elif family == "MetalGrate":
            modes = sum(
                amplitude * math.sin(TAU * frequency * tuning * t) * math.exp(-t * decay)
                for frequency, amplitude, decay in ((895, 0.19, 24), (1753, 0.14, 30), (2937, 0.10, 36))
            )
            rattle = sum(math.exp(-((t - offset) / 0.0025) ** 2) for offset in (0.021, 0.040, 0.064, 0.090))
            value = attack * (modes + 0.4 * band * math.exp(-t * 50) + 0.22 * rattle * band)
        else:
            raise ValueError(family)
        output.append(value * tail)

    # Remove DC and match RMS between variants. Apply one shared family gain
    # afterwards, so timbral variation never chooses a random volume.
    mean = sum(output) / count
    output = [sample - mean for sample in output]
    rms = math.sqrt(sum(sample * sample for sample in output) / count)
    output = [sample * (0.09 / max(rms, 1e-9)) for sample in output]
    fade = round(RATE * 0.0005)
    for i in range(fade):
        output[i] *= i / fade
        output[-1 - i] *= i / fade
    return output


def main() -> None:
    manifest = {"description": "Original synthesized AUDIO M1 prototype effects; not recordings.",
                "sample_rate": RATE, "channels": 1, "sample_width_bytes": 2, "assets": []}
    families = [("Whiz", 3, 0.145), ("Flyby", 2, 0.190), ("Crack", 2, 0.055),
                ("Concrete", 3, 0.165), ("Steel", 3, 0.240), ("MetalGrate", 3, 0.270)]
    for family, variants, duration in families:
        sounds = [generate(family, index, duration) for index in range(1, variants + 1)]
        family_peak = max(abs(sample) for sound in sounds for sample in sound)
        family_gain = min(1.0, 0.64 / family_peak)
        for index, samples in enumerate(sounds, 1):
            samples = [sample * family_gain for sample in samples]
            pcm = [round(sample * 32767) for sample in samples]
            assert max(map(abs, pcm)) < 32767, "Unexpected clipping"
            filename = ROOT / f"S_Bullet_{family}_{index:02}.wav"
            with wave.open(str(filename), "wb") as wav:
                wav.setnchannels(1)
                wav.setsampwidth(2)
                wav.setframerate(RATE)
                wav.writeframes(struct.pack(f"<{len(pcm)}h", *pcm))
            manifest["assets"].append({
                "file": filename.name, "family": family, "duration_seconds": round(len(pcm) / RATE, 6),
                "peak": round(max(map(abs, samples)), 6),
                "rms": round(math.sqrt(sum(s * s for s in samples) / len(samples)), 6),
                "sha256": hashlib.sha256(filename.read_bytes()).hexdigest(),
            })
    (ROOT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Generated {len(manifest['assets'])} mono 44.1 kHz / 16-bit WAVs, no clipped samples.")


if __name__ == "__main__":
    main()
