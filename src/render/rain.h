#ifndef RENDER_RAIN_H
#define RENDER_RAIN_H

#include "raylib.h"
#include "render/mode7.h"

#define RAIN_DROPS 1300

// A drop in the world, in map pixels: x and y on the map, z over the ground
typedef struct RainDrop
{
    Vector3 position;
} RainDrop;

// Falling rain in the road view
// The drops are points in the world near the camera, so they stay in place when the view turns, they come towards
// the camera when the car drives, and the near drops look larger than the far drops
// A drop is drawn as a short streak along its movement as seen from the car
typedef struct Rain
{
    RainDrop drops[RAIN_DROPS];
    unsigned int random;
} Rain;

// Put the drops in the space in front of the camera
void RainInit(Rain *rain, Mode7Camera camera);

// Move the drops, velocity is the velocity of the car in map pixels each second
void RainUpdate(Rain *rain, float dt, Mode7Camera camera, Vector2 velocity);

// Draw the streaks into the road view, in a color a little lighter than the fog
void RainDraw(const Rain *rain, RenderTexture2D target, Mode7Camera camera, Vector2 velocity, Color fog,
              float fog_end);

#endif
