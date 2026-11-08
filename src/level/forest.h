#ifndef LEVEL_FOREST_H
#define LEVEL_FOREST_H

#include <stdbool.h>

#include "level/level.h"
#include "level/track.h"
#include "raylib.h"

typedef struct Tree
{
    int variant;      // index in the list of LevelTreeVariants
    Vector2 position; // the trunk, in map pixels
    float scale;      // 1 is the size of the model of the tree
} Tree;

// The trees of a level: they fill all the ground around the road, and some distance past the map
// The trees are kept in a grid of square cells, so that the trees near the camera are found fast
typedef struct Forest
{
    Tree *trees; // sorted by cell
    int count;
    int *cell_start; // index of the first tree of each cell, one more value at the end
    int columns;     // cells in a row of the grid
    int rows;
    Vector2 origin; // map position of the corner of the first cell, the grid starts outside the map
} Forest;

// Place the trees on both sides of the track, not on the water of the logic map (RGBA, it can be NULL)
// The same level always gives the same forest
bool ForestBuild(Forest *forest, const Level *level, const Track *track, const Image *logic);
void ForestUnload(Forest *forest);

// Return the trees of a cell, or NULL and a count of 0 when the cell is outside of the grid
const Tree *ForestCell(const Forest *forest, int column, int row, int *count);

// Return true when a box touches the trunk of a tree
// The box has its centre at center, its length along heading (radians) and its width across it
// The radius of a trunk is TREE_TRUNK_RADIUS times the scale of the tree
bool ForestHit(const Forest *forest, Vector2 center, float heading, float length, float width);

#endif
