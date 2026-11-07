#ifndef LEVEL_LEVEL_H
#define LEVEL_LEVEL_H

#include <stdbool.h>
#include <stddef.h>

#include "level/tree_species.h"
#include "raylib.h"

#define LEVEL_MAX_POINTS 512
#define LEVEL_MAX_NAME 64

// The data of a level file (levels/<name>/level.json)
// The points are the control points of the road curve, in map pixels
typedef struct Level
{
    char name[LEVEL_MAX_NAME];
    Vector2 points[LEVEL_MAX_POINTS];
    int count;
    bool closed; // the last point connects to the first point
    int map_width;
    int map_height;
    int road_width;
    Color road_color; // color of the road in the logic map
    bool trees[TREE_SPECIES_COUNT]; // the species of trees around the road
} Level;

// Load a level by its folder name, write the reason in message when it fails
bool LevelLoad(Level *level, const char *name, char *message, size_t size);

// Write the names of the variants of the selected tree species, for example "spruce_v1", return the count
int LevelTreeVariants(const Level *level, char names[TREE_MAX_VARIANTS][TREE_VARIANT_NAME_MAX]);

// Write the path of a file in the folder of the level
void LevelFilePath(const Level *level, const char *file, char *path, size_t size);

// Write the name of the first saved level (in alphabetical order), return false if there is none
bool LevelFindFirst(char *name, size_t size);

// Write the names of all saved levels, separated by commas
void LevelListNames(char *list, size_t size);

#endif
