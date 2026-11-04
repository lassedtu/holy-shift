"""Render the car cockpit as a pixel-art panorama with Blender 5.x and Cycles.

The script saves these files in the assets directory. This directory is created
if it does not exist.

* car_cockpit_panorama.png: cabin view without the steering wheel. The window
    pixels are partly transparent so the game world can show through.
* car_cockpit_wheel.png: steering wheel frames in one row. Draw the selected
    frame over the panorama with alpha blending.
* car_cockpit.json: panorama layout and steering wheel frame data.

Run this command from the project root:

    /Applications/Blender.app/Contents/MacOS/Blender -b \
        "assets_raw/Retro Car1.0/Car Asset.blend" \
        --python scripts/car_cockpit_panorama.py -- [--out assets] \
        [--scale 2] [--samples 64]

Use the panorama as an equirectangular strip. Column x is a yaw angle and
row y is an elevation angle:

        yaw_deg = yaw_min_deg + x / px_per_deg
        lat_deg = lat_top_deg - y / px_per_deg

Yaw 0 points forward and positive yaw points right. Elevation 0 is eye level.

Do not copy a flat window out of the strip. That stretches the picture
towards the edges. For every screen pixel, build the view ray of a normal
(rectilinear) camera with horizontal FOV view_hfov_deg, tilt it down by
view_pitch_deg, turn it by the head yaw, convert the ray to yaw and elevation,
and sample the strip there. demo/cockpit_demo.c does this in a shader.
Limit head turn to max_head_yaw_deg. The strip is wider than that range by
half the view FOV plus a margin.

The wheel frames use panorama pixels. Draw the frame closest to the steering
input at (wheel.x, wheel.y) in panorama space BEFORE the re-projection, so it
is projected together with the cabin. Use alpha blending.
"""
import json
import math
import os
import struct
import sys
import tempfile
import zlib

import bpy
import numpy as np
from mathutils import Matrix, Vector

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_OUTPUT_DIR = os.path.join(PROJECT_ROOT, "assets")

GAME_VIEW_W, GAME_VIEW_H = 640, 360     # Game resolution before scaling.
VIEW_HFOV_DEG = 90.0                    # Horizontal field of view.
MAX_HEAD_YAW_DEG = 120.0                # Maximum head turn in either direction.
DEFAULT_SCALE = 2.0                     # Panorama pixels per game pixel.
VIEW_PITCH_DEG = 14.0                   # Recommended downward tilt of the game view.
STRIP_LAT_TOP_DEG = 25.0                # Top of the strip (eye level is 0).
STRIP_LAT_BOTTOM_DEG = -70.0            # Bottom of the strip.
STRIP_YAW_MARGIN_DEG = 10.0             # Extra yaw range for the corners of a tilted view.

# Set the camera position relative to the wheel centre.
# The car faces -Y. The driver sits on +X.
HEAD_OFFSET_FROM_WHEEL = Vector((0.0, 0.45, 0.42))

WHEEL_ANGLES_DEG = [-90, -70, -45, -22, 0, 22, 45, 70, 90]  # Positive is right.
WHEEL_PATCH_MARGIN_PX = 3

PALETTE_COLORS = 32                     # Colors shared by all output images.
ALPHA_SOLID = 0.6                       # Alpha cutoff for solid pixels.
ALPHA_GLASS_MIN = 0.05                  # Alpha cutoff for glass pixels.
GLASS_RENDER_ALPHA = 0.25               # Glass alpha during rendering.
GLASS_COLOR = (0.35, 0.5, 0.6)          # Glass tint in linear color values.
GLASS_OUTPUT_ALPHA = 80                 # Glass alpha in the output image.

SUN_ROTATION_DEG = (50.0, 0.0, -35.0)
SUN_STRENGTH = 3.0
AMBIENT_COLOR = (0.55, 0.65, 0.85)
AMBIENT_STRENGTH = 1.0

TEXTURE_BLUE = os.path.join("Blue Texture", "Blue Car Texture.png")
TEXTURE_EMISSIVE = "Emissive Texture.png"
WINDOW_MATERIAL = "carwindows"
WHEEL_OBJECT = "Steering_wheel"

OUT_PANORAMA = "car_cockpit_panorama.png"
OUT_WHEEL = "car_cockpit_wheel.png"
OUT_JSON = "car_cockpit.json"


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    opts = {"out": DEFAULT_OUTPUT_DIR, "scale": DEFAULT_SCALE, "samples": 64}
    i = 0
    while i < len(argv):
        key = argv[i].lstrip("-")
        if key not in opts:
            raise SystemExit(f"Unknown argument: {argv[i]}")
        opts[key] = type(opts[key])(argv[i + 1])
        i += 2
    return opts


def write_png(path, rgba):
    """Write an (H, W, 4) uint8 array as a PNG. The first row is at the top."""
    h, w, _ = rgba.shape
    raw = b"".join(b"\x00" + rgba[y].tobytes() for y in range(h))

    def chunk(tag, data):
        body = tag + data
        return (struct.pack(">I", len(data)) + body
                + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def load_rgba(path):
    img = bpy.data.images.load(path)
    img.colorspace_settings.name = 'Non-Color'   # Read pixel values without color conversion.
    img.alpha_mode = 'STRAIGHT'
    w, h = img.size
    buf = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(buf)
    bpy.data.images.remove(img)
    return np.flipud(buf.reshape(h, w, 4))       # Convert Blender's bottom-up rows.


def relink_textures(root):
    """Set the material image nodes to the available car textures."""
    mat = bpy.data.materials["Black_Car_Mat"]
    for node in mat.node_tree.nodes:
        if node.bl_idname != 'ShaderNodeTexImage':
            continue
        old = (node.image.filepath if node.image else "").lower()
        rel = TEXTURE_EMISSIVE if "emissive" in old else TEXTURE_BLUE
        path = os.path.join(root, rel)
        if not os.path.isfile(path):
            raise SystemExit(f"Texture not found: {path}")
        node.image = bpy.data.images.load(path, check_existing=True)
        node.image.reload()


def make_glass_material():
    """Create a tinted, partly transparent glass material."""
    mat = bpy.data.materials.new("SpriteGlass")
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*GLASS_COLOR, 1.0)
    bsdf.inputs["Alpha"].default_value = GLASS_RENDER_ALPHA
    bsdf.inputs["Roughness"].default_value = 1.0
    for key in ("Specular IOR Level", "Specular"):
        if key in bsdf.inputs:
            bsdf.inputs[key].default_value = 0.0
    return mat


def replace_window_material(objects):
    glass = make_glass_material()
    count = 0
    for ob in objects:
        for slot in ob.material_slots:
            if slot.material and slot.material.name == WINDOW_MATERIAL:
                slot.material = glass
                count += 1
    if not count:
        print(f"WARNING: no '{WINDOW_MATERIAL}' slots found")


def make_materials_matte(objects):
    for ob in objects:
        for slot in ob.material_slots:
            mat = slot.material
            if mat is None or mat.node_tree is None:
                continue
            for node in mat.node_tree.nodes:
                if node.bl_idname == 'ShaderNodeBsdfPrincipled':
                    node.inputs["Roughness"].default_value = 1.0
                    for key in ("Specular IOR Level", "Specular"):
                        if key in node.inputs:
                            node.inputs[key].default_value = 0.0


def mesh_world_vertices(ob, extra=None):
    """Return evaluated mesh vertices in world space."""
    dg = bpy.context.evaluated_depsgraph_get()
    ev = ob.evaluated_get(dg)
    mesh = ev.to_mesh()
    mw = ev.matrix_world if extra is None else extra
    verts = [mw @ v.co for v in mesh.vertices]
    ev.to_mesh_clear()
    return verts


# The car faces -Y. Its right side is -X, and up is +Z.
class Pano:
    def __init__(self, scale):
        f = (GAME_VIEW_W / 2) / math.tan(math.radians(VIEW_HFOV_DEG) / 2)
        self.px_per_deg = f * math.pi / 180.0 * scale
        half_span = MAX_HEAD_YAW_DEG + VIEW_HFOV_DEG / 2 + STRIP_YAW_MARGIN_DEG
        if half_span > 180:
            raise SystemExit("Head yaw + FOV exceeds 360 degrees")
        self.yaw_min = -half_span
        self.yaw_max = half_span
        self.width = int(round(2 * half_span * self.px_per_deg))
        self.lat_top = STRIP_LAT_TOP_DEG
        self.lat_bottom = STRIP_LAT_BOTTOM_DEG
        self.height = int(round((self.lat_top - self.lat_bottom)
                                * self.px_per_deg))
        self.scale = scale

    def project(self, eye, p):
        """Project a world point to panorama pixel coordinates."""
        d = p - eye
        a, b, c = -d.y, -d.x, d.z      # Forward, right, and up components.
        yaw = math.degrees(math.atan2(b, a))
        lat = math.degrees(math.atan2(c, math.hypot(a, b)))
        x = (yaw - self.yaw_min) * self.px_per_deg
        y = (self.lat_top - lat) * self.px_per_deg
        return x, y


def setup_scene(pano, eye, samples):
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    cy = scene.cycles
    cy.device = 'CPU'
    cy.samples = samples
    cy.use_adaptive_sampling = False
    cy.use_denoising = True
    cy.filter_width = 0.01          # Minimize pixel filtering.
    cy.max_bounces = 4

    r = scene.render
    r.resolution_x, r.resolution_y = pano.width, pano.height
    r.resolution_percentage = 100
    r.film_transparent = True
    r.use_border = False
    r.image_settings.file_format = 'PNG'
    r.image_settings.color_mode = 'RGBA'
    r.image_settings.color_depth = '8'
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'None'
    scene.display_settings.display_device = 'sRGB'

    world = scene.world or bpy.data.worlds.new("CockpitWorld")
    scene.world = world
    if hasattr(world, "use_nodes"):
        world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    if bg is None:
        bg = world.node_tree.nodes.new("ShaderNodeBackground")
    bg.inputs["Color"].default_value = (*AMBIENT_COLOR, 1.0)
    bg.inputs["Strength"].default_value = AMBIENT_STRENGTH

    sun_data = bpy.data.lights.new("CockpitSun", 'SUN')
    sun_data.energy = SUN_STRENGTH
    sun = bpy.data.objects.new("CockpitSun", sun_data)
    sun.rotation_euler = [math.radians(a) for a in SUN_ROTATION_DEG]
    scene.collection.objects.link(sun)

    cam_data = bpy.data.cameras.new("CockpitCam")
    cam_data.type = 'PANO'
    cam_data.panorama_type = 'EQUIRECTANGULAR'
    cam_data.longitude_min = math.radians(pano.yaw_min)
    cam_data.longitude_max = math.radians(pano.yaw_max)
    cam_data.latitude_min = math.radians(pano.lat_bottom)
    cam_data.latitude_max = math.radians(pano.lat_top)
    cam_data.clip_start = 0.01
    cam_data.clip_end = 1000.0
    cam = bpy.data.objects.new("CockpitCam", cam_data)
    cam.location = eye
    cam.rotation_euler = (math.pi / 2, 0.0, math.pi)   # Point the camera along -Y.
    scene.collection.objects.link(cam)
    scene.camera = cam
    return scene


def render_frame(scene, tmp, name, border=None):
    """Render an image and return float RGBA data with shape (H, W, 4).

    If border is set to (x0, y0, x1, y1), render only that top-down pixel
    region. Keep the rest of the image transparent.
    """
    r = scene.render
    if border is None:
        r.use_border = False
    else:
        x0, y0, x1, y1 = border
        w, h = r.resolution_x, r.resolution_y
        r.use_border = True
        r.use_crop_to_border = False
        r.border_min_x, r.border_max_x = x0 / w, x1 / w
        r.border_min_y, r.border_max_y = 1 - y1 / h, 1 - y0 / h
    path = os.path.join(tmp, name)
    r.filepath = path
    bpy.ops.render.render(write_still=True)
    return load_rgba(path)


def classify(frame):
    """Return (opaque mask, glass mask)."""
    a = frame[..., 3]
    opaque = a >= ALPHA_SOLID
    glass = (a >= ALPHA_GLASS_MIN) & ~opaque
    return opaque, glass


def build_palette(frames, k):
    """Build a weighted k-means palette from all visible pixels."""
    pix = np.concatenate([f[classify(f)[0] | classify(f)[1]][:, :3]
                          for f in frames])
    q = np.round(np.clip(pix, 0, 1) * 63).astype(np.int32)
    keys, counts = np.unique(q, axis=0, return_counts=True)
    pts = keys / 63.0
    w = counts.astype(np.float64)
    order = np.argsort(pts @ np.array([0.299, 0.587, 0.114]))
    cum = np.cumsum(w[order]) / w.sum()
    init = [order[min(np.searchsorted(cum, (i + 0.5) / k), len(order) - 1)]
            for i in range(k)]
    centers = pts[init].copy()
    for _ in range(40):
        lab = ((pts[:, None, :] - centers[None]) ** 2).sum(-1).argmin(1)
        for c in range(k):
            m = lab == c
            if m.any():
                centers[c] = (pts[m] * w[m, None]).sum(0) / w[m].sum()
    return centers


def palette_lookup(palette):
    """Map each 64-level quantized color to a palette index."""
    g = np.arange(64) / 63.0
    grid = np.stack(np.meshgrid(g, g, g, indexing='ij'), -1).reshape(-1, 3)
    d = ((grid[:, None, :] - palette[None]) ** 2).sum(-1)
    return d.argmin(1).astype(np.uint8)


def mean_palette_color(frame, palette, lut, mask):
    """Return the palette color (uint8 RGB) nearest to the mean color in mask."""
    if not mask.any():
        return np.zeros(3, dtype=np.uint8)
    q = np.round(np.clip(frame[mask][:, :3].mean(0), 0, 1) * 63).astype(int)
    idx = lut[q[0] * 4096 + q[1] * 64 + q[2]]
    return (palette[idx] * 255 + 0.5).astype(np.uint8)


def to_pixel_art(frame, palette, lut, empty_rgb=(0, 0, 0)):
    """Apply hard alpha edges and palette colors. Keep three alpha levels.

    Fully transparent pixels get empty_rgb instead of black, so bilinear
    sampling in the game does not create dark fringes."""
    opaque, glass = classify(frame)
    q = np.round(np.clip(frame[..., :3], 0, 1) * 63).astype(np.int32)
    idx = lut[q[..., 0] * 4096 + q[..., 1] * 64 + q[..., 2]]
    rgb = (palette[idx] * 255 + 0.5).astype(np.uint8)
    visible = opaque | glass
    out = np.zeros(frame.shape, dtype=np.uint8)
    out[..., :3] = np.where(visible[..., None], rgb,
                            np.asarray(empty_rgb, dtype=np.uint8))
    out[..., 3] = np.where(opaque, 255, np.where(glass, GLASS_OUTPUT_ALPHA, 0))
    return out


def main():
    opts = parse_args()
    out_dir = opts["out"]
    if not os.path.isabs(out_dir):
        out_dir = os.path.join(PROJECT_ROOT, out_dir)
    out_dir = os.path.abspath(out_dir)
    os.makedirs(out_dir, exist_ok=True)

    root = os.path.dirname(bpy.data.filepath)
    relink_textures(root)

    bpy.context.view_layer.update()
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    wheel = bpy.data.objects.get(WHEEL_OBJECT)
    if wheel is None:
        raise SystemExit(f"Object '{WHEEL_OBJECT}' not found")
    replace_window_material(meshes)
    make_materials_matte(meshes)
    for ob in bpy.context.scene.objects:
        if ob.type == 'LIGHT':
            ob.hide_render = True

    # Find the wheel centre and its local Z axis in world space.
    wheel_verts = mesh_world_vertices(wheel)
    wheel_center = sum(wheel_verts, Vector()) / len(wheel_verts)
    eye = wheel_center + HEAD_OFFSET_FROM_WHEEL
    axis_world = (wheel.matrix_world.to_3x3() @ Vector((0, 0, 1))).normalized()
    # A positive steering angle turns the wheel clockwise from the driver's view.
    # Blender's positive rotation appears counter-clockwise from this view.
    toward_driver = axis_world.dot(eye - wheel_center) > 0
    handed = -1.0 if toward_driver else 1.0

    pano = Pano(opts["scale"])
    scene = setup_scene(pano, eye, opts["samples"])
    print(f"Panorama {pano.width}x{pano.height}, "
          f"{pano.px_per_deg:.3f} px/deg, eye {tuple(round(c, 3) for c in eye)}")

    # Rotate the wheel around its local centre.
    local_center = sum((v.co for v in wheel.data.vertices), Vector()) \
        / len(wheel.data.vertices)
    base_matrix = wheel.matrix_basis.copy()

    def set_wheel_angle(deg):
        rot = Matrix.Rotation(math.radians(deg) * handed, 4, 'Z')
        pivot = (Matrix.Translation(local_center) @ rot
                 @ Matrix.Translation(-local_center))
        wheel.matrix_basis = base_matrix @ pivot
        bpy.context.view_layer.update()

    # Find the rectangle that contains the wheel at every angle.
    xs, ys = [], []
    for deg in WHEEL_ANGLES_DEG:
        set_wheel_angle(deg)
        for v in mesh_world_vertices(wheel):
            x, y = pano.project(eye, v)
            xs.append(x)
            ys.append(y)
    m = WHEEL_PATCH_MARGIN_PX
    px0, py0 = int(math.floor(min(xs))) - m, int(math.floor(min(ys))) - m
    px1, py1 = int(math.ceil(max(xs))) + m, int(math.ceil(max(ys))) + m
    px0, py0 = max(px0, 0), max(py0, 0)
    px1, py1 = min(px1, pano.width), min(py1, pano.height)
    if min(xs) < 0 or min(ys) < 0 or max(xs) > pano.width or max(ys) > pano.height:
        print("WARNING: the steering wheel is clipped by the panorama; "
              "adjust STRIP_LAT_BOTTOM_DEG or HEAD_OFFSET_FROM_WHEEL")
    pw, ph = px1 - px0, py1 - py0

    with tempfile.TemporaryDirectory() as tmp:
        # Render the cabin without the wheel.
        set_wheel_angle(0)
        wheel.hide_render = True
        base = render_frame(scene, tmp, "base.png")
        print("base done")

        # Use other meshes as holdouts so the cabin hides covered wheel pixels.
        wheel.hide_render = False
        for ob in meshes:
            ob.is_holdout = ob is not wheel
        patches = []
        for deg in WHEEL_ANGLES_DEG:
            set_wheel_angle(deg)
            full = render_frame(scene, tmp, f"wheel_{deg}.png",
                                border=(px0, py0, px1, py1))
            patches.append(full[py0:py1, px0:px1])
            print(f"wheel {deg} done")

    palette = build_palette([base] + patches, PALETTE_COLORS)
    lut = palette_lookup(palette)
    mid = patches[len(patches) // 2]
    glass_rgb = mean_palette_color(base, palette, lut, classify(base)[1])
    wheel_rgb = mean_palette_color(mid, palette, lut, classify(mid)[0])
    write_png(os.path.join(out_dir, OUT_PANORAMA),
              to_pixel_art(base, palette, lut, glass_rgb))
    atlas = np.concatenate([to_pixel_art(p, palette, lut, wheel_rgb)
                            for p in patches], axis=1)
    write_png(os.path.join(out_dir, OUT_WHEEL), atlas)

    meta = {
        "panorama": OUT_PANORAMA,
        "panorama_width": pano.width,
        "panorama_height": pano.height,
        "scale": pano.scale,
        "px_per_deg": pano.px_per_deg,
        "yaw_min_deg": pano.yaw_min,
        "yaw_max_deg": pano.yaw_max,
        "max_head_yaw_deg": MAX_HEAD_YAW_DEG,
        "lat_top_deg": pano.lat_top,
        "lat_bottom_deg": pano.lat_bottom,
        "view_pitch_deg": VIEW_PITCH_DEG,
        "game_view": [GAME_VIEW_W, GAME_VIEW_H],
        "view_hfov_deg": VIEW_HFOV_DEG,
        "glass_alpha": GLASS_OUTPUT_ALPHA,
        "palette": ["#%02x%02x%02x" % tuple(int(c * 255 + 0.5) for c in col)
                    for col in palette],
        "wheel": {
            "image": OUT_WHEEL,
            "frame_width": pw,
            "frame_height": ph,
            # Top-left corner of the wheel frames in panorama pixels.
            "x": px0,
            "y": py0,
            # Frame i starts at x = i * frame_width.
            "angles_deg": WHEEL_ANGLES_DEG,
        },
    }
    with open(os.path.join(out_dir, OUT_JSON), "w") as f:
        json.dump(meta, f, indent=2)
    print(f"Wrote {out_dir} ({pano.width}x{pano.height})")


main()
