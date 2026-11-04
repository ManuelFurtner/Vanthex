"""Verify committed source audio independently of Unreal and the synthesis routine."""
from pathlib import Path
import array
import hashlib
import json
import math
import sys
import wave

ROOT = Path(__file__).resolve().parents[1]


def verify():
    bank = ROOT / "AudioSource/Weapons"
    manifest = json.loads((bank / "manifest.json").read_text(encoding="utf-8"))
    results = []
    failures = []
    for entry in manifest["files"]:
        path = bank / entry["file"]
        try:
            assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"], "manifest hash mismatch"
            with wave.open(str(path), "rb") as source:
                assert (source.getnchannels(), source.getsampwidth(), source.getframerate()) == (1, 2, 48000), "format"
                assert source.getcomptype() == "NONE", "compressed source"
                samples = array.array("h", source.readframes(source.getnframes()))
                if sys.byteorder != "little":
                    samples.byteswap()
            duration = len(samples) / 48000
            assert 0.02 <= duration <= 0.6, "duration"
            peak = max(abs(s) for s in samples) / 32768
            rms = math.sqrt(sum((s / 32768) ** 2 for s in samples) / len(samples))
            assert 0.001 < rms and peak <= 0.49, "silence or insufficient headroom"
            assert abs(samples[0]) <= 1 and abs(samples[-1]) <= 1, "edge discontinuity"
            assert abs(duration - entry["duration"]) < 1 / 48000, "manifest duration"
            results.append({"file": path.name, "duration": round(duration, 5), "peak_dbfs": round(20 * math.log10(peak), 3), "rms": round(rms, 6)})
        except (AssertionError, OSError, wave.Error) as error:
            failures.append({"file": path.name, "error": str(error)})
    for weapon in ("AUG", "MP7"):
        variants = [x for x in manifest["files"] if x["file"].startswith(f"S_WA_{weapon}_Shot_")]
        if len(variants) != 3 or len({x["sha256"] for x in variants}) != 3:
            failures.append({"file": weapon, "error": "expected three distinct shot variants"})
    report = {"status": "FAIL" if failures else "PASS", "checked": len(results), "failures": failures, "audio": results}
    report_path = ROOT / "Docs/Validation/AudioMenu/source-audio.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{report['status']}: {len(results)} source WAVs checked; {len(failures)} failures")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(verify())
