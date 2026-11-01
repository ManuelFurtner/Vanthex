"""Check the actual Unreal mixer capture from -BulletAudioPlaybackTest (stdlib only)."""
import argparse
import array
import json
import math
import pathlib
import sys
import wave


def verify(path: pathlib.Path) -> dict:
    with wave.open(str(path), "rb") as wav:
        channels, width, rate, frames = wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getnframes()
        if channels != 2 or width != 2:
            raise ValueError(f"Expected 16-bit stereo mixer WAV, got {channels} channels/{width * 8} bits")
        samples = array.array("h", wav.readframes(frames))
    if sys.byteorder != "little":
        samples.byteswap()

    def rms(start: float, end: float, channel: int) -> float:
        values = samples[int(start * rate) * channels + channel:int(end * rate) * channels:channels]
        return math.sqrt(sum(value * value for value in values) / max(1, len(values))) / 32768.0

    left_event = [rms(0.9, 1.9, channel) for channel in range(2)]
    right_event = [rms(1.9, 2.9, channel) for channel in range(2)]
    impact = [rms(2.9, 3.9, channel) for channel in range(2)]
    peak = max((abs(value) for value in samples), default=0) / 32768.0
    passed = (frames / rate >= 4.0 and 0.001 < peak < 0.99
              and left_event[0] > 1.5 * max(left_event[1], 1e-8)
              and right_event[1] > 1.5 * max(right_event[0], 1e-8)
              and max(impact) > 1e-5)
    return {"status": "PASS" if passed else "FAIL", "path": str(path.resolve()),
            "channels": channels, "sample_rate": rate, "duration": frames / rate, "peak": peak,
            "left_event_rms_lr": left_event, "right_event_rms_lr": right_event, "impact_rms_lr": impact}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("wav", nargs="?", type=pathlib.Path,
                        default=pathlib.Path(__file__).resolve().parents[1] / "Saved" / "BulletAudioPlayback.wav")
    args = parser.parse_args()
    result = verify(args.wav)
    output = json.dumps(result, indent=2)
    print(output)
    args.wav.with_suffix(".verification.json").write_text(output + "\n", encoding="utf-8")
    sys.exit(0 if result["status"] == "PASS" else 1)
