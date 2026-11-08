#include "level/surface.h"

#include <stdio.h>

#include "core/config.h"

bool SurfaceMapLoad(SurfaceMap *map, const Level *level, const Image *logic)
{
    *map = (SurfaceMap){0};
    map->logic = ImageCopy(*logic);
    map->road = level->road_color;
    map->water = level->water_color;
    return IsImageValid(map->logic);
}

void SurfaceMapUnload(SurfaceMap *map)
{
    if (IsImageValid(map->logic))
    {
        UnloadImage(map->logic);
    }
    *map = (SurfaceMap){0};
}

Surface SurfaceAt(const SurfaceMap *map, Vector2 point)
{
    int x = (int)point.x;
    int y = (int)point.y;
    if (point.x < 0.0f || point.y < 0.0f || x >= map->logic.width || y >= map->logic.height)
    {
        return SURFACE_GRASS;
    }
    const Color *pixels = map->logic.data;
    Color pixel = pixels[y * map->logic.width + x];
    if (pixel.r == map->road.r && pixel.g == map->road.g && pixel.b == map->road.b)
    {
        return SURFACE_ROAD;
    }
    bool water = pixel.r == map->water.r && pixel.g == map->water.g && pixel.b == map->water.b;
    return water ? SURFACE_WATER : SURFACE_GRASS;
}
