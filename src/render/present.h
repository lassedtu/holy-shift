#ifndef RENDER_PRESENT_H
#define RENDER_PRESENT_H

#include "raylib.h"

// Return the area of the window for the game view
// The area is as large as possible and in the centre, the rest of the window stays black
// The scale is a whole number when the window is larger than the view, so pixels stay square
Rectangle PresentDestination(int window_width, int window_height);

#endif
