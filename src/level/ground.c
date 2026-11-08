#include "level/ground.h"

#include <math.h>
#include <stdlib.h>

#define LITTER_COLORS 4

// The colors and the amounts of one theme
typedef struct GroundStyle
{
    Color road[3]; // dark, normal and light gravel
    Color track;   // the tyre tracks along the road
    Color pebble[2];
    Color edge; // loose gravel at the edge of the road
    Color verge[2];
    Color ground[4]; // from light to dark
    Color patch;     // soil or moss
    Color leaves[LITTER_COLORS]; // fallen leaves or needles
    Color tuft[2]; // light top and dark base of a tuft of grass
    Color puddle[2];
    float verge_width;    // map pixels from the road edge
    float track_offset;   // map pixels from the middle of the road to the middle of a track
    float track_width;    // map pixels
    float pebbles;        // part of the road pixels that have a stone
    float litter;         // leaves or needles on each pixel of the ground
    float tree_litter;    // leaves or needles under each tree
    float tufts;          // tufts of grass on each pixel of the ground
    float patches;        // from 0 to 1, how much of the ground is soil or moss
    float puddles;        // from 0 to 1, how much of the road is water
} GroundStyle;

static const GroundStyle STYLES[GROUND_THEME_COUNT] = {
    [GROUND_FOREST] =
        {
            .road = {{104, 86, 66, 255}, {122, 102, 78, 255}, {140, 120, 94, 255}},
            .track = {92, 76, 60, 255},
            .pebble = {{78, 66, 54, 255}, {168, 152, 126, 255}},
            .edge = {132, 114, 88, 255},
            .verge = {{104, 86, 58, 255}, {88, 72, 50, 255}},
            .ground = {{84, 132, 60, 255}, {66, 116, 50, 255}, {52, 98, 44, 255}, {42, 82, 40, 255}},
            .patch = {78, 92, 44, 255},
            .leaves = {{112, 82, 52, 255}, {96, 70, 46, 255}, {132, 100, 62, 255}, {74, 60, 40, 255}},
            .tuft = {{110, 160, 72, 255}, {46, 90, 40, 255}},
            .puddle = {{70, 84, 90, 255}, {120, 136, 140, 255}},
            .verge_width = 5.0f,
            .track_offset = 11.0f,
            .track_width = 5.0f,
            .pebbles = 0.035f,
            .litter = 0.004f,
            .tree_litter = 30.0f,
            .tufts = 0.004f,
            .patches = 0.35f,
            .puddles = 0.0f,
        },
    [GROUND_AUTUMN] =
        {
            .road = {{118, 98, 74, 255}, {136, 116, 88, 255}, {156, 136, 106, 255}},
            .track = {106, 88, 68, 255},
            .pebble = {{88, 74, 60, 255}, {182, 166, 138, 255}},
            .edge = {146, 128, 98, 255},
            .verge = {{116, 92, 60, 255}, {98, 78, 52, 255}},
            .ground = {{132, 134, 64, 255}, {112, 116, 54, 255}, {94, 100, 48, 255}, {78, 84, 42, 255}},
            .patch = {110, 82, 50, 255},
            .leaves = {{206, 112, 40, 255}, {172, 62, 38, 255}, {222, 172, 62, 255}, {142, 88, 48, 255}},
            .tuft = {{150, 150, 76, 255}, {84, 90, 44, 255}},
            .puddle = {{80, 92, 96, 255}, {130, 144, 148, 255}},
            .verge_width = 5.0f,
            .track_offset = 11.0f,
            .track_width = 5.0f,
            .pebbles = 0.03f,
            .litter = 0.05f,
            .tree_litter = 60.0f,
            .tufts = 0.002f,
            .patches = 0.3f,
            .puddles = 0.0f,
        },
    [GROUND_MUD] =
        {
            .road = {{66, 52, 42, 255}, {82, 64, 50, 255}, {98, 78, 60, 255}},
            .track = {54, 42, 34, 255},
            .pebble = {{48, 38, 32, 255}, {120, 104, 84, 255}},
            .edge = {92, 72, 54, 255},
            .verge = {{76, 60, 44, 255}, {62, 50, 38, 255}},
            .ground = {{70, 100, 48, 255}, {58, 86, 42, 255}, {50, 72, 38, 255}, {60, 52, 40, 255}},
            .patch = {72, 58, 42, 255},
            .leaves = {{86, 66, 44, 255}, {72, 56, 40, 255}, {100, 80, 52, 255}, {60, 48, 36, 255}},
            .tuft = {{88, 120, 58, 255}, {42, 62, 34, 255}},
            .puddle = {{56, 62, 64, 255}, {110, 122, 126, 255}},
            .verge_width = 7.0f,
            .track_offset = 11.0f,
            .track_width = 8.0f,
            .pebbles = 0.01f,
            .litter = 0.003f,
            .tree_litter = 20.0f,
            .tufts = 0.003f,
            .patches = 0.55f,
            .puddles = 0.6f,
        },
    [GROUND_LAKESIDE] =
        {
            .road = {{116, 100, 80, 255}, {134, 118, 94, 255}, {152, 136, 110, 255}},
            .track = {104, 90, 72, 255},
            .pebble = {{86, 76, 64, 255}, {178, 166, 140, 255}},
            .edge = {150, 134, 106, 255},
            .verge = {{178, 162, 122, 255}, {158, 142, 106, 255}},
            .ground = {{98, 146, 70, 255}, {80, 130, 58, 255}, {64, 112, 50, 255}, {52, 96, 46, 255}},
            .patch = {150, 138, 100, 255},
            .leaves = {{120, 96, 62, 255}, {104, 84, 56, 255}, {140, 116, 76, 255}, {86, 72, 50, 255}},
            .tuft = {{124, 172, 84, 255}, {54, 100, 46, 255}},
            .puddle = {{80, 100, 110, 255}, {130, 150, 160, 255}},
            .verge_width = 9.0f,
            .track_offset = 11.0f,
            .track_width = 5.0f,
            .pebbles = 0.04f,
            .litter = 0.002f,
            .tree_litter = 20.0f,
            .tufts = 0.006f,
            .patches = 0.2f,
            .puddles = 0.0f,
        },
};

static unsigned int HashInt(int x, int y, unsigned int seed)
{
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Return a number from 0 to 1 for each pixel
static float Hash(int x, int y, unsigned int seed)
{
    return (float)(HashInt(x, y, seed) & 0xFFFFFF) / 16777216.0f;
}

// Smooth random values from 0 to 1, with one random value on each corner of a grid of the given size
static float Noise(float x, float y, float size, unsigned int seed)
{
    x /= size;
    y /= size;
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);
    float fx = x - (float)ix;
    float fy = y - (float)iy;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float a = Hash(ix, iy, seed);
    float b = Hash(ix + 1, iy, seed);
    float c = Hash(ix, iy + 1, seed);
    float d = Hash(ix + 1, iy + 1, seed);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

// Distance in pixels from each pixel to the nearest pixel that is not inside, 0 for the pixels that are not inside
// Two passes over the image with the steps to the 8 neighbours
static void Distance(const unsigned char *inside, float *distance, int width, int height)
{
    const float far = 1e9f;
    const float diagonal = 1.4142135f;
    for (int i = 0; i < width * height; i++)
    {
        distance[i] = inside[i] ? far : 0.0f;
    }
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float *d = &distance[y * width + x];
            if (*d == 0.0f)
            {
                continue;
            }
            if (x > 0)
                *d = fminf(*d, d[-1] + 1.0f);
            if (y > 0)
            {
                *d = fminf(*d, d[-width] + 1.0f);
                if (x > 0)
                    *d = fminf(*d, d[-width - 1] + diagonal);
                if (x < width - 1)
                    *d = fminf(*d, d[-width + 1] + diagonal);
            }
        }
    }
    for (int y = height - 1; y >= 0; y--)
    {
        for (int x = width - 1; x >= 0; x--)
        {
            float *d = &distance[y * width + x];
            if (*d == 0.0f)
            {
                continue;
            }
            if (x < width - 1)
                *d = fminf(*d, d[1] + 1.0f);
            if (y < height - 1)
            {
                *d = fminf(*d, d[width] + 1.0f);
                if (x < width - 1)
                    *d = fminf(*d, d[width + 1] + diagonal);
                if (x > 0)
                    *d = fminf(*d, d[width - 1] + diagonal);
            }
        }
    }
}

static unsigned int NameSeed(const char *name)
{
    unsigned int h = 2166136261u;
    for (const char *c = name; *c != '\0'; c++)
    {
        h = (h ^ (unsigned char)*c) * 16777619u;
    }
    return h;
}

// A random number from 0 to 1 for each pixel, so the border between two colors is a mix of random dots
static float Speckle(int x, int y)
{
    return Hash(x, y, 0x5bd1e995u);
}

// Choose one of the colors from dark to light with a value from 0 to 1
static Color Pick(const Color *colors, int count, float value, int x, int y)
{
    int index = (int)((value + (Speckle(x, y) - 0.5f) * 0.14f) * (float)count);
    index = index < 0 ? 0 : (index >= count ? count - 1 : index);
    return colors[index];
}

static Color RoadPixel(const GroundStyle *style, int x, int y, float inside, float half, unsigned int seed)
{
    float wobble = Noise((float)x, (float)y, 6.0f, seed + 1) - 0.5f;
    if (inside < 2.5f + 2.0f * wobble)
    {
        float h = Hash(x, y, seed + 2);
        return h < 0.25f ? style->pebble[0] : (h < 0.45f ? style->pebble[1] : style->edge);
    }

    float from_middle = half - inside;
    float track = fabsf(from_middle - style->track_offset + 2.0f * wobble);
    bool on_track = track < style->track_width * 0.5f + (Speckle(x, y) - 0.5f) * 1.5f;

    // Puddles in the low parts of the road, most of them in the ruts, with a wet rim
    if (style->puddles > 0.0f && inside > 4.0f)
    {
        float rut = fmaxf(0.0f, 1.0f - track / (style->track_width * 1.5f));
        float water = Noise((float)x, (float)y, 26.0f, seed + 3) * 0.6f + Noise((float)x, (float)y, 8.0f, seed + 4) * 0.25f +
                      0.25f * rut;
        float level = 1.0f - 0.4f * style->puddles;
        if (water > level + 0.03f)
        {
            return Hash(x, y, seed + 18) < 0.03f ? style->puddle[1] : style->puddle[0];
        }
        if (water > level)
        {
            return style->track;
        }
    }

    if (Hash(x, y, seed + 5) < style->pebbles * (on_track ? 0.2f : 1.0f))
    {
        return style->pebble[Hash(x, y, seed + 6) < 0.6f ? 0 : 1];
    }
    if (on_track)
    {
        return style->track;
    }
    float value = Noise((float)x, (float)y, 40.0f, seed + 7) * 0.55f + Noise((float)x, (float)y, 10.0f, seed + 8) * 0.3f +
                  Noise((float)x, (float)y, 3.0f, seed + 9) * 0.15f;
    return Pick(style->road, 3, value, x, y);
}

static Color GroundPixel(const GroundStyle *style, int x, int y, float outside, unsigned int seed)
{
    float wobble = Noise((float)x, (float)y, 5.0f, seed + 10) - 0.5f;
    float verge = style->verge_width + 5.0f * wobble;
    float into = (outside - verge) / 3.0f;
    if (into < Speckle(x, y))
    {
        float value = Noise((float)x, (float)y, 8.0f, seed + 11) * 0.7f + Hash(x, y, seed + 12) * 0.3f;
        return style->verge[value < 0.55f ? 0 : 1];
    }

    float patch = Noise((float)x, (float)y, 48.0f, seed + 13) * 0.7f + Noise((float)x, (float)y, 9.0f, seed + 14) * 0.3f;
    if (patch + (Speckle(x, y) - 0.5f) * 0.06f > 1.0f - 0.35f * style->patches)
    {
        return style->patch;
    }
    float value = Noise((float)x, (float)y, 80.0f, seed + 15) * 0.5f + Noise((float)x, (float)y, 20.0f, seed + 16) * 0.3f +
                  Noise((float)x, (float)y, 4.0f, seed + 17) * 0.2f;
    return Pick(style->ground, 4, value, x, y);
}

static void Put(Color *pixels, const unsigned char *road, int width, int height, int x, int y, Color color)
{
    if (x >= 0 && y >= 0 && x < width && y < height && !road[y * width + x])
    {
        pixels[y * width + x] = color;
    }
}

// A leaf or a needle is one or two pixels, a tuft of grass is a light pixel over a dark pixel
static void Scatter(Color *pixels, const unsigned char *road, const float *outside, int width, int height,
                    const GroundStyle *style, unsigned int seed)
{
    float area = (float)width * (float)height;
    int litter = (int)(area * style->litter);
    for (int i = 0; i < litter; i++)
    {
        int x = (int)(HashInt(i, 1, seed) % (unsigned int)width);
        int y = (int)(HashInt(i, 2, seed) % (unsigned int)height);
        // Fewer leaves stay near the road, the cars blow them away
        if (outside[y * width + x] < style->verge_width && Hash(i, 3, seed) < 0.6f)
        {
            continue;
        }
        Color color = style->leaves[HashInt(i, 4, seed) % LITTER_COLORS];
        Put(pixels, road, width, height, x, y, color);
        unsigned int shape = HashInt(i, 5, seed) % 3;
        if (shape == 1)
            Put(pixels, road, width, height, x + 1, y, color);
        else if (shape == 2)
            Put(pixels, road, width, height, x, y + 1, color);
    }

    int tufts = (int)(area * style->tufts);
    for (int i = 0; i < tufts; i++)
    {
        int x = (int)(HashInt(i, 6, seed) % (unsigned int)width);
        int y = (int)(HashInt(i, 7, seed) % (unsigned int)height);
        if (outside[y * width + x] < style->verge_width + 2.0f)
        {
            continue;
        }
        Put(pixels, road, width, height, x, y, style->tuft[0]);
        Put(pixels, road, width, height, x, y + 1, style->tuft[1]);
        if (HashInt(i, 8, seed) % 2)
            Put(pixels, road, width, height, x + 1, y + 1, style->tuft[0]);
    }
}

// Needles or leaves fall around each trunk, more near the trunk
static void TreeLitter(Color *pixels, const unsigned char *road, int width, int height, const GroundStyle *style,
                       const Forest *forest, unsigned int seed)
{
    for (int t = 0; t < forest->count; t++)
    {
        const Tree *tree = &forest->trees[t];
        if (tree->position.x < 0.0f || tree->position.y < 0.0f || tree->position.x >= (float)width ||
            tree->position.y >= (float)height)
        {
            continue;
        }
        float radius = 9.0f * tree->scale;
        int count = (int)(style->tree_litter * tree->scale);
        for (int i = 0; i < count; i++)
        {
            float angle = Hash(t, i * 3, seed) * 6.2831853f;
            float r = radius * sqrtf(Hash(t, i * 3 + 1, seed)) * Hash(t, i * 3 + 2, seed + 1);
            int x = (int)(tree->position.x + cosf(angle) * r);
            int y = (int)(tree->position.y + sinf(angle) * r);
            Put(pixels, road, width, height, x, y, style->leaves[HashInt(t, i, seed + 2) % LITTER_COLORS]);
        }
    }
}

Image GroundPaint(const Image *logic, const Level *level, const Forest *forest)
{
    Image result = {0};
    int width = logic->width;
    int height = logic->height;
    int count = width * height;
    Color *source = LoadImageColors(*logic);
    unsigned char *road = malloc((size_t)count);
    unsigned char *grass = malloc((size_t)count);
    float *inside = malloc((size_t)count * sizeof(float));
    float *outside = malloc((size_t)count * sizeof(float));
    Color *pixels = malloc((size_t)count * sizeof(Color));
    if (source == NULL || road == NULL || grass == NULL || inside == NULL || outside == NULL || pixels == NULL)
    {
        goto done;
    }

    Color key = level->road_color;
    for (int i = 0; i < count; i++)
    {
        Color c = source[i];
        road[i] = abs(c.r - key.r) + abs(c.g - key.g) + abs(c.b - key.b) < 12;
        grass[i] = !road[i];
    }
    Distance(road, inside, width, height);
    Distance(grass, outside, width, height);

    const GroundStyle *style = &STYLES[level->ground_theme];
    unsigned int seed = NameSeed(level->name);
    float half = (float)level->road_width * 0.5f;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int i = y * width + x;
            pixels[i] = road[i] ? RoadPixel(style, x, y, inside[i], half, seed)
                                : GroundPixel(style, x, y, outside[i], seed);
        }
    }
    Scatter(pixels, road, outside, width, height, style, seed);
    if (forest != NULL)
    {
        TreeLitter(pixels, road, width, height, style, forest, seed);
    }

    result = (Image){.data = pixels, .width = width, .height = height, .mipmaps = 1,
                     .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    pixels = NULL;

done:
    UnloadImageColors(source);
    free(road);
    free(grass);
    free(inside);
    free(outside);
    free(pixels);
    return result;
}
