#ifndef LEVEL_GROUND_H
#define LEVEL_GROUND_H

#include "level/forest.h"
#include "level/level.h"
#include "raylib.h"

// The materials in the alpha channel of the ground picture
#define GROUND_MATERIAL_WATER 0    // a lake: it reflects the sky
#define GROUND_MATERIAL_PUDDLE 64  // a puddle on the road: it reflects the sky in any weather
#define GROUND_MATERIAL_ROAD 192   // the road: it is wet and reflects a little when it rains
#define GROUND_MATERIAL_GROUND 255

// Paint the ground that the player sees from the logic map, in the theme of the level
// The road gets gravel, tyre tracks and loose stones at the edge, the ground gets patches, grass and leaves
// The forest puts fallen needles or leaves under the trees, it can be NULL
// The alpha channel of the picture is the material of each pixel, for the shader (see GROUND_MATERIAL_*)
// The same level always gives the same picture
// Return an image of the size of the logic map, or an image with no data when there is not enough memory
Image GroundPaint(const Image *logic, const Level *level, const Forest *forest);

#endif
