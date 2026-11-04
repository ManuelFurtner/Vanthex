"""Original deterministic mono gadget synthesis. Python 3 standard library only."""
from pathlib import Path
import hashlib
import json
import math
import random
import struct
import wave

ROOT = Path(__file__).resolve().parent
RATE = 48000
MANIFEST = []


def generate(event, duration, kind, frequency=100):
    rng = random.Random(int.from_bytes(hashlib.sha256(event.encode()).digest()[:8], "little"))
    samples, low = [], 0.0
    for i in range(int(duration*RATE)):
        t = i/RATE
        noise = rng.uniform(-1, 1)
        low = low*.9+noise*.1
        if kind == "blast":
            value = (.66*math.sin(2*math.pi*frequency*t)*math.exp(-t/.12)
                     + .63*low*math.exp(-t/.23) + .38*noise*math.exp(-t/.012))
        elif kind == "flash":
            value = .7*noise*math.exp(-t/.018)+.4*low*math.exp(-t/.085)+.08*math.sin(2*math.pi*1300*t)*math.exp(-t/.06)
        elif kind == "bounce":
            value = (.6*noise+.4*math.sin(2*math.pi*frequency*t))*math.exp(-t/.012)+.15*math.sin(2*math.pi*175*t)*math.exp(-t/.028)
        elif kind == "hiss":
            value = .8*low+.08*noise
        elif kind == "flight":
            value = low*.7+math.sin(2*math.pi*frequency*t)*.025
        elif kind == "emp":
            sweep = 2*math.pi*(frequency*t-120*t*t)
            value = (.5*math.sin(sweep)+.14*math.sin(sweep*2.4)+.3*low)*math.exp(-t/.28)
        elif kind == "hum":
            value = .45*math.sin(2*math.pi*60*t)+.12*math.sin(2*math.pi*120*t)+.12*low
        elif kind == "ping":
            value = (math.sin(2*math.pi*frequency*t)+.18*math.sin(2*math.pi*frequency*2*t))*math.exp(-t/.042)
        else:
            value = (low+.18*math.sin(2*math.pi*frequency*t))*math.exp(-t/.07)
        samples.append(value)
    fade = min(480, len(samples)//8)
    for i in range(fade):
        samples[i] *= i/fade
        samples[-i-1] *= i/fade
    peak = max(abs(x) for x in samples) or 1
    pcm = b"".join(struct.pack("<h", round(x*.48/peak*32767)) for x in samples)
    path = ROOT/f"S_GD_{event}.wav"
    with wave.open(str(path), "wb") as f:
        f.setnchannels(1); f.setsampwidth(2); f.setframerate(RATE); f.writeframes(pcm)
    MANIFEST.append({"event": event, "file": path.name, "channels": 1, "sample_rate": RATE,
                     "duration": len(samples)/RATE, "peak_dbfs": 20*math.log10(.48),
                     "loop": event.endswith("Loop"), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})


def main():
    for args in [
        ("Throw", .18, "gear", 320), ("FlightLoop", .6, "flight", 140),
        ("Bounce01", .13, "bounce", 950), ("Bounce02", .14, "bounce", 1130), ("Bounce03", .12, "bounce", 780),
        ("Flash", .48, "flash", 100), ("SmokeStart", .3, "gear", 220),
        ("SmokeLoop", 1.2, "hiss", 80), ("SmokeEnd", .24, "gear", 120),
        ("Frag", 1.1, "blast", 57), ("EMP", .85, "emp", 430),
        ("EMPHumLoop", .8, "hum", 60), ("SensorStart", .32, "emp", 680),
        ("SensorPing", .18, "ping", 920), ("SensorEnd", .2, "ping", 370),
    ]:
        generate(*args)
    (ROOT/"manifest.json").write_text(json.dumps({"version": 1, "source": "Original deterministic synthesis, no third-party recordings", "files": MANIFEST}, indent=2)+"\n", encoding="utf-8")
    print(f"Generated {len(MANIFEST)} mono gadget sounds, 48 kHz PCM16, peak -6.38 dBFS")


if __name__ == "__main__":
    main()
