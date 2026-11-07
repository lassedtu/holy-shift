"""Make the pixel art portraits of the story characters.

The script needs the Pillow package.

Run this command from the project root:

    python3 scripts/portraits.py

Each portrait is 40 x 40 pixels. The head, the hair and the clothes are
filled shapes with a dark outline. The eyebrows, the eyes and the mouth are
small stamps that change with the mood. So all the characters and all the
moods have the same style, and a new mood only needs new stamps.

The script writes assets/story/portraits/<name>.png (the normal mood) and
<name>_<mood>.png for each mood in MOODS. Use --sheet FILE to also write a
large picture of all the portraits, to look at them together.
"""
import argparse
import os

from PIL import Image

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(PROJECT_ROOT, "assets", "story", "portraits")
SIZE = 40

# The colors that all the characters use. Each character adds its own colors (see CHARACTERS).
BASE_PALETTE = {
    "B": (78, 104, 136),   # background
    "K": (38, 26, 30),     # outline
    "E": (34, 22, 26),     # eye
    "W": (250, 248, 240),  # white of the eye, glint, shirt
    "M": (150, 56, 60),    # inside of the mouth
    "G": (70, 56, 66),     # frame of the glasses
    "g": (196, 222, 236),  # glass
    "r": (222, 140, 126),  # cheek
    "y": (226, 186, 70),   # gold
}
SKIN = {"S": (236, 186, 152), "s": (206, 146, 118)}
SICK_SKIN = {"S": (196, 206, 150), "s": (156, 170, 112)}  # a green face

MOODS = ["angry", "sad", "scared", "sick", "smug", "smile", "cold", "brave"]

# The left eye is at x 15, the right eye at x 23, the eyes are at y 18, the mouth is at y 26.
# Each mood: (left brow, right brow, y of the brows, eye, mouth, x of the mouth)
FEATURES = {
    "normal": ([".DDDD."], [".DDDD."], 15, ["EE", "EE"], ["KKKK"], 18),
    "angry": (["DD....", ".DDD..", "...DD."], ["....DD", "..DDD.", ".DD..."], 14, ["..", "EE"], [".KKKK.", "K....K"], 17),
    "sad": (["....DD", "..DD..", "DD...."], ["DD....", "..DD..", "....DD"], 13, ["..", "EE"], [".KK.", "K..K"], 18),
    "scared": ([".DDDD."], [".DDDD."], 13, ["WW.", "WEW", ".WW"], [".KK.", "KMMK", ".KK."], 18),
    "sick": (["....DD", "DDDD.."], ["DD....", "..DDDD"], 14, ["..", "KK"], ["K.KK.K", ".K..K."], 17),
    "smug": ([".DDDD."], ["DDDD.."], 14, ["KK", "EE"], ["....K", "KKKK."], 18),
    "smile": ([".DDDD."], [".DDDD."], 15, ["EE", "EE"], ["K....K", ".KKKK."], 17),
    "cold": (["DDDDD."], [".DDDDD"], 16, ["..", "EE"], ["KKK"], 19),
    "brave": (["DD....", "..DDD."], ["....DD", ".DDD.."], 14, ["EE", "EE"], ["KKKKK"], 18),
}
# The smug right brow is lower than the left brow
LOWER_RIGHT_BROW = {"smug": 2}


class Canvas:
    def __init__(self):
        self.px = [["B"] * SIZE for _ in range(SIZE)]

    def set(self, x, y, c):
        if 0 <= x < SIZE and 0 <= y < SIZE and c != ".":
            self.px[y][x] = c

    def shape(self, mask, fill, shade=None, shade_rule=None):
        """Fill a mask with a color, shade a part of it, and draw a dark outline at its edge."""
        for y in range(SIZE):
            for x in range(SIZE):
                if not mask(x, y):
                    continue
                c = shade if shade and shade_rule and shade_rule(x, y) else fill
                inside = all(mask(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                self.set(x, y, c if inside else "K")

    def stamp(self, x0, y0, rows):
        for dy, row in enumerate(rows):
            for dx, c in enumerate(row):
                self.set(x0 + dx, y0 + dy, c)

    def image(self, palette):
        im = Image.new("RGB", (SIZE, SIZE))
        for y in range(SIZE):
            for x in range(SIZE):
                im.putpixel((x, y), palette[self.px[y][x]])
        return im


def ellipse(cx, cy, rx, ry):
    return lambda x, y: ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1.0


def face_features(c, mood, glasses=False):
    left, right, brow_y, eye, mouth, mouth_x = FEATURES[mood]
    c.stamp(13, brow_y, left)
    c.stamp(21, brow_y + LOWER_RIGHT_BROW.get(mood, 0), right)
    if glasses:
        frame = [".GGGG.", "GggggG", "GggggG", ".GGGG."]
        c.stamp(13, 17, frame)
        c.stamp(21, 17, frame)
        c.stamp(19, 18, ["GG"])
    eye_x = 14 if len(eye[0]) == 3 else 15
    c.stamp(eye_x, 18 if len(eye) < 3 else 17, eye)
    c.stamp(eye_x + 8, 18 if len(eye) < 3 else 17, eye)
    if glasses:
        c.stamp(14, 18, ["W"])
        c.stamp(22, 18, ["W"])
    c.stamp(mouth_x, 26, mouth)
    c.stamp(19, 22, [".s", "ss"])  # nose


def shoulders(c, fill, shade):
    body = lambda x, y: y >= 33 and abs(x + 0.5 - 20) <= 6 + (y - 33) * 2.2
    c.shape(body, fill, shade, lambda x, y: x < 14 or x > 26)


def neck(c):
    c.shape(lambda x, y: 17 <= x <= 22 and 29 <= y <= 33, "S", "s", lambda x, y: y <= 30)


def rajka(c, mood):
    shoulders(c, "P", "p")
    c.stamp(15, 33, ["..CCC.CCC..", "...CCCCC..."])  # collar
    neck(c)
    c.shape(ellipse(20, 19, 11.5, 11), "H", "h", lambda x, y: x < 13 or y > 25)  # hair behind the head
    c.shape(ellipse(20, 6.5, 5.5, 4.5), "H", "h", lambda x, y: y > 8 or x < 17)  # bun
    c.shape(ellipse(20, 21.5, 8.5, 10), "S", "s", lambda x, y: x < 13 or y > 28)  # face
    fringe = lambda x, y: ellipse(20, 11.5, 10, 4.5)(x, y) and y <= 13
    c.shape(fringe, "H", "h", lambda x, y: y >= 12 and 18 <= x <= 21)
    c.stamp(12, 24, ["r"])
    c.stamp(27, 24, ["r"])
    c.stamp(14, 26, ["s"])  # wrinkles
    c.stamp(25, 26, ["s"])
    face_features(c, mood, glasses=True)


def jelka(c, mood):
    shoulders(c, "J", "j")
    c.stamp(14, 33, ["ZZZ.....ZZZ", ".ZZZ...ZZZ."])  # green collar of the Zlo jacket
    c.stamp(25, 36, ["ZZ", "ZZ"])  # Zlo patch
    neck(c)
    bob = lambda x, y: ellipse(20, 19, 11, 10.5)(x, y) and y <= 27
    c.shape(bob, "H", "h", lambda x, y: x < 12 or y > 24)
    c.shape(ellipse(20, 21.5, 7.8, 10), "S", "s", lambda x, y: x < 14 or y > 28)  # a narrow face
    # fringe swept to the right side
    fringe = lambda x, y: ellipse(19, 11.5, 10.5, 4.5)(x, y) and y <= 13.5 - max(0, x - 16) * 0.3
    c.shape(fringe, "H", "h", lambda x, y: y >= 12 and x < 17)
    c.stamp(14, 24, ["s"])  # cheekbones
    c.stamp(25, 24, ["s"])
    c.stamp(11, 24, ["y"])  # earrings
    c.stamp(28, 24, ["y"])
    face_features(c, mood)


def luka(c, mood):
    shoulders(c, "L", "l")
    c.stamp(17, 34, ["C....C", "C....C"])  # strings of the hoodie
    c.stamp(14, 33, ["ll........ll"])
    neck(c)
    c.shape(ellipse(20, 17, 11, 10), "N", "n", lambda x, y: x < 13 or y > 22)  # hair behind the head
    c.shape(ellipse(20, 21.5, 8.5, 9.5), "S", "s", lambda x, y: x < 13 or y > 28)  # face
    # messy fringe with points at the bottom
    fringe = lambda x, y: ellipse(20, 12, 10.5, 5.5)(x, y) and y <= 14 + (1 if x % 3 == 0 else 0) - (1 if x % 4 == 1 else 0)
    c.shape(fringe, "N", "n", lambda x, y: y >= 13)
    c.stamp(18, 6, ["N.N", "NNN"])  # hair that sticks up
    c.stamp(14, 23, ["r.r"])  # freckles
    c.stamp(24, 23, ["r.r"])
    face_features(c, mood)


def zlo(c, mood):
    shoulders(c, "U", "u")
    c.stamp(17, 33, ["WW..WW", ".WZZW.", "..ZZ..", "..ZZ..", "..ZZ..", "..ZZ.."])  # shirt and green tie
    neck(c)
    c.shape(ellipse(20, 21, 9, 10.5), "S", "s", lambda x, y: x < 13 or y > 28)  # a wide face
    # slicked hair with a side part
    hair = lambda x, y: ellipse(20, 14, 10, 6.5)(x, y) and (y <= 13 or x <= 11 or x >= 28) and y <= 19
    c.shape(hair, "X", "x", lambda x, y: y <= 10 and 15 <= x <= 25)
    c.stamp(15, 8, ["KK"])  # the part
    c.stamp(16, 29, ["s.s.s.s.s"])  # stubble
    face_features(c, mood)


CHARACTERS = {
    "rajka": (rajka, {"H": (214, 214, 222), "h": (164, 164, 180), "D": (96, 92, 108),
                      "P": (128, 72, 146), "p": (92, 50, 110), "C": (238, 232, 214)}),
    "jelka": (jelka, {"H": (138, 140, 156), "h": (98, 100, 116), "D": (60, 58, 70),
                      "J": (44, 44, 52), "j": (30, 30, 36), "Z": (84, 206, 114)}),
    "luka": (luka, {"N": (120, 76, 44), "n": (86, 52, 30), "D": (86, 52, 30),
                    "L": (70, 140, 80), "l": (50, 106, 60), "C": (238, 232, 214)}),
    "zlo": (zlo, {"X": (40, 36, 44), "x": (88, 88, 104), "D": (40, 36, 44),
                  "U": (64, 66, 78), "u": (46, 48, 58), "Z": (84, 206, 114)}),
}


def portrait(name, mood):
    draw, colors = CHARACTERS[name]
    c = Canvas()
    draw(c, mood)
    palette = dict(BASE_PALETTE)
    palette.update(SICK_SKIN if mood == "sick" else SKIN)
    palette.update(colors)
    return c.image(palette)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sheet", help="also write a picture of all the portraits to this file")
    parser.add_argument("--out", default=OUT)
    args = parser.parse_args()

    os.makedirs(args.out, exist_ok=True)
    moods = ["normal"] + MOODS
    scale = 4
    sheet = Image.new("RGB", (len(moods) * (SIZE * scale + 4) + 4, len(CHARACTERS) * (SIZE * scale + 4) + 4), (20, 20, 24))
    for row, name in enumerate(CHARACTERS):
        for column, mood in enumerate(moods):
            image = portrait(name, mood)
            file = f"{name}.png" if mood == "normal" else f"{name}_{mood}.png"
            image.save(os.path.join(args.out, file))
            sheet.paste(image.resize((SIZE * scale, SIZE * scale), Image.NEAREST),
                        (4 + column * (SIZE * scale + 4), 4 + row * (SIZE * scale + 4)))
    print(f"wrote {len(CHARACTERS) * len(moods)} portraits to {args.out}")
    if args.sheet:
        sheet.save(args.sheet)


if __name__ == "__main__":
    main()
