"""Make the gear change sound for the game from a recording.

The script needs the NumPy package.

Run this command from the project root:

    python3 scripts/gear_sound.py

The script reads assets_raw/gear-select.wav. It keeps one part of the
recording, fades the start and the end, makes it louder and writes a mono
file to assets/sounds/gear_change.wav.
"""
import argparse
import os
import wave

import numpy as np

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

START_SECONDS = 0.28     # Start of the part that is kept.
END_SECONDS = 0.80       # End of the part that is kept.
FADE_IN_SECONDS = 0.005
FADE_OUT_SECONDS = 0.15
PEAK = 0.8               # Peak of the output.


def read_mono(path):
    with wave.open(path) as w:
        if w.getsampwidth() != 2:
            raise SystemExit(f"{path}: only 16 bit files are supported")
        data = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16)
        data = data.astype(np.float64) / 32768.0
        return data.reshape(-1, w.getnchannels()).mean(axis=1), w.getframerate()


def write_mono(path, data, rate):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes((np.clip(data, -1.0, 1.0) * 32767.0).astype(np.int16).tobytes())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default=os.path.join(PROJECT_ROOT, "assets_raw", "gear-select.wav"))
    parser.add_argument("--out", default=os.path.join(PROJECT_ROOT, "assets", "sounds", "gear_change.wav"))
    args = parser.parse_args()

    data, rate = read_mono(args.input)
    part = data[int(START_SECONDS * rate):int(END_SECONDS * rate)].copy()
    part -= part.mean()
    fade_in = int(FADE_IN_SECONDS * rate)
    fade_out = int(FADE_OUT_SECONDS * rate)
    part[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
    part[-fade_out:] *= np.linspace(1.0, 0.0, fade_out) ** 2
    part *= PEAK / np.abs(part).max()
    write_mono(args.out, part, rate)
    print(f"wrote {args.out}: {len(part) / rate:.2f} s at {rate} Hz")


if __name__ == "__main__":
    main()
