#ifndef RENDER_VOMIT_H
#define RENDER_VOMIT_H

#include <stdbool.h>

#include "raylib.h"

// The windshield, in degrees: x is the yaw (positive to the right), y is how far below eye level (positive down)
#define VOMIT_GLASS_LEFT -55.0f   // a splash that slides past these lines has left the windshield
#define VOMIT_GLASS_RIGHT 60.0f
#define VOMIT_GLASS_BOTTOM 18.0f

// A splash of vomit on the windshield, in degrees (see above)
typedef struct VomitBlob
{
    Vector2 position;
    Vector2 velocity;  // degrees each second, a loose splash slides off the glass
    float radius;
    float stick;       // from 1 (stuck) to 0 (loose)
    unsigned int seed; // the shape of the splash and its streaks
    bool alive;
} VomitBlob;

// A splash with a larger radius than this, in degrees, is a big splat with streaks, a smaller one is a drop
#define VOMIT_BIG 4.0f

// Draw a splash at a place in pixels with a radius in pixels, the seed of the blob gives its shape
// A big splat has a lumpy edge, arms with drops, and streaks that run down with clear glass between them
void VomitDraw(Vector2 center, float radius, unsigned int seed, bool big, float alpha);

#endif
