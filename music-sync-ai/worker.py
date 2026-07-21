#!/usr/bin/env python3
"""Music Sync DJ — optional analysis worker (spec section 16).

Runs OUTSIDE the Mixxx process. It never touches playback, decks, recording or
the Mixxx database — it only reads audio files and returns a spectral energy the
native heuristic cannot give well: an absolute RMS energy on ONE scale across the
whole library (the native one scales by the waveform max, squeezing real music
into a narrow band). Vocal density / structural segmentation via a real model
(ONNX) are a later worker step (spec 16.1) — a mid-band proxy does not actually
tell a vocal from a melodic synth, so it is not shipped as if it did.

Protocol (spec 16.2):
  request : a JSON file  {"tracks": [{"id": 123, "path": "..."}]}  (argv[1])
  events  : JSON Lines on stdout
              {"event":"progress","done":i,"total":n}
              {"event":"track","id":123,"overallEnergy":..,"energyCurve":[..]}
              {"event":"trackError","id":123,"message":".."}
              {"event":"done","total":n}
  logs    : stderr

The module stays fully functional without this worker; if Python or its deps are
missing, Mixxx keeps its C++ heuristics (spec rule 15).
"""

import json
import subprocess
import sys

try:
    import numpy as np
    import imageio_ffmpeg
except Exception as exc:  # deps missing -> tell the caller cleanly, don't crash
    sys.stdout.write(
        json.dumps({"event": "fatal", "message": "missing deps: %s" % exc}) + "\n")
    sys.stdout.flush()
    sys.exit(2)

SR = 22050
WIN = 2048
HOP = 1024


def emit(obj):
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def decode(path):
    """Decode any Mixxx-supported file to mono float32 PCM via bundled ffmpeg."""
    ff = imageio_ffmpeg.get_ffmpeg_exe()
    proc = subprocess.run(
        [ff, "-v", "quiet", "-i", path, "-ac", "1", "-ar", str(SR), "-f", "f32le", "-"],
        stdout=subprocess.PIPE,
        check=True,
    )
    return np.frombuffer(proc.stdout, dtype=np.float32)


def analyze(x):
    if len(x) < WIN:
        return 0.0, []
    n = 1 + (len(x) - WIN) // HOP
    frames = np.lib.stride_tricks.as_strided(
        x, shape=(n, WIN), strides=(x.strides[0] * HOP, x.strides[0])
    ).copy()
    frames *= np.hanning(WIN).astype(np.float32)
    power = np.abs(np.fft.rfft(frames, axis=1)) ** 2
    total = power.sum(axis=1) + 1e-9
    rms = np.sqrt(total)

    # Overall energy: absolute mean RMS. Kept on one scale across every track the
    # worker sees, so the module's library-relative percentile ranks them fairly.
    # This is the real win — the native heuristic scales energy by the waveform's
    # theoretical max, squeezing real music into a narrow band.
    overall = float(rms.mean())

    # Shape of the journey, peak-normalized, downsampled to ~64 points.
    peak = rms.max() + 1e-9
    curve = rms / peak
    k = max(1, len(curve) // 64)
    down = [round(float(v), 4) for v in curve[::k][:64]]

    return overall, down


def main():
    if len(sys.argv) < 2:
        emit({"event": "fatal", "message": "usage: worker.py <request.json>"})
        return 2
    try:
        with open(sys.argv[1], encoding="utf-8") as f:
            req = json.load(f)
    except Exception as exc:
        emit({"event": "fatal", "message": "cannot read request: %s" % exc})
        return 2

    tracks = req.get("tracks", [])
    total = len(tracks)
    for i, t in enumerate(tracks):
        emit({"event": "progress", "done": i, "total": total})
        try:
            overall, curve = analyze(decode(t["path"]))
            emit({
                "event": "track",
                "id": t.get("id"),
                "overallEnergy": overall,
                "energyCurve": curve,
            })
        except Exception as exc:
            sys.stderr.write("track %s failed: %s\n" % (t.get("id"), exc))
            emit({"event": "trackError", "id": t.get("id"), "message": str(exc)})
    emit({"event": "done", "total": total})
    return 0


if __name__ == "__main__":
    sys.exit(main())
