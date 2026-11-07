#ifndef LEVEL_SURFACE_H
#define LEVEL_SURFACE_H

#include <stdbool.h>
#include <stddef.h>

#include "level/level.h"
#include "raylib.h"

// The type of ground under a point of the map
typedef enum Surface
{
    SURFACE_ROAD,
    SURFACE_GRASS
} Surface;

// The logic map of a level in memory, for the rules of the game
typedef struct SurfaceMap
{
    Image logic;
    Color road;
} SurfaceMap;

// Load the logic map of a level, write the reason in message when it fails
bool SurfaceMapLoad(SurfaceMap *map, const Level *level, char *message, size_t size);
void SurfaceMapUnload(SurfaceMap *map);

// Return the surface at a point of the map
// A point outside of the map is grass
Surface SurfaceAt(const SurfaceMap *map, Vector2 point);

#endif
