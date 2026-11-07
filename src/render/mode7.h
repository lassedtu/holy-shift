#ifndef RENDER_MODE7_H
#define RENDER_MODE7_H

#include <stdbool.h>
#include <stddef.h>

#include "raylib.h"

// The view of the road: the ground of the level seen from the camera, with a sky that is the color of the fog at the horizon
// The view is drawn at the game resolution and scaled to the window later
typedef struct Mode7
{
    Shader shader;
    RenderTexture2D target;
} Mode7;

// The camera of the road view, at the height of CAMERA_HEIGHT
typedef struct Mode7Camera
{
    Vector2 position; // map pixels
    float heading;    // radians, 0 is towards +x and a positive angle turns right
    float pitch;      // degrees, a positive angle is down
} Mode7Camera;

// Focal length in pixels
// Everything that is drawn on the road must use the same value
float Mode7Focal(void);

// Find the position in the view of a point near the camera
// side is to the right, height is up from the eye and depth is to the front, all in map pixels and level with the ground
// Return the distance of the point along the view direction
// When this distance is 0 or less, the point is behind the camera and the screen position is not valid
float Mode7Project(float pitch, float side, float height, float depth, Vector2 *screen);

// Load the shader and make the view, write the reason in message when it fails
bool Mode7Init(Mode7 *mode7, char *message, size_t size);
void Mode7Unload(Mode7 *mode7);

// Draw the ground texture as seen from the camera
void Mode7Render(Mode7 *mode7, Texture2D ground, Mode7Camera camera);

// Draw the last rendered view in an area of the window
void Mode7Draw(const Mode7 *mode7, Rectangle destination);

#endif
