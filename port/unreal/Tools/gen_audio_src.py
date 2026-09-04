"""Generates the synthesized radio-net source WAVs (BUILDER-BRIDGE, P4).

Deterministic (fixed seed), stdlib-only (wave/struct/random) - run with any
Python 3; THIS FILE is the source of truth for the two generated WAVs in
audio_src/ (they are checked in as import sources for gen_audio_assets.py,
regenerate-at-will):

  squelch_click.wav  the radio squelch cue framing FDC lines: 10 ms noise
                     transient + short tail, BANDPASSED 300-3000 Hz with a
                     light tanh soft-clip - the radio DSP character the P4
                     audio brief asks for, baked into the cue itself (the
                     actual FDC voice is P7; until then lines are text +
                     this cue).
  net_hiss_loop.wav  4 s seamless low-level noise bed (the idle net). Same
                     bandpass so it sits in the same "radio" register.

The distant boom is NOT generated - it is a CC0 recording (see
port/CREDITS.md), synthesis being the fallback only where downloads failed.

Usage:  python port/unreal/Tools/gen_audio_src.py
"""
import math
import os
import random
import struct
import wave

SR = 22050
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "audio_src")


class Biquad:
    """RBJ cookbook biquad (audio-eq-cookbook), direct form 1."""

    def __init__(self, b0, b1, b2, a0, a1, a2):
        self.b = (b0 / a0, b1 / a0, b2 / a0)
        self.a = (a1 / a0, a2 / a0)
        self.x1 = self.x2 = self.y1 = self.y2 = 0.0

    def run(self, x):
        y = (self.b[0] * x + self.b[1] * self.x1 + self.b[2] * self.x2
             - self.a[0] * self.y1 - self.a[1] * self.y2)
        self.x2, self.x1 = self.x1, x
        self.y2, self.y1 = self.y1, y
        return y


def highpass(fc, q=0.707):
    w = 2 * math.pi * fc / SR
    cw, sw = math.cos(w), math.sin(w)
    alpha = sw / (2 * q)
    return Biquad((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + alpha, -2 * cw, 1 - alpha)


def lowpass(fc, q=0.707):
    w = 2 * math.pi * fc / SR
    cw, sw = math.cos(w), math.sin(w)
    alpha = sw / (2 * q)
    return Biquad((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + alpha, -2 * cw, 1 - alpha)


def radio_chain(samples):
    """300-3000 Hz bandpass + light distortion (the P4 radio DSP spec)."""
    hp, lp = highpass(300.0), lowpass(3000.0)
    out = []
    for s in samples:
        s = lp.run(hp.run(s))
        out.append(math.tanh(s * 2.2) * 0.75)   # light soft-clip
    return out


def write_wav(name, samples):
    path = os.path.join(OUT, name)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        frames = b"".join(
            struct.pack("<h", max(-32767, min(32767, int(s * 32767))))
            for s in samples)
        w.writeframes(frames)
    print("wrote", path, len(samples) / SR, "s")


def gen_squelch():
    rng = random.Random(0x51F1)
    n = int(0.09 * SR)                     # 90 ms
    raw = []
    for i in range(n):
        t = i / SR
        if t < 0.010:                      # sharp click transient
            env = 1.0
        elif t < 0.070:                    # noise tail
            env = 0.35 * math.exp(-(t - 0.010) * 55.0)
        else:                              # hard gate at the end (squelch!)
            env = 0.0
        raw.append(env * (rng.random() * 2 - 1))
    write_wav("squelch_click.wav", radio_chain(raw))


def gen_hiss():
    rng = random.Random(0xB0B0)
    n = int(4.0 * SR)
    fade = int(0.1 * SR)
    raw = [0.16 * (rng.random() * 2 - 1) for _ in range(n + fade)]
    shaped = radio_chain(raw)
    # seamless loop: crossfade the tail into the head, drop the tail
    for i in range(fade):
        a = i / fade
        shaped[i] = shaped[i] * a + shaped[n + i] * (1 - a)
    write_wav("net_hiss_loop.wav", shaped[:n])


if __name__ == "__main__":
    gen_squelch()
    gen_hiss()
