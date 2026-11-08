#include "level/logic_map.h"

#include <stdio.h>
#include <stdlib.h>

#include "core/config.h"

static bool SameColor(Color a, Color b)
{
    return abs(a.r - b.r) + abs(a.g - b.g) + abs(a.b - b.b) < 12;
}

static void Repaint(Image *image, const Level *level, const Track *track)
{
    Color *pixels = image->data;
    int count = image->width * image->height;
    for (int i = 0; i < count; i++)
    {
        if (SameColor(pixels[i], level->road_color))
        {
            pixels[i] = level->outside_color;
        }
    }
    int radius = level->road_width / 2;
    for (int i = 0; i < track->count; i++)
    {
        ImageDrawCircleV(image, track->samples[i], radius, level->road_color);
    }
}

bool LogicMapLoad(Image *image, const Level *level, const Track *track, bool repaint, char *message, size_t size)
{
    char path[256];
    LevelFilePath(level, LEVEL_LOGIC_FILE, path, sizeof(path));
    *image = LoadImage(path);
    if (!IsImageValid(*image))
    {
        snprintf(message, size, "Could not load %s", path);
        return false;
    }
    if (image->width != level->map_width || image->height != level->map_height)
    {
        snprintf(message, size, "%s has size %dx%d, the level file says %dx%d", path, image->width, image->height,
                 level->map_width, level->map_height);
        UnloadImage(*image);
        *image = (Image){0};
        return false;
    }
    ImageFormat(image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (repaint)
    {
        Repaint(image, level, track);
    }
    return true;
}
