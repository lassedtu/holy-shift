#ifndef RENDER_TREE_SHEET_H
#define RENDER_TREE_SHEET_H

#include <stdbool.h>
#include <stddef.h>

#include "raylib.h"

#define TREE_SHEET_MAX_ROWS 4

// The pictures of one tree seen from many directions and some camera angles
// The cell of a direction is in the column, the cell of a camera angle is in the row
typedef struct TreeSheet
{
    Texture2D texture;
    int cell_width;
    int cell_height;
    int columns;
    int rows;
    float azimuth_step_deg;
    float look_up_deg[TREE_SHEET_MAX_ROWS]; // angle of the camera above the tree, for each row
    float anchor_x[TREE_SHEET_MAX_ROWS];    // trunk base in the cell, for each row
    float anchor_y[TREE_SHEET_MAX_ROWS];
    float pixels_per_unit;
    float height_units; // height of the tree in the units of the picture
} TreeSheet;

// Load <base>.json and <base>.png, write the reason in message when it fails
bool TreeSheetLoad(TreeSheet *sheet, const char *base, char *message, size_t size);
void TreeSheetUnload(TreeSheet *sheet);

// Return the column of the sheet that shows the tree to a camera
// The direction is the angle in degrees from the tree to the camera, 0 is the south of the map
int TreeSheetColumn(const TreeSheet *sheet, float azimuth_deg);

// Return the row of the sheet with the camera angle that is nearest to an angle
int TreeSheetRow(const TreeSheet *sheet, float look_up_deg);

#endif
