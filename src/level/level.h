#ifndef LEVEL_LEVEL_H
#define LEVEL_LEVEL_H

#include <stdbool.h>
#include <stddef.h>

#include "core/difficulty.h"
#include "level/ground_theme.h"
#include "level/tree_species.h"
#include "raylib.h"

#define LEVEL_MAX_POINTS 512
#define LEVEL_MAX_NAME 64
#define LEVEL_MAX_ORDERED 64 // most levels that LevelFindFirst looks at
#define LEVEL_RIVALS 7 // the other drivers of a stage, see game/standings.h

// The data of a level file (levels/<name>/level.json)
// The points are the control points of the road curve, in map pixels
typedef struct Level
{
    char name[LEVEL_MAX_NAME];         // the name of the folder
    char display_name[LEVEL_MAX_NAME]; // the name that the player sees, the folder name when the file has none
    Vector2 points[LEVEL_MAX_POINTS];
    int count;
    bool closed;               // the last point connects to the first point
    bool tutorial;             // the game shows the hints of the tutorial on this level
    float times[DIFFICULTY_COUNT][LEVEL_RIVALS]; // seconds of the other drivers on each difficulty, in the order of
                                                 // game/standings.c
    int time_count[DIFFICULTY_COUNT];            // 0 when the level file has no times for a difficulty
    bool jelka_just_ahead;     // Jelka's time is always 1 second less than the time of the player
    int map_width;
    int map_height;
    int road_width;
    Color road_color;               // color of the road in the logic map
    Color outside_color;            // color of the ground next to the road in the logic map
    bool trees[TREE_SPECIES_COUNT]; // the species of trees around the road
    GroundTheme ground_theme;       // the look of the ground, GROUND_FOREST when the file has no theme
} Level;

// Load a level by its folder name, write the reason in message when it fails
bool LevelLoad(Level *level, const char *name, char *message, size_t size);

// Change the road width of a loaded level for a difficulty
void LevelApplyDifficulty(Level *level, Difficulty difficulty);

// Write the names of the variants of the selected tree species, for example "spruce_v1", return the count
int LevelTreeVariants(const Level *level, char names[TREE_MAX_VARIANTS][TREE_VARIANT_NAME_MAX]);

// Write the path of a file in the folder of the level
void LevelFilePath(const Level *level, const char *file, char *path, size_t size);

// Write the name of the first saved level (the first in LevelFindAll), return false if there is none
bool LevelFindFirst(char *name, size_t size);

// Write the display name of a saved level, or its folder name when it has none
void LevelReadDisplayName(const char *name, char *display_name, int size);

// Write the names of the saved levels, at most max names, return the count
// The levels in LEVEL_ORDER_PATH come first in the order of the file, the other levels follow in alphabetical order
int LevelFindAll(char names[][LEVEL_MAX_NAME], int max);

// Write the names of all saved levels, separated by commas
void LevelListNames(char *list, size_t size);

#endif
