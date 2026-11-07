"""Make the pixel art of the story from drawings.

The script needs the Pillow and NumPy packages.

Run this command from the project root:

    python3 scripts/pixelate.py

The script reads the pictures in assets_raw/scene_images/ and the portraits
in assets_raw/portraits/ (PNG or JPG, also in folders, for example
assets_raw/scene_images/00/00_garage.jpg). The file name is the name of the
picture in the scene. For each file it:

1. makes the drawing smaller: a picture becomes as small as possible while
   it still covers 320 x 180 (a 16:9 drawing becomes 320 x 180, a 3:2 photo
   becomes 320 x 213), a portrait becomes 40 x 40 pixels (the middle square
   of the drawing),
2. uses fewer colors (--colors, 32 when you do not give it),
3. writes a PNG file with the same name to assets/story/images/ or
   assets/story/portraits/.

A picture that is wider or higher than 320 x 180 can move on the screen
with the "pan" command of a scene.

Options:
    --colors N       the number of colors in each picture
    --dither         mix two colors in a pattern of dots where the picture
                     has a color between them
    --palette FILE   use only the colors of this image, so all pictures use
                     the same colors (for example a small PNG with one pixel
                     for each color)
    --contrast       make the darkest color black and the lightest color
                     white, for a photo of a drawing on paper
    --smooth         remove the fine detail of a photo first, so the result
                     has flat areas of color, like pixel art
    --sepia          brown and faded, like an old photo (for the flashbacks)
    --only NAME      make only the drawing with this name
"""
import argparse
import os

import numpy as np
from PIL import Image, ImageFilter, ImageOps

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PICTURE_WIDTH = 320
PICTURE_HEIGHT = 180
PORTRAIT_SIZE = 40
EXTENSIONS = (".png", ".jpg", ".jpeg")

# Threshold of each pixel in a 4 x 4 pattern, from 0 to 1
BAYER = (np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) + 0.5) / 16.0
SMOOTH_SIZE = 960  # a photo is made this small before it is smoothed
DITHER_STRENGTH = 0.12  # how far the pattern moves a color, as part of the full range


def load(path, args):
    image = Image.open(path)
    image = ImageOps.exif_transpose(image).convert("RGBA")
    rgb = image.convert("RGB")
    if args.smooth:
        # Remove the fine detail of a photo (grain, leaves, texture), so the result has flat areas
        small = rgb.copy()
        small.thumbnail((SMOOTH_SIZE, SMOOTH_SIZE), Image.Resampling.LANCZOS)
        rgb = small.filter(ImageFilter.MedianFilter(5)).filter(ImageFilter.MedianFilter(5))
        image = image.resize(rgb.size, Image.Resampling.LANCZOS)
    if args.contrast:
        rgb = ImageOps.autocontrast(rgb, cutoff=1)
    if args.sepia:
        gray = ImageOps.autocontrast(rgb.convert("L"), cutoff=2)
        rgb = ImageOps.colorize(gray, black=(52, 34, 24), white=(240, 222, 186), mid=(156, 118, 80))
    return Image.merge("RGBA", (*rgb.split(), image.getchannel("A")))


def resize_picture(image):
    """Make the picture as small as possible while it still covers the whole screen."""
    scale = max(PICTURE_WIDTH / image.width, PICTURE_HEIGHT / image.height)
    size = (max(PICTURE_WIDTH, round(image.width * scale)), max(PICTURE_HEIGHT, round(image.height * scale)))
    return image.resize(size, Image.Resampling.BOX)


def resize_portrait(image):
    side = min(image.width, image.height)
    left = (image.width - side) // 2
    top = (image.height - side) // 2
    square = image.crop((left, top, left + side, top + side))
    return square.resize((PORTRAIT_SIZE, PORTRAIT_SIZE), Image.Resampling.BOX)


def palette_of(image, colors):
    """Return the colors (K x 3, from 0 to 1) that show the image best."""
    small = image.convert("RGB").quantize(colors=colors, method=Image.Quantize.MEDIANCUT, kmeans=4)
    table = np.array(small.getpalette()[: colors * 3], dtype=np.float64).reshape(-1, 3) / 255.0
    used = np.unique(np.array(small))
    return table[used]


def palette_from_file(path):
    pixels = np.array(Image.open(path).convert("RGB"), dtype=np.float64).reshape(-1, 3) / 255.0
    return np.unique(pixels, axis=0)


def map_to_palette(image, palette, dither):
    """Give each pixel the nearest color of the palette."""
    rgba = np.array(image, dtype=np.float64) / 255.0
    rgb = rgba[:, :, :3]
    if dither:
        h, w = rgb.shape[:2]
        pattern = np.tile(BAYER, (h // 4 + 1, w // 4 + 1))[:h, :w]
        rgb = rgb + (pattern[:, :, None] - 0.5) * DITHER_STRENGTH
    distance = ((rgb[:, :, None, :] - palette[None, None, :, :]) ** 2).sum(axis=3)
    result = palette[distance.argmin(axis=2)]
    alpha = (rgba[:, :, 3:] >= 0.5).astype(np.float64)
    out = np.concatenate([result, alpha], axis=2)
    return Image.fromarray((out * 255.0 + 0.5).astype(np.uint8), "RGBA")


def convert(source_dir, target_dir, resize, args):
    if not os.path.isdir(source_dir):
        return
    os.makedirs(target_dir, exist_ok=True)
    fixed = palette_from_file(args.palette) if args.palette else None
    files = sorted(os.path.join(root, file) for root, _, names in os.walk(source_dir) for file in names)
    for source in files:
        name, extension = os.path.splitext(os.path.basename(source))
        if extension.lower() not in EXTENSIONS or (args.only and args.only != name):
            continue
        image = resize(load(source, args))
        palette = fixed if fixed is not None else palette_of(image, args.colors)
        result = map_to_palette(image, palette, args.dither)
        path = os.path.join(target_dir, name + ".png")
        result.save(path)
        print(f"wrote {path}: {result.width} x {result.height}, {len(palette)} colors")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pictures", default=os.path.join(PROJECT_ROOT, "assets_raw", "scene_images"))
    parser.add_argument("--portraits", default=os.path.join(PROJECT_ROOT, "assets_raw", "portraits"))
    parser.add_argument("--out", default=os.path.join(PROJECT_ROOT, "assets", "story"))
    parser.add_argument("--colors", type=int, default=32)
    parser.add_argument("--dither", action="store_true")
    parser.add_argument("--palette")
    parser.add_argument("--contrast", action="store_true")
    parser.add_argument("--smooth", action="store_true")
    parser.add_argument("--sepia", action="store_true")
    parser.add_argument("--only")
    args = parser.parse_args()

    convert(args.pictures, os.path.join(args.out, "images"), resize_picture, args)
    convert(args.portraits, os.path.join(args.out, "portraits"), resize_portrait, args)


if __name__ == "__main__":
    main()
