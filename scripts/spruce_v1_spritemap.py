"""Create a pixel-art sprite sheet from the Spruce_V1 collection in Blender 5.x.

The script renders 48 camera directions at four upward camera angles. It saves
the frames in one PNG file and writes the layout data to a JSON file.

Run this command from the project root:

    /Applications/Blender.app/Contents/MacOS/Blender -b \
        assets_raw/ee_low_poly_trees/low_poly_trees.blend \
        --python scripts/spruce_v1_spritemap.py -- [--out assets] [--ppu 12]

The sprite sheet has 48 columns and four rows. Column 0 views the tree from
the -Y side. The camera moves counter-clockwise when viewed from above. Row 0
uses the smallest upward angle. The camera points at the centre of the tree's
bounding box. The JSON file gives the trunk base position in each cell.
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
from mathutils import Vector

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_OUTPUT_DIR = os.path.join(PROJECT_ROOT, "assets")

COLLECTION_NAME = "Spruce_V1"
AZIMUTH_STEPS = 48
LOOK_UP_DEG = [5.0, 15.0, 25.0, 35.0]  # One upward camera angle for each row.
PIXELS_PER_UNIT = 12.0                  # Scale from Blender units to pixels.
PADDING_PX = 1                          # Border width around each frame.
PALETTE_COLORS = 16                     # Size of the palette shared by all frames.
ALPHA_THRESHOLD = 0.5                   # Alpha cutoff for solid pixels.

SUN_ROTATION_DEG = (50.0, 0.0, -35.0)   # Use the same sun angle for each frame.
SUN_STRENGTH = 3.0
AMBIENT_COLOR = (0.55, 0.65, 0.85)
AMBIENT_STRENGTH = 1.0

OUTPUT_NAME = "spruce_v1_spritemap"


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out_dir, ppu = DEFAULT_OUTPUT_DIR, PIXELS_PER_UNIT
    i = 0
    while i < len(argv):
        if argv[i] == "--out":
            out_dir = argv[i + 1]
            i += 2
        elif argv[i] == "--ppu":
            ppu = float(argv[i + 1])
            i += 2
        else:
            raise SystemExit(f"Unknown argument: {argv[i]}")
    return out_dir, ppu


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


def camera_offset(azimuth_rad, look_up_rad):
    """Return the unit vector from the tree centre to the camera."""
    c = math.cos(look_up_rad)
    return Vector((math.sin(azimuth_rad) * c,
                   -math.cos(azimuth_rad) * c,
                   -math.sin(look_up_rad)))


def view_rotation(offset):
    return (-offset).to_track_quat('-Z', 'Y')


def enable_collection(name):
    """The tree collections are excluded in the view layer of the source file;
    include the collection (and its parents) so it is evaluated and rendered."""
    def find(lc):
        if lc.name == name:
            return [lc]
        for child in lc.children:
            path = find(child)
            if path:
                return [lc] + path
        return None

    path = find(bpy.context.view_layer.layer_collection)
    if path is None:
        raise SystemExit(f"Collection '{name}' not found in view layer")
    for lc in path:
        lc.exclude = False
        lc.hide_viewport = False
    bpy.context.view_layer.update()


def collect_world_vertices(objects):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    verts = []
    for ob in objects:
        if ob.type != 'MESH':
            continue
        ev = ob.evaluated_get(depsgraph)
        mesh = ev.to_mesh()
        mw = ev.matrix_world
        verts.extend(mw @ v.co for v in mesh.vertices)
        ev.to_mesh_clear()
    return verts


def make_materials_matte(objects):
    """Remove specular highlights so shading reads as clean flat colour."""
    for ob in objects:
        for slot in ob.material_slots:
            mat = slot.material
            if mat is None or mat.node_tree is None:
                print(f"WARNING: {ob.name} has a slot without a node material")
                continue
            for node in mat.node_tree.nodes:
                if node.bl_idname == 'ShaderNodeBsdfPrincipled':
                    node.inputs["Roughness"].default_value = 1.0
                    for key in ("Specular IOR Level", "Specular"):
                        if key in node.inputs:
                            node.inputs[key].default_value = 0.0


def setup_scene(tree_objects, res_w, res_h, ortho_scale, ppu_shift_y):
    scene = bpy.context.scene
    keep = set(tree_objects)

    for ob in scene.objects:
        ob.hide_render = ob not in keep

    # Select an Eevee engine supported by this Blender version.
    engines = {e.identifier for e in
               scene.render.bl_rna.properties['engine'].enum_items}
    scene.render.engine = ("BLENDER_EEVEE" if "BLENDER_EEVEE" in engines
                           else "BLENDER_EEVEE_NEXT")

    r = scene.render
    r.resolution_x, r.resolution_y = res_w, res_h
    r.resolution_percentage = 100
    r.film_transparent = True
    r.filter_size = 0.0          # Disable filtering to keep pixels sharp.
    r.image_settings.file_format = 'PNG'
    r.image_settings.color_mode = 'RGBA'
    r.image_settings.color_depth = '8'
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'None'
    scene.display_settings.display_device = 'sRGB'
    if hasattr(scene.eevee, "taa_render_samples"):
        scene.eevee.taa_render_samples = 1

    # Set the ambient light.
    world = scene.world or bpy.data.worlds.new("SpriteWorld")
    scene.world = world
    if hasattr(world, "use_nodes"):
        world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    if bg is None:
        bg = world.node_tree.nodes.new("ShaderNodeBackground")
    bg.inputs["Color"].default_value = (*AMBIENT_COLOR, 1.0)
    bg.inputs["Strength"].default_value = AMBIENT_STRENGTH

    # Add the sun light.
    sun_data = bpy.data.lights.new("SpriteSun", 'SUN')
    sun_data.energy = SUN_STRENGTH
    sun_data.use_shadow = False   # Shadow sampling noise breaks pixel art.
    sun = bpy.data.objects.new("SpriteSun", sun_data)
    sun.rotation_euler = [math.radians(a) for a in SUN_ROTATION_DEG]
    scene.collection.objects.link(sun)

    # Add the orthographic camera.
    cam_data = bpy.data.cameras.new("SpriteCam")
    cam_data.type = 'ORTHO'
    cam_data.sensor_fit = 'AUTO'
    cam_data.ortho_scale = ortho_scale
    cam_data.clip_start = 0.1
    cam_data.clip_end = 1000.0
    cam_data.shift_x = 0.0
    cam_data.shift_y = ppu_shift_y
    cam = bpy.data.objects.new("SpriteCam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    return scene, cam


def load_rgba(path):
    img = bpy.data.images.load(path)
    img.colorspace_settings.name = 'Non-Color'   # Read pixel values without color conversion.
    img.alpha_mode = 'STRAIGHT'
    w, h = img.size
    buf = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(buf)
    bpy.data.images.remove(img)
    return np.flipud(buf.reshape(h, w, 4))       # Convert Blender's bottom-up rows.


def build_palette(frames, k):
    """Deterministic weighted k-means over all solid pixels of all frames."""
    pix = np.concatenate([f[f[..., 3] >= ALPHA_THRESHOLD][:, :3]
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


def to_pixel_art(frame, palette):
    """Apply a hard alpha edge and snap colors to the palette (uint8 RGBA)."""
    solid = frame[..., 3] >= ALPHA_THRESHOLD
    d = ((frame[..., None, :3] - palette[None, None]) ** 2).sum(-1)
    rgb = palette[d.argmin(-1)]
    out = np.zeros(frame.shape, dtype=np.uint8)
    out[..., :3] = np.where(solid[..., None], rgb * 255 + 0.5, 0).astype(np.uint8)
    out[..., 3] = np.where(solid, 255, 0)
    return out


def main():
    out_dir, ppu = parse_args()
    if not os.path.isabs(out_dir):
        out_dir = os.path.join(PROJECT_ROOT, out_dir)
    out_dir = os.path.abspath(out_dir)
    os.makedirs(out_dir, exist_ok=True)

    coll = bpy.data.collections.get(COLLECTION_NAME)
    if coll is None:
        raise SystemExit(f"Collection '{COLLECTION_NAME}' not found")
    enable_collection(COLLECTION_NAME)
    tree_objects = [o for o in coll.all_objects if o.type == 'MESH']
    make_materials_matte(tree_objects)

    verts = collect_world_vertices(tree_objects)
    xs, ys, zs = zip(*verts)
    center = Vector(((min(xs) + max(xs)) / 2,
                     (min(ys) + max(ys)) / 2,
                     (min(zs) + max(zs)) / 2))
    base = Vector((center.x, center.y, min(zs)))   # Position of the trunk base.

    azimuths = [math.radians(i * 360.0 / AZIMUTH_STEPS)
                for i in range(AZIMUTH_STEPS)]
    pitches = [math.radians(a) for a in LOOK_UP_DEG]

    # Find the largest extent across all views to set one cell size.
    max_x = max_y = 0.0
    for el in pitches:
        for az in azimuths:
            inv = view_rotation(camera_offset(az, el)).to_matrix().transposed()
            for v in verts:
                p = inv @ (v - center)
                max_x = max(max_x, abs(p.x))
                max_y = max(max_y, abs(p.y))

    cell_w = int(math.ceil(2 * max_x * ppu)) + 2 * PADDING_PX
    cell_h = int(math.ceil(2 * max_y * ppu)) + 2 * PADDING_PX
    # Use even dimensions to keep the tree axis between pixel columns.
    cell_w += cell_w % 2
    cell_h += cell_h % 2
    ortho_scale = max(cell_w, cell_h) / ppu

    scene, cam = setup_scene(tree_objects, cell_w, cell_h, ortho_scale, 0.0)

    raw, anchors = [], []
    with tempfile.TemporaryDirectory() as tmp:
        for row, (el_deg, el) in enumerate(zip(LOOK_UP_DEG, pitches)):
            row_anchors = []
            for col, az in enumerate(azimuths):
                offset = camera_offset(az, el)
                rot = view_rotation(offset)
                cam.location = center + offset * 200.0
                cam.rotation_mode = 'QUATERNION'
                cam.rotation_quaternion = rot
                bpy.context.view_layer.update()

                path = os.path.join(tmp, f"f_{row}_{col}.png")
                scene.render.filepath = path
                bpy.ops.render.render(write_still=True)
                raw.append(load_rgba(path))

                # Store the trunk base position from the cell's top-left corner.
                p = rot.to_matrix().transposed() @ (base - center)
                row_anchors.append([round(cell_w / 2 + p.x * ppu, 2),
                                    round(cell_h / 2 - p.y * ppu, 2)])
            anchors.append(row_anchors)
            print(f"row {row + 1}/{len(LOOK_UP_DEG)} done (look-up {el_deg} deg)")

    palette = build_palette(raw, PALETTE_COLORS)
    rows = [np.concatenate([to_pixel_art(f, palette) for f in
                            raw[r * AZIMUTH_STEPS:(r + 1) * AZIMUTH_STEPS]],
                           axis=1)
            for r in range(len(LOOK_UP_DEG))]
    sheet = np.concatenate(rows, axis=0)
    png_path = os.path.join(out_dir, OUTPUT_NAME + ".png")
    write_png(png_path, sheet)

    meta = {
        "image": OUTPUT_NAME + ".png",
        "cell_width": cell_w,
        "cell_height": cell_h,
        "columns": AZIMUTH_STEPS,
        "rows": len(LOOK_UP_DEG),
        "azimuth_step_deg": 360.0 / AZIMUTH_STEPS,
        "look_up_deg": LOOK_UP_DEG,
        "pixels_per_unit": ppu,
        "palette": ["#%02x%02x%02x" % tuple(int(c * 255 + 0.5) for c in col)
                    for col in palette],
        "tree_height_units": round(max(zs) - min(zs), 3),
        # Each anchor gives the trunk base pixel as [x, y] within its cell.
        "anchors": anchors,
    }
    with open(os.path.join(out_dir, OUTPUT_NAME + ".json"), "w") as f:
        json.dump(meta, f, indent=2)

    print(f"Wrote {png_path} ({sheet.shape[1]}x{sheet.shape[0]}, "
          f"cell {cell_w}x{cell_h})")


main()
