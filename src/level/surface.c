#include "level/surface.h"

#include <stdio.h>

#include "core/config.h"

bool SurfaceMapLoad(SurfaceMap *map, const Level *level, const Image *logic)
{
    *map = (SurfaceMap){0};
    map->logic = ImageCopy(*logic);
    map->road = level->road_color;
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
    bool road = pixel.r == map->road.r && pixel.g == map->road.g && pixel.b == map->road.b;
    return road ? SURFACE_ROAD : SURFACE_GRASS;
}
