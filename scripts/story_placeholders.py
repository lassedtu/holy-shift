"""Make placeholder pictures and portraits for the story from the scene files.

The script needs the Pillow package.

Run this command from the project root:

    python3 scripts/story_placeholders.py

For each "image <file>" command in assets/story/scenes/*.scene, the script
writes a picture of 320 x 120 pixels (the part above the dialogue) with the text of the "# picture:" and
"# search:" comments before the command. For each speaker in the scenes (also in the
lines that are comments) it writes a portrait with the name and the mood, but only when the
portrait does not exist (scripts/portraits.py makes the real portraits).

A picture or a portrait that has a file in assets_raw/scene_images/ or
assets_raw/portraits/ is not
changed, so the real art is never replaced. Run the script again after you
change the descriptions.
"""
import argparse
import glob
import os
import re
import textwrap

from PIL import Image, ImageDraw, ImageFont

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STORY = os.path.join(PROJECT_ROOT, "assets", "story")
RAW = {"pictures": os.path.join(PROJECT_ROOT, "assets_raw", "scene_images"),
       "portraits": os.path.join(PROJECT_ROOT, "assets_raw", "portraits")}
FONT = os.path.join(PROJECT_ROOT, "assets", "fonts", "Jersey15-Regular.ttf")
FONT_SIZE = 27  # sharp: one pixel of the font is one pixel of the picture
LINE_HEIGHT = 19

PICTURE_SIZE = (320, 120)  # the part of a picture above the dialogue
PORTRAIT_SIZE = (80, 80)  # drawn 1:1 in the portrait frame, a real portrait is 40 x 40

LINE = re.compile(r"^#?\s*([A-Za-z]+)\s*(?:\(([a-z ]+)\))?\s*:\s")
NOT_SPEAKERS = {"picture", "goal", "mood"}  # words of the comments at the top of a scene
COLORS = {
    "rajka": (110, 60, 130),
    "luka": (50, 110, 60),
    "jelka": (150, 40, 40),
    "zlo": (20, 20, 25),
}


def has_drawing(folder, name):
    stem = os.path.splitext(name)[0]
    return any(glob.glob(os.path.join(RAW[folder], "**", stem + ext), recursive=True) for ext in (".png", ".jpg", ".jpeg"))


def read_scenes():
    """Return the pictures [(scene, file, description, search)] and the portraits {file: (speaker, mood)}."""
    pictures = []
    portraits = {}
    for path in sorted(glob.glob(os.path.join(STORY, "scenes", "*.scene"))):
        scene = os.path.splitext(os.path.basename(path))[0]
        description = []
        search = ""
        for raw in open(path, encoding="utf-8"):
            line = raw.strip()
            if line.startswith("# picture:"):
                description.append(line[len("# picture:"):].strip())
            elif line.startswith("# search:"):
                search = line[len("# search:"):].strip()
            elif line.startswith("image "):
                pictures.append((scene, line[len("image "):].strip(), " ".join(description), search))
                description = []
                search = ""
            else:
                match = LINE.match(line)
                if match and match.group(1).lower() not in NOT_SPEAKERS:
                    speaker = match.group(1).lower()
                    mood = (match.group(2) or "").strip().replace(" ", "_")
                    portraits[speaker + ".png"] = (speaker, "")
                    if mood:
                        portraits[f"{speaker}_{mood}.png"] = (speaker, mood)
    return pictures, portraits


def draw_text(draw, font, lines, x, y, fill):
    for line in lines:
        draw.text((x, y), line, font=font, fill=fill)
        y += LINE_HEIGHT
    return y


def wrap(draw, font, text, width):
    """Break a text into lines that fit a width in pixels."""
    lines = []
    for paragraph in text.split("\n"):
        line = ""
        for word in paragraph.split():
            candidate = (line + " " + word).strip()
            if line and draw.textlength(candidate, font=font) > width:
                lines.append(line)
                line = word
            else:
                line = candidate
        lines.append(line)
    return lines


def make_picture(scene, name, description, search, font):
    flashback = "FLASHBACK" in description
    background = (92, 74, 52) if flashback else (34, 44, 60)
    image = Image.new("RGB", PICTURE_SIZE, background)
    draw = ImageDraw.Draw(image)
    draw.fontmode = "1"
    draw.rectangle([0, 0, PICTURE_SIZE[0] - 1, PICTURE_SIZE[1] - 1], outline=(200, 180, 120), width=2)
    title = os.path.splitext(name)[0]
    y = draw_text(draw, font, [title], 8, 2, (255, 204, 0))
    lines = wrap(draw, font, description or "(no description yet)", PICTURE_SIZE[0] - 16)
    search_lines = wrap(draw, font, "search: " + search, PICTURE_SIZE[0] - 16) if search else []
    room = (PICTURE_SIZE[1] - y - 4) // LINE_HEIGHT - len(search_lines)
    if len(lines) > room:
        print(f"  {scene}/{name}: the description has {len(lines)} lines, only {room} fit")
        lines = lines[:room]
    y = draw_text(draw, font, lines, 8, y, (245, 240, 228))
    draw_text(draw, font, search_lines, 8, y, (140, 200, 255))
    return image


def make_portrait(speaker, mood, font):
    image = Image.new("RGB", PORTRAIT_SIZE, COLORS.get(speaker, (70, 80, 100)))
    draw = ImageDraw.Draw(image)
    draw.fontmode = "1"
    lines = [speaker.upper(), mood or "normal"]
    y = (PORTRAIT_SIZE[1] - LINE_HEIGHT * len(lines)) // 2 - 4
    for line in lines:
        width = draw.textlength(line, font=font)
        draw.text(((PORTRAIT_SIZE[0] - width) / 2, y), line, font=font, fill=(255, 255, 255))
        y += LINE_HEIGHT
    return image


def main():
    argparse.ArgumentParser(description=__doc__).parse_args()
    font = ImageFont.truetype(FONT, FONT_SIZE)
    pictures, portraits = read_scenes()
    os.makedirs(os.path.join(STORY, "images"), exist_ok=True)
    os.makedirs(os.path.join(STORY, "portraits"), exist_ok=True)

    for scene, name, description, search in pictures:
        if has_drawing("pictures", name):
            continue
        make_picture(scene, name, description, search, font).save(os.path.join(STORY, "images", name))
        print(f"picture {name}")
    for name, (speaker, mood) in sorted(portraits.items()):
        # scripts/portraits.py makes the real portraits, a placeholder is only for a portrait that does not exist
        if has_drawing("portraits", name) or os.path.exists(os.path.join(STORY, "portraits", name)):
            continue
        make_portrait(speaker, mood, font).save(os.path.join(STORY, "portraits", name))
        print(f"portrait {name}")


if __name__ == "__main__":
    main()
