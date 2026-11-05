"""Build smaller tree sheets from the full size sheets of tree_spritemap.py.

The script does not need Blender. It needs the Pillow and NumPy packages.

Run this command from the project root:

    python3 scripts/tree_lods.py --out assets/trees

The script reads each <tree>.json and <tree>.png in the folder, for example
spruce_v1. For each one it writes these files:

    <tree>_lod1.png / .json   Half size. 16 directions. 2 camera angles.
    <tree>_lod2.png / .json   Quarter size. 4 directions. 1 camera angle.

The output colors come from the sprite sheet palette.
"""
import argparse
import json
import math
import os

import numpy as np
from PIL import Image

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Each LOD entry: divisor, kept directions, and kept camera-angle rows.
LODS = [
    ("_lod1", 2, 3, [0, 1]),
    ("_lod2", 4, 12, [0]),
]

ALPHA_LIMIT = 128


def read_palette(meta):
    colors = [[int(c[i:i + 2], 16) for i in (1, 3, 5)] for c in meta["palette"]]
    return np.array(colors, dtype=np.float64)


def snap(rgb, palette):
    """Return the palette color closest to each pixel color."""
    flat = rgb.reshape(-1, 3).astype(np.float64)
    distance = ((flat[:, None, :] - palette[None]) ** 2).sum(-1)
    return palette[distance.argmin(1)].reshape(rgb.shape)


def reduce_cell(cell, factor, palette):
    """Shrink a cell by an integer factor. A block stays solid when at least half of its pixels are solid."""
    h, w, _ = cell.shape
    blocks = cell.reshape(h // factor, factor, w // factor, factor, 4).astype(np.float64)
    solid = blocks[..., 3] >= ALPHA_LIMIT
    count = solid.sum(axis=(1, 3))
    total = (blocks[..., :3] * solid[..., None]).sum(axis=(1, 3))
    rgb = total / np.maximum(count, 1)[..., None]
    out = np.zeros((h // factor, w // factor, 4), dtype=np.uint8)
    keep = count * 2 >= factor * factor
    out[..., :3] = np.where(keep[..., None], snap(rgb, palette), 0).astype(np.uint8)
    out[..., 3] = np.where(keep, 255, 0)
    return out


def make_lod(sheet, meta, palette, name, factor, column_step, rows, out_dir):
    cw, ch = meta["cell_width"], meta["cell_height"]
    pw = int(math.ceil(cw / factor)) * factor
    ph = int(math.ceil(ch / factor)) * factor
    pad_left = (pw - cw) // 2
    pad_top = ph - ch
    columns = list(range(0, meta["columns"], column_step))

    out_rows = []
    for row in rows:
        cells = []
        for col in columns:
            cell = sheet[row * ch:(row + 1) * ch, col * cw:(col + 1) * cw]
            padded = np.zeros((ph, pw, 4), dtype=np.uint8)
            padded[pad_top:, pad_left:pad_left + cw] = cell
            cells.append(reduce_cell(padded, factor, palette))
        out_rows.append(np.concatenate(cells, axis=1))
    image = np.concatenate(out_rows, axis=0)
    Image.fromarray(image, "RGBA").save(os.path.join(out_dir, name + ".png"))

    # The trunk base is the same for all directions in one row.
    anchor_x = [round((meta["row_anchor_x"][r] + pad_left) / factor, 2) for r in rows]
    anchor_y = [round((meta["row_anchor_y"][r] + pad_top) / factor, 2) for r in rows]
    out_meta = {
        "image": name + ".png",
        "cell_width": pw // factor,
        "cell_height": ph // factor,
        "columns": len(columns),
        "rows": len(rows),
        "azimuth_step_deg": meta["azimuth_step_deg"] * column_step,
        "look_up_deg": [meta["look_up_deg"][r] for r in rows],
        "pixels_per_unit": meta["pixels_per_unit"] / factor,
        "tree_height_units": meta["tree_height_units"],
        "row_anchor_x": anchor_x,
        "row_anchor_y": anchor_y,
    }
    with open(os.path.join(out_dir, name + ".json"), "w") as f:
        json.dump(out_meta, f, indent=2)
    print(f"Wrote {name}.png ({image.shape[1]}x{image.shape[0]})")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=os.path.join(PROJECT_ROOT, "assets", "trees"))
    out_dir = os.path.abspath(parser.parse_args().out)

    # The full size sheets have no _lod in the name
    names = sorted(f[:-5] for f in os.listdir(out_dir)
                   if f.endswith(".json") and "_lod" not in f)
    for tree in names:
        with open(os.path.join(out_dir, tree + ".json")) as f:
            meta = json.load(f)
        sheet = np.asarray(Image.open(os.path.join(out_dir, tree + ".png")).convert("RGBA"))
        palette = read_palette(meta)
        for suffix, factor, column_step, rows in LODS:
            make_lod(sheet, meta, palette, tree + suffix, factor, column_step, rows, out_dir)


main()
