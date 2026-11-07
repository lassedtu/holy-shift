"""Make a loop of the tyre screech for the game from a recording.

The script needs the NumPy package.

Run this command from the project root:

    python3 scripts/screech_loop.py

The script reads assets_raw/car-tire-screech.wav. It keeps the loud middle
part, makes the loudness constant, joins the end to the start with a
crossfade and writes a mono loop to assets/sounds/tire_screech.wav.
"""
import argparse
import os
import wave

import numpy as np

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

START_SECONDS = 0.25     # Start of the loud part of the recording.
LOOP_SECONDS = 0.35      # Length of the loop.
FADE_SECONDS = 0.08      # Length of the crossfade between the end and the start.
ENVELOPE_SECONDS = 0.03  # Window for the loudness that is made constant.
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


def flatten(data, rate):
    """Divide by the moving loudness, so that the swell of the recording is gone."""
    window = max(1, int(ENVELOPE_SECONDS * rate))
    power = np.convolve(data ** 2, np.ones(window) / window, mode="same")
    envelope = np.sqrt(np.maximum(power, 1e-8))
    return data / np.maximum(envelope, envelope.max() * 0.05)


def make_loop(data, rate):
    start = int(START_SECONDS * rate)
    length = int(LOOP_SECONDS * rate)
    fade = int(FADE_SECONDS * rate)
    if start + length + fade > len(data):
        raise SystemExit("the recording is too short for the loop settings")
    part = data[start:start + length + fade]
    loop = part[:length].copy()
    t = np.linspace(0.0, 1.0, fade, endpoint=False)
    # Equal power crossfade: the tail after the loop fades out over the head of the loop.
    loop[:fade] = part[:fade] * np.sin(t * np.pi / 2) + part[length:length + fade] * np.cos(t * np.pi / 2)
    return loop


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default=os.path.join(PROJECT_ROOT, "assets_raw", "car-tire-screech.wav"))
    parser.add_argument("--out", default=os.path.join(PROJECT_ROOT, "assets", "sounds", "tire_screech.wav"))
    args = parser.parse_args()

    data, rate = read_mono(args.input)
    loop = make_loop(flatten(data, rate), rate)
    loop = loop - loop.mean()
    loop *= PEAK / np.abs(loop).max()
    write_mono(args.out, loop, rate)
    print(f"wrote {args.out}: {len(loop) / rate:.2f} s at {rate} Hz")


if __name__ == "__main__":
    main()
