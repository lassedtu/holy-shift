#ifndef CAMERA_FLY_CAMERA_H
#define CAMERA_FLY_CAMERA_H

#include "raylib.h"

// A free camera that moves over the map
// The camera stays at the height of CAMERA_HEIGHT, it cannot move up or down
typedef struct FlyCamera
{
    Vector2 position; // map pixels
    float heading;    // radians, 0 is towards +x and a positive angle turns right
} FlyCamera;

// Move and turn the camera
// forward and right are from -1 to 1 (for example -1 for the S key)
// mouse_dx is the mouse movement in pixels since the last frame
// The camera stays inside of the map
void FlyCameraUpdate(FlyCamera *camera, float forward, float right, float mouse_dx, float dt,
                     float map_width, float map_height);

#endif
