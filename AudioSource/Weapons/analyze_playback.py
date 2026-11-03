"""Check actual UE mixer output, not the source WAVs. Usage: python .../analyze_playback.py [WAV]."""
from pathlib import Path
import json
import math
import struct
import sys

path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / "Saved/WeaponAudioPlayback.wav"
raw = path.read_bytes()
assert raw[:4] == b"RIFF" and raw[8:12] == b"WAVE", "Expected WAV RIFF"
offset, fmt, payload = 12, None, None
while offset + 8 <= len(raw):
    name, size = raw[offset:offset+4], struct.unpack_from("<I", raw, offset+4)[0]
    data = raw[offset+8:offset+8+size]
    if name == b"fmt ": fmt = data
    if name == b"data": payload = data
    offset += 8 + size + size % 2
assert fmt and payload, "Missing format or mixer data"
encoding, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", fmt)
if encoding == 0xfffe: encoding = struct.unpack_from("<H", fmt, 24)[0]
assert channels == 2, f"Stereo render required, found {channels} channels"
if encoding == 3 and bits == 32:
    samples = struct.unpack("<"+"f"*(len(payload)//4), payload)
elif encoding == 1 and bits == 16:
    samples = [x/32768 for x in struct.unpack("<"+"h"*(len(payload)//2), payload)]
else:
    raise AssertionError(f"Unsupported mixer encoding={encoding}, bits={bits}")
assert all(math.isfinite(x) for x in samples), "Non-finite mixer sample"
peak = max((abs(x) for x in samples), default=0.0)
clipped_samples = sum(abs(x) >= .999 for x in samples)
assert peak < .999 and clipped_samples == 0, f"Mixer reaches full-scale/clipping: peak={peak:.6f}, samples={clipped_samples}"
frames = len(samples)//channels


def energy(start, end, channel):
    lo, hi = max(0, int(start*rate)), min(frames, int(end*rate))
    return sum(samples[i*2+channel]**2 for i in range(lo, hi))/max(1, hi-lo)


# Recording starts at age 1, sounds at ages 2..8. Allow timer/audio buffer latency.
labels = ["left_AUG", "right_MP7", "front_Pistol", "rear_AUG", "above_AUG", "below_AUG", "occluded_front_AUG"]
result = {"wav": str(path), "sample_rate": rate, "channels": channels, "frames": frames,
          "peak": peak, "peak_dbfs": 20*math.log10(peak) if peak else None,
          "clipped_samples": clipped_samples, "directions": {}}
for i, name in enumerate(labels, 1):
    left, right = energy(i-.12, i+.65, 0), energy(i-.12, i+.65, 1)
    result["directions"][name] = {"left_rms": math.sqrt(left), "right_rms": math.sqrt(right)}
    assert left+right > 1e-10, f"No actual rendered sound: {name}"
l = result["directions"]["left_AUG"]
r = result["directions"]["right_MP7"]
assert l["left_rms"] > l["right_rms"]*1.5, "Left source did not render predominantly left"
assert r["right_rms"] > r["left_rms"]*1.5, "Right source did not render predominantly right"
clear = result["directions"]["rear_AUG"]
muffled = result["directions"]["occluded_front_AUG"]
assert sum(muffled.values()) < sum(clear.values())*.35, "Occluded sound is insufficiently attenuated"
result["result"] = "PASS"
result["limit"] = "Above/below events are rendered, but stereo energy does not prove human HRTF elevation localization."
out = path.with_name("WeaponAudioPlayback-analysis.json")
out.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
print(json.dumps(result, indent=2))
