#!/usr/bin/env python3
"""Generate rack's small UI sounds. No external samples, no licensing baggage."""

import math
import random
import struct
import wave
from pathlib import Path

RATE = 44100
OUT = Path(__file__).resolve().parent.parent / "sounds"


def write(name, samples, gain=0.8):
    OUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT / name), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(RATE)
        frames = bytearray()
        for value in samples:
            clipped = max(-1.0, min(1.0, value * gain))
            frames += struct.pack("<h", int(clipped * 32767))
        handle.writeframes(frames)


def envelope(t, decay):
    return math.exp(-t * decay)


def click():
    random.seed(7)
    out = []
    for i in range(int(RATE * 0.055)):
        t = i / RATE
        tone = math.sin(2 * math.pi * 2100 * t) * 0.45
        tone += math.sin(2 * math.pi * 3300 * t) * 0.2
        noise = (random.random() * 2 - 1) * 0.55
        out.append((tone + noise) * envelope(t, 95))
    return out


def rail():
    random.seed(11)
    out = []
    for i in range(int(RATE * 0.10)):
        t = i / RATE
        tone = math.sin(2 * math.pi * 170 * t) * 0.55
        tone += math.sin(2 * math.pi * 310 * t) * 0.2
        noise = (random.random() * 2 - 1) * 0.25
        out.append((tone + noise) * envelope(t, 42))
    return out


def pocket():
    random.seed(19)
    out = []
    for i in range(int(RATE * 0.24)):
        t = i / RATE
        sweep = 220 - 130 * (t / 0.24)
        tone = math.sin(2 * math.pi * sweep * t) * 0.55
        tone += math.sin(2 * math.pi * 90 * t) * 0.25
        noise = (random.random() * 2 - 1) * 0.2 * envelope(t, 30)
        out.append((tone + noise) * envelope(t, 14) * min(1.0, t / 0.006))
    return out


if __name__ == "__main__":
    write("click.wav", click())
    write("rail.wav", rail())
    write("pocket.wav", pocket())
    print("wrote", OUT)
