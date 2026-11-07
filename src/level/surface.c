#include "level/surface.h"

#include <stdio.h>

#include "core/config.h"

bool SurfaceMapLoad(SurfaceMap *map, const Level *level, char *message, size_t size)
{
    *map = (SurfaceMap){0};
    char path[256];
    LevelFilePath(level, LEVEL_LOGIC_FILE, path, sizeof(path));
    map->logic = LoadImage(path);
    if (!IsImageValid(map->logic))
    {
        snprintf(message, size, "Could not load %s", path);
        return false;
    }
    ImageFormat(&map->logic, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    map->road = level->road_color;
    return true;
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
