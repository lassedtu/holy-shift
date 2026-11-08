#ifndef LEVEL_GROUND_H
#define LEVEL_GROUND_H

#include "level/forest.h"
#include "level/level.h"
#include "raylib.h"

// Paint the ground that the player sees from the logic map, in the theme of the level
// The road gets gravel, tyre tracks and loose stones at the edge, the ground gets patches, grass and leaves
// The forest puts fallen needles or leaves under the trees, it can be NULL
// The same level always gives the same picture
// Return an image of the size of the logic map, or an image with no data when there is not enough memory
Image GroundPaint(const Image *logic, const Level *level, const Forest *forest);

#endif
