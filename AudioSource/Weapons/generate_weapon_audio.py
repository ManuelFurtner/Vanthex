"""Deterministic, original synthesis for Vanthex. Python 3 standard library only."""
from pathlib import Path
import hashlib
import json
import math
import random
import struct
import wave

ROOT = Path(__file__).resolve().parent
RATE = 48000
# body frequency, decay, texture. These are designed signatures, not firearm recordings.
PROFILES = {
    "Pistol": (145, .070, .66), "P250": (123, .082, .72),
    "MP5": (172, .060, .65), "MP7": (215, .043, .84),
    "M4": (109, .082, .85), "AK": (88, .100, .92),
    "AUG": (97, .091, .77), "DMR": (74, .125, .88),
    "Shotgun": (61, .145, 1.0),
}
PHASES = ["Mechanic", "ReloadStart", "ReloadMagazineOut", "ReloadMagazineIn", "ReloadBolt", "ReloadEnd", "DryFire", "Draw", "Holster"]
MANIFEST = []


def rng_for(name):
    return random.Random(int.from_bytes(hashlib.sha256(name.encode()).digest()[:8], "little"))


def save(name, samples):
    # Peak headroom; edge ramps prevent clicks at file boundaries.
    featured = "_Shot_" in name and ("_AUG_" in name or "_MP7_" in name)
    fade = min(36 if featured else 180, len(samples) // 10)
    for i in range(fade):
        samples[i] *= i / fade
        samples[-i - 1] *= i / fade
    peak = max(abs(x) for x in samples) or 1
    samples = [x * .48 / peak for x in samples]
    pcm = b"".join(struct.pack("<h", round(x * 32767)) for x in samples)
    path = ROOT / (name + ".wav")
    with wave.open(str(path), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(pcm)
    MANIFEST.append({"file": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                     "channels": 1, "sample_rate": RATE, "duration": len(samples)/RATE,
                     "peak_dbfs": round(20*math.log10(max(abs(x) for x in samples)), 3)})


def shot(name, frequency, decay, texture):
    if "_AUG_" in name or "_MP7_" in name:
        return featured_shot(name, "_MP7_" in name)
    rng = rng_for(name)
    length = min(.46, .17 + decay * 1.6)
    out, smooth = [], 0.0
    detune = rng.uniform(.965, 1.035)
    for i in range(int(RATE * length)):
        t = i/RATE
        noise = rng.uniform(-1, 1)
        smooth = .70*smooth + .30*noise
        # A broad pressure impulse, falling pitched body and brief high crack.
        pressure = math.sin(2*math.pi*frequency*detune*t + 1.2*(1-math.exp(-t*45))) * math.exp(-t/decay)
        grit = (.72*smooth + .28*noise) * math.exp(-t/(decay*.75))
        crack = noise * math.exp(-t/.008)
        harmonic = math.sin(2*math.pi*(frequency*2.37)*t) * math.exp(-t/(decay*.45))
        out.append(.58*pressure + texture*.75*grit + .45*crack + .13*harmonic)
    save(name, out)


def featured_shot(name, compact):
    """Dry, original layered signature. Room response belongs to the runtime router."""
    rng = rng_for(name)
    duration, decay = (.19, .028) if compact else (.31, .052)
    low, band = 0.0, 0.0
    frequency = (184 if compact else 92) * rng.uniform(.98, 1.02)
    out = []
    for i in range(int(RATE * duration)):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        low = .88 * low + .12 * noise
        band = .43 * band + .57 * noise
        transient = (noise - low) * math.exp(-t / (.0028 if compact else .0045))
        blast = (band - low) * math.exp(-t / decay)
        # A brief pressure body avoids a long electronic sine-wave tail.
        pressure = math.sin(2 * math.pi * frequency * t + .7 * (1-math.exp(-t*90))) * math.exp(-t / (decay*.55))
        age = max(0, t - (.018 if compact else .031))
        latch = (noise - band) * math.exp(-age/.005) if age > 0 else 0
        out.append(.72*transient + 1.25*blast + .34*pressure + .08*latch)
    save(name, out)


def mechanism(name, event, character=0):
    rng = rng_for(name)
    duration = {"ReloadStart": .13, "ReloadMagazineOut": .22, "ReloadMagazineIn": .20,
                "ReloadBolt": .24, "ReloadEnd": .12, "Mechanic": .075,
                "DryFire": .065, "Draw": .26, "Holster": .22, "KnifeSwing": .27}[event]
    out = [0.0] * int(RATE * duration)
    if event == "KnifeSwing":
        low = 0.0
        for i in range(len(out)):
            t = i / len(out)
            noise = rng.uniform(-1, 1)
            low = .88*low + .12*noise
            out[i] = (noise-low) * math.sin(math.pi*t)**2 * (.3 + .7*t)
    else:
        pulses = [0.004] if event in ("DryFire", "Mechanic") else [0.008, duration*.48]
        for start in pulses:
            freq = rng.uniform(850, 1600) + character * 23
            for j in range(int(RATE*.065)):
                at = int(start*RATE)+j
                if at >= len(out): break
                t = j/RATE
                click = (.65*rng.uniform(-1, 1) + .35*math.sin(2*math.pi*freq*t))*math.exp(-t/.009)
                body = .24*math.sin(2*math.pi*(180+character*7)*t)*math.exp(-t/.023)
                out[at] += click + body
        if event in ("Draw", "Holster", "ReloadMagazineOut", "ReloadMagazineIn"):
            low = 0.0
            for i in range(len(out)):
                low = .87*low + .13*rng.uniform(-1, 1)
                out[i] += low*.22*math.sin(math.pi*i/len(out))**2
    save(name, out)


def ui(event):
    frequencies = {"PurchaseAccepted": (620, 930), "PurchaseRejected": (270, 215),
                   "BuyPhaseStart": (440, 660, 880), "RoundStart": (520, 780, 1040)}[event]
    duration = .12*len(frequencies)
    out = []
    for i in range(int(RATE*duration)):
        t = i/RATE
        note = min(len(frequencies)-1, int(t/.12))
        age = t-note*.12
        out.append(math.sin(2*math.pi*frequencies[note]*age)*math.exp(-age/0.038)*min(1,age/.004))
    save("S_WA_UI_"+event, out)


def equipment_move(name):
    """One combined foot contact + quiet cloth/strap movement; no armor-dependent layers."""
    rng = rng_for(name)
    out, low = [], 0.0
    body_frequency = rng.uniform(72, 88)
    for i in range(int(RATE*.23)):
        t = i/RATE
        noise = rng.uniform(-1, 1)
        low = .84*low + .16*noise
        sole = (.52*math.sin(2*math.pi*body_frequency*t) + .45*low) * math.exp(-t/.024)
        sole += .15*noise*math.exp(-t/.006)
        gear_age = max(0, t-.018)
        gear = .20*low*min(1, gear_age/.018)*math.exp(-gear_age/.065)
        out.append(sole+gear)
    save(name, out)


def main():
    for k, (weapon, (freq, decay, texture)) in enumerate(PROFILES.items()):
        for variant in range(1, 4):
            shot(f"S_WA_{weapon}_Shot_{variant:02d}", freq, decay, texture)
        for phase in PHASES:
            mechanism(f"S_WA_{weapon}_{phase}", phase, k)
    for phase in ("Draw", "Holster", "KnifeSwing"):
        mechanism("S_WA_Knife_"+phase, phase, 10)
    for event in ("PurchaseAccepted", "PurchaseRejected", "BuyPhaseStart", "RoundStart"):
        ui(event)
    rng = rng_for("room-tail")
    tail, low = [], 0.0
    for i in range(int(RATE*.18)):
        t = i/RATE
        low = .88*low+.12*rng.uniform(-1, 1)
        tail.append(low*math.exp(-t/.045)*min(1,t/.008))
    save("S_WA_RoomTail", tail)
    for variant in range(1, 4):
        equipment_move(f"S_WA_EquipmentMove_{variant:02d}")
    (ROOT/"manifest.json").write_text(json.dumps({"generator_version": 3, "source": "Original deterministic procedural synthesis; no sampled or downloaded recordings", "files": MANIFEST}, indent=2)+"\n", encoding="utf-8")
    print(f"Generated {len(MANIFEST)} mono PCM16 sources; all peaks <= -6.37 dBFS")


if __name__ == "__main__":
    main()
