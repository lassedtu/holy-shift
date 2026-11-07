"""Make the short sounds of the game from recordings.

The script needs the NumPy package.

Run this command from the project root:

    python3 scripts/sound_clips.py

For each sound in CLIPS, the script reads the recording in assets_raw/. It
keeps one part of the recording, fades the start and the end, makes it louder
and writes a 16 bit mono file to assets/sounds/. Change the times in CLIPS to
use a different part of a recording.
"""
import argparse
import os
import wave

import numpy as np

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Output name: (recording, start seconds, end seconds, fade out seconds)
CLIPS = {
    "gear_change": ("gear-select.wav", 0.28, 0.80, 0.15),
    "crash": ("car-crash.wav", 0.36, 3.40, 1.0),
}

FADE_IN_SECONDS = 0.005
PEAK = 0.8  # Peak of the output.


def read_mono(path):
    with wave.open(path) as w:
        width = w.getsampwidth()
        raw = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8)
        channels = w.getnchannels()
        rate = w.getframerate()
    if width == 2:
        data = raw.view(np.int16).astype(np.float64) / 32768.0
    elif width == 3:
        b = raw.reshape(-1, 3).astype(np.int32)
        value = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        data = np.where(value >= 1 << 23, value - (1 << 24), value) / float(1 << 23)
    else:
        raise SystemExit(f"{path}: only 16 and 24 bit files are supported")
    return data.reshape(-1, channels).mean(axis=1), rate


def write_mono(path, data, rate):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes((np.clip(data, -1.0, 1.0) * 32767.0).astype(np.int16).tobytes())


def make_clip(data, rate, start, end, fade_out):
    part = data[int(start * rate):int(end * rate)].copy()
    part -= part.mean()
    fade_in = int(FADE_IN_SECONDS * rate)
    fade_out = int(fade_out * rate)
    part[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
    part[-fade_out:] *= np.linspace(1.0, 0.0, fade_out) ** 2
    return part * (PEAK / np.abs(part).max())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw", default=os.path.join(PROJECT_ROOT, "assets_raw"))
    parser.add_argument("--out", default=os.path.join(PROJECT_ROOT, "assets", "sounds"))
    args = parser.parse_args()

    for name, (recording, start, end, fade_out) in CLIPS.items():
        data, rate = read_mono(os.path.join(args.raw, recording))
        clip = make_clip(data, rate, start, end, fade_out)
        path = os.path.join(args.out, name + ".wav")
        write_mono(path, clip, rate)
        print(f"wrote {path}: {len(clip) / rate:.2f} s at {rate} Hz")


if __name__ == "__main__":
    main()
