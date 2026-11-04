#ifndef RENDER_MODE7_H
#define RENDER_MODE7_H

#include <stdbool.h>
#include <stddef.h>

#include "raylib.h"

// The view of the road: the ground of the level seen from the camera, with a blue sky
// The view is drawn at the game resolution and scaled to the window later
typedef struct Mode7
{
    Shader shader;
    RenderTexture2D target;
} Mode7;

// Load the shader and make the view, write the reason in message when it fails
bool Mode7Init(Mode7 *mode7, char *message, size_t size);
void Mode7Unload(Mode7 *mode7);

// Draw the ground texture as seen from a position and a direction (radians) in the map
void Mode7Render(Mode7 *mode7, Texture2D ground, Vector2 position, float heading);

// Draw the last rendered view in an area of the window
void Mode7Draw(const Mode7 *mode7, Rectangle destination);

#endif
