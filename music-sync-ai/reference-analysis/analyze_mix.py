"""Objective feature analysis of a recorded DJ mix — a tool for studying real
sets to inform the music_sync engine. It measures, it does not listen.

Decodes the MP3 to mono 22.05 kHz via a bundled ffmpeg, then reports:
  1. energy envelope over time -> the journey shape and where it peaks
  2. energy dips                -> candidate transitions / breakdowns
  3. low-band behaviour at dips  -> bass management (bass swap / EQ kill)
  4. onset autocorrelation       -> global tempo and how steady it is

All timestamps are mm:ss so a finding can be checked by ear against the file.

Honest limits: energy alone cannot tell a track transition from an in-track
breakdown, cannot read the harmonic (key) moves, and cannot name the transition
type. Treat one set as a data point, not a law.

Usage:
    pip install numpy imageio-ffmpeg mutagen
    python analyze_mix.py "path/to/mix.mp3"
"""
import subprocess
import sys

import numpy as np
import imageio_ffmpeg
from mutagen.mp3 import MP3

sys.stdout.reconfigure(encoding="utf-8")  # so mm:ss and ≈ survive the console

SR = 22050


def mmss(seconds):
    seconds = int(round(seconds))
    return f"{seconds // 60}:{seconds % 60:02d}"


def analyze(path):
    dur = MP3(path).info.length
    print(f"Arquivo: {path.rsplit(chr(92), 1)[-1]}")
    print(f"  {dur / 60:.1f} min ({mmss(dur)}), {MP3(path).info.bitrate // 1000} kbps\n")

    ff = imageio_ffmpeg.get_ffmpeg_exe()
    proc = subprocess.run(
        [ff, "-v", "error", "-i", path, "-ac", "1", "-ar", str(SR), "-f", "f32le", "pipe:1"],
        stdout=subprocess.PIPE, check=True)
    x = np.frombuffer(proc.stdout, dtype=np.float32)

    win, hop = 1024, 512
    n_frames = 1 + (len(x) - win) // hop
    frames = np.lib.stride_tricks.as_strided(
        x, shape=(n_frames, win), strides=(x.strides[0] * hop, x.strides[0])).copy()
    frames *= np.hanning(win).astype(np.float32)

    power = np.abs(np.fft.rfft(frames, axis=1)) ** 2
    freqs = np.fft.rfftfreq(win, 1 / SR)
    energy = power.sum(axis=1)
    low = power[:, freqs < 250].sum(axis=1)
    frame_rate = SR / hop

    def db(v):
        return 10 * np.log10(v + 1e-9)

    def smooth(v, seconds):
        k = max(1, int(seconds * frame_rate))
        return np.convolve(v, np.ones(k) / k, mode="same")

    env_db = db(energy)
    env_db -= env_db.max()

    # 1. journey
    journey = smooth(energy, 6.0)
    journey = journey / journey.max()
    print("=== 1. FORMA DA JORNADA (energia por trecho, 0..1) ===")
    for i in range(12):
        a, b = i * len(journey) // 12, (i + 1) * len(journey) // 12
        v = journey[a:b].mean()
        print(f"  {mmss(a / frame_rate):>6}  {v:0.2f} |{'#' * int(round(v * 20))}")
    peak = int(np.argmax(smooth(energy, 20.0))) / len(energy)
    print(f"Pico de energia em ~{peak * 100:.0f}% do set ({mmss(peak * dur)})\n")

    # 2. dips
    med = smooth(env_db, 8.0)
    level = smooth(env_db, 45.0)
    depth = level - med
    dips = []
    i = int(30 * frame_rate)
    guard = int(25 * frame_rate)
    while i < len(depth) - int(5 * frame_rate):
        if depth[i] > 3.0:
            j0 = i
            while i < len(depth) and depth[i] > 1.5:
                i += 1
            center = (j0 + i) // 2
            dips.append((center / frame_rate, (i - j0) / frame_rate, depth[j0:i].max()))
            i = center + guard
        else:
            i += 1
    print(f"=== 2. QUEDAS DE ENERGIA ({len(dips)} candidatas a transição/breakdown) ===")
    for t, width, d in dips:
        print(f"  {mmss(t):>6}   ~{width:4.0f}s   -{d:0.1f} dB")
    widths = [w for _, w, _ in dips] or [0]
    print(f"\n~{dur / 60 / max(1, len(dips)):.1f} min entre quedas | "
          f"duração mediana da queda {np.median(widths):.0f}s\n")

    # 3. bass at dips
    bass_ratio = smooth(low, 2.0) / (smooth(energy, 2.0) + 1e-9)
    typical = np.median(bass_ratio)
    swaps = sum(
        1 for t, width, _ in dips
        if bass_ratio[max(0, int((t - max(width, 8)) * frame_rate)):
                      int((t + max(width, 8)) * frame_rate)].min() < typical * 0.6)
    print("=== 3. GRAVE NAS VIRADAS ===")
    print(f"proporção de grave típica: {typical:0.2f} | "
          f"{swaps}/{len(dips)} quedas com o grave despencando (bass swap/EQ kill)\n")

    # 4. tempo + stability
    onset = np.maximum(0, np.diff(smooth(energy, 0.05)))

    def tempo_of(seg):
        seg = seg - seg.mean()
        ac = np.correlate(seg, seg, mode="full")[len(seg) - 1:]
        lo, hi = int(frame_rate * 60 / 150), int(frame_rate * 60 / 110)
        return 60 * frame_rate / (lo + int(np.argmax(ac[lo:hi])))

    bpms = [tempo_of(onset[i * len(onset) // 8:(i + 1) * len(onset) // 8]) for i in range(8)]
    print("=== 4. TEMPO E ESTABILIDADE ===")
    print(f"tempo global: {tempo_of(onset):.1f} BPM | por trecho: "
          f"{'  '.join(f'{v:.0f}' for v in bpms)}")
    print(f"desvio: {np.std(bpms):.1f} BPM "
          f"({'muito estável — beatmatchado' if np.std(bpms) < 2 else 'varia ao longo do set'})")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("uso: python analyze_mix.py \"caminho/para/mix.mp3\"")
    analyze(sys.argv[1])
