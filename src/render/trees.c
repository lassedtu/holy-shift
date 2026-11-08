#include "render/trees.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/config.h"
#include "render/mode7.h"
#include "render/shader_util.h"

// Added to the name of a variant to get the name of the files of each level of detail
static const char *const LOD_SUFFIXES[TREE_LOD_COUNT] = {"", "_lod1", "_lod2"};

// A picture is used when one of its pixels covers this many pixels of the view or more
// The pictures of the next levels of detail have pixels that are twice as large
#define LOD_SWITCH_PIXELS 0.75f

#define SHADOW_TEXTURE_SIZE 64

static float Smoothstep(float from, float to, float value)
{
    float t = fminf(fmaxf((value - from) / (to - from), 0.0f), 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// The red channel of the tint is the amount of fog for the shader, the green channel the light of the headlights
static Color FogTint(float amount, float beam)
{
    return (Color){(unsigned char)(amount * 255.0f + 0.5f), (unsigned char)(beam * 255.0f + 0.5f), 0, 255};
}

// The light of one headlight at an offset from the headlight, the same as Headlight in mode7.fs
static float Headlight(float dx, float dy, float angle)
{
    float along = dx * cosf(angle) + dy * sinf(angle);
    if (along <= 0.0f)
    {
        return 0.0f;
    }
    float across = fabsf(-dx * sinf(angle) + dy * cosf(angle));
    float cone = 1.0f - Smoothstep(0.0f, 1.0f, across / (along * tanf(HEADLIGHT_HALF_ANGLE_DEG * DEG2RAD)));
    return cone * (1.0f - Smoothstep(HEADLIGHT_NEAR, HEADLIGHT_FAR, along));
}

// Return the light of the two headlights at a point, from 0 to 1, in the same bands as on the ground
static float HeadlightBeam(Vector2 point, Mode7Camera camera, float strength)
{
    if (strength <= 0.0f)
    {
        return 0.0f;
    }
    float heading = camera.car_heading;
    float toe = HEADLIGHT_TOE_DEG * DEG2RAD;
    Vector2 right = {-sinf(heading), cosf(heading)};
    Vector2 forward = {cosf(heading), sinf(heading)};
    float dx = point.x - (camera.position.x + forward.x * HEADLIGHT_FORWARD + right.x * HEADLIGHT_RIGHT);
    float dy = point.y - (camera.position.y + forward.y * HEADLIGHT_FORWARD + right.y * HEADLIGHT_RIGHT);
    float light = fmaxf(Headlight(dx + right.x * HEADLIGHT_SPACING, dy + right.y * HEADLIGHT_SPACING, heading - toe),
                        Headlight(dx - right.x * HEADLIGHT_SPACING, dy - right.y * HEADLIGHT_SPACING, heading + toe));
    return floorf(light * strength * HEADLIGHT_BANDS + 0.5f) / HEADLIGHT_BANDS;
}

bool TreesInit(Trees *trees, const Level *level, char *message, size_t size)
{
    *trees = (Trees){0};
    if (!ShaderLoadFile(&trees->shader, SHADER_TREES_PATH, message, size))
    {
        return false;
    }
    if (!ShaderLoadFile(&trees->shadow_shader, SHADER_SHADOW_PATH, message, size))
    {
        TreesUnload(trees);
        return false;
    }

    char names[TREE_MAX_VARIANTS][TREE_VARIANT_NAME_MAX];
    int variant_count = LevelTreeVariants(level, names);
    for (int variant = 0; variant < variant_count; variant++)
    {
        // The variant counts as loaded at once, so that TreesUnload frees the sheets that are loaded
        trees->variant_count = variant + 1;
        for (int lod = 0; lod < TREE_LOD_COUNT; lod++)
        {
            char base[160];
            snprintf(base, sizeof(base), "%s/%s%s", TREE_SHEET_DIR, names[variant], LOD_SUFFIXES[lod]);
            if (!TreeSheetLoad(&trees->variants[variant].sheets[lod], base, message, size))
            {
                TreesUnload(trees);
                return false;
            }
        }
    }
    Image circle = GenImageGradientRadial(SHADOW_TEXTURE_SIZE, SHADOW_TEXTURE_SIZE, TREE_SHADOW_SOLID,
                                          BLACK, BLANK);
    trees->shadow = LoadTextureFromImage(circle);
    UnloadImage(circle);
    SetTextureFilter(trees->shadow, TEXTURE_FILTER_BILINEAR);
    trees->visible = malloc(TREE_MAX_VISIBLE * sizeof(VisibleTree));
    if (trees->visible == NULL)
    {
        snprintf(message, size, "Not enough memory for the trees");
        TreesUnload(trees);
        return false;
    }
    return true;
}

void TreesUnload(Trees *trees)
{
    for (int variant = 0; variant < trees->variant_count; variant++)
    {
        for (int lod = 0; lod < TREE_LOD_COUNT; lod++)
        {
            TreeSheetUnload(&trees->variants[variant].sheets[lod]);
        }
    }
    if (IsShaderValid(trees->shadow_shader))
    {
        UnloadShader(trees->shadow_shader);
    }
    if (IsShaderValid(trees->shader))
    {
        UnloadShader(trees->shader);
    }
    if (IsTextureValid(trees->shadow))
    {
        UnloadTexture(trees->shadow);
    }
    free(trees->visible);
    *trees = (Trees){0};
}

// Add the trees of the cells in front of the camera to the list, return the count
static int CollectTrees(Trees *trees, const Forest *forest, Vector2 position, Vector2 forward,
                        Vector2 right, float pitch, float far)
{
    // The cells that touch the box around the visible area
    float half_width = tanf(VIEW_HFOV_DEG * 0.5f * DEG2RAD);
    Vector2 far_left = {position.x + (forward.x - right.x * half_width) * far,
                        position.y + (forward.y - right.y * half_width) * far};
    Vector2 far_right = {position.x + (forward.x + right.x * half_width) * far,
                         position.y + (forward.y + right.y * half_width) * far};
    float min_x = fminf(position.x, fminf(far_left.x, far_right.x)) - TREE_VIEW_MARGIN;
    float max_x = fmaxf(position.x, fmaxf(far_left.x, far_right.x)) + TREE_VIEW_MARGIN;
    float min_y = fminf(position.y, fminf(far_left.y, far_right.y)) - TREE_VIEW_MARGIN;
    float max_y = fmaxf(position.y, fmaxf(far_left.y, far_right.y)) + TREE_VIEW_MARGIN;

    int first_column = (int)floorf((min_x - forest->origin.x) / FOREST_CELL_SIZE);
    int last_column = (int)floorf((max_x - forest->origin.x) / FOREST_CELL_SIZE);
    int first_row = (int)floorf((min_y - forest->origin.y) / FOREST_CELL_SIZE);
    int last_row = (int)floorf((max_y - forest->origin.y) / FOREST_CELL_SIZE);

    int count = 0;
    for (int row = first_row; row <= last_row; row++)
    {
        for (int column = first_column; column <= last_column; column++)
        {
            int in_cell;
            const Tree *cell = ForestCell(forest, column, row, &in_cell);
            for (int i = 0; i < in_cell && count < TREE_MAX_VISIBLE; i++)
            {
                float dx = cell[i].position.x - position.x;
                float dy = cell[i].position.y - position.y;
                float depth = dx * forward.x + dy * forward.y;
                float side = dx * right.x + dy * right.y;
                const TreeSheet *model = &trees->variants[cell[i].variant].sheets[0];
                float crown = 0.5f * (float)model->cell_width * cell[i].scale * TREE_MAP_PIXELS_PER_UNIT /
                              model->pixels_per_unit;
                Vector2 base;
                float front = Mode7Project(pitch, side, -CAMERA_HEIGHT, depth, &base);
                if (depth < TREE_NEAR || depth > far || front <= 0.0f ||
                    fabsf(side) > front * half_width + crown)
                {
                    continue;
                }
                trees->visible[count++] = (VisibleTree){depth, side, &cell[i]};
            }
        }
    }
    return count;
}

// Farthest first, so that the near trees cover the far trees
static int CompareDepth(const void *a, const void *b)
{
    float depth_a = ((const VisibleTree *)a)->depth;
    float depth_b = ((const VisibleTree *)b)->depth;
    return (depth_a < depth_b) - (depth_a > depth_b);
}

// Draw dots on the ground under a tree, so that the flat bottom of the trunk does not float on the ground
static void DrawShadow(const Trees *trees, const VisibleTree *visible, float focal, float pitch,
                       const Lighting *light)
{
    const Tree *tree = visible->tree;
    const TreeSheet *model = &trees->variants[tree->variant].sheets[0];
    float height = tree->scale * model->height_units * TREE_MAP_PIXELS_PER_UNIT;
    float radius = height * TREE_SHADOW_RADIUS;

    // A circle on the ground is as wide as it is far away, and it is flat by the height of the camera
    float depth = visible->depth;
    Vector2 center;
    float front = Mode7Project(pitch, visible->side, -CAMERA_HEIGHT, depth, &center);
    int x = (int)roundf(center.x);
    int y = (int)roundf(center.y);
    int radius_x = (int)ceilf(radius * focal / front);
    int radius_y = (int)ceilf(radius * CAMERA_HEIGHT * focal / (front * front));
    float fog = Smoothstep(light->fog_start, light->fog_end, depth);
    unsigned char alpha = (unsigned char)(255.0f * (1.0f - fog));
    if (radius_y < 1)
    {
        radius_y = 1;
    }
    Rectangle source = {0, 0, SHADOW_TEXTURE_SIZE, SHADOW_TEXTURE_SIZE};
    Rectangle destination = {(float)(x - radius_x), (float)(y - radius_y), (float)(2 * radius_x),
                             (float)(2 * radius_y)};
    DrawTexturePro(trees->shadow, source, destination, (Vector2){0, 0}, 0.0f, (Color){255, 255, 255, alpha});
}

static void DrawTree(const Trees *trees, const VisibleTree *visible, Mode7Camera camera, float focal,
                     const Lighting *light)
{
    Vector2 position = camera.position;
    float pitch = camera.pitch;
    const Tree *tree = visible->tree;
    float depth = visible->depth;
    Vector2 base;
    float front = Mode7Project(pitch, visible->side, -CAMERA_HEIGHT, depth, &base);

    // Screen pixels for each pixel of the most detailed picture
    const TreeVariant *variant = &trees->variants[tree->variant];
    const TreeSheet *best = &variant->sheets[0];
    float map_per_pixel = tree->scale * TREE_MAP_PIXELS_PER_UNIT / best->pixels_per_unit;
    float pixel = map_per_pixel * focal / front;
    int lod = 0;
    while (lod < TREE_LOD_COUNT - 1 && pixel < LOD_SWITCH_PIXELS * powf(0.5f, (float)lod))
    {
        lod++;
    }
    const TreeSheet *sheet = &variant->sheets[lod];
    float k = pixel * best->pixels_per_unit / sheet->pixels_per_unit;

    // The direction and the angle at which the camera sees the middle of the tree
    float to_x = position.x - tree->position.x;
    float to_y = position.y - tree->position.y;
    float azimuth = atan2f(to_x, to_y) * RAD2DEG;
    if (azimuth < 0.0f)
    {
        azimuth += 360.0f;
    }
    float ground = sqrtf(to_x * to_x + to_y * to_y);
    float look_up = atan2f(tree->scale * best->height_units * TREE_MAP_PIXELS_PER_UNIT * TREE_SIGHT_HEIGHT - CAMERA_HEIGHT, ground) * RAD2DEG;
    look_up = fminf(look_up, TREE_MAX_LOOK_UP_DEG);
    int column = TreeSheetColumn(sheet, azimuth);
    int row = TreeSheetRow(sheet, look_up);

    float left = roundf(base.x - sheet->anchor_x[row] * k);
    float top = roundf(base.y - sheet->anchor_y[row] * k);
    Rectangle destination = {left, top, fmaxf(1.0f, roundf((float)sheet->cell_width * k)),
                             fmaxf(1.0f, roundf((float)sheet->cell_height * k))};
    Rectangle source = {(float)(column * sheet->cell_width), (float)(row * sheet->cell_height),
                        (float)sheet->cell_width, (float)sheet->cell_height};

    float fog = Smoothstep(light->fog_start, light->fog_end, depth);
    float beam = HeadlightBeam(tree->position, camera, light->headlights);
    DrawTexturePro(sheet->texture, source, destination, (Vector2){0, 0}, 0.0f, FogTint(fog, beam));
}

void TreesRender(Trees *trees, const Forest *forest, RenderTexture2D target, Mode7Camera camera,
                 const Lighting *light)
{
    float focal = Mode7Focal();
    Vector2 position = camera.position;
    Vector2 forward = {cosf(camera.heading), sinf(camera.heading)};
    Vector2 right = {-forward.y, forward.x};

    int count = CollectTrees(trees, forest, position, forward, right, camera.pitch, fminf(FOG_END, light->fog_end));
    qsort(trees->visible, (size_t)count, sizeof(VisibleTree), CompareDepth);

    BeginTextureMode(target);
    if (light->shadow > 0.0f)
    {
        ShaderSetFloat(trees->shadow_shader, "darkness", TREE_SHADOW_ALPHA * light->shadow);
        BeginShaderMode(trees->shadow_shader);
        for (int i = 0; i < count; i++)
        {
            DrawShadow(trees, &trees->visible[i], focal, camera.pitch, light);
        }
        EndShaderMode();
    }
    ShaderSetColor(trees->shader, "fogColor", light->fog);
    SetShaderValue(trees->shader, GetShaderLocation(trees->shader, "tint"), &light->tint, SHADER_UNIFORM_VEC3);
    BeginShaderMode(trees->shader);
    for (int i = 0; i < count; i++)
    {
        DrawTree(trees, &trees->visible[i], camera, focal, light);
    }
    EndShaderMode();
    EndTextureMode();
}
