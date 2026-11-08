#ifndef RENDER_COCKPIT_H
#define RENDER_COCKPIT_H

#include <stdbool.h>
#include <stddef.h>

#include "raylib.h"
#include "render/vomit.h"

#define COCKPIT_MAX_WHEEL_FRAMES 16

// The numbers of the cabin images, from assets/car_cockpit.json
typedef struct CockpitMetrics
{
    float px_per_deg;
    float yaw_min_deg; // yaw at the left edge of the panorama
    float lat_top_deg;    // elevation at the top of the panorama
    float lat_bottom_deg; // elevation at the bottom of the panorama
    int wheel_x;       // position of the wheel frames in panorama pixels
    int wheel_y;
    int wheel_frame_width;
    int wheel_frame_height;
    int wheel_frame_count;
    float wheel_angles_deg[COCKPIT_MAX_WHEEL_FRAMES];
} CockpitMetrics;

// The cabin of the car: a panorama of the inside and the frames of the steering wheel
typedef struct Cockpit
{
    Texture2D panorama;
    Texture2D wheel;
    Shader shader;
    CockpitMetrics metrics;
    RenderTexture2D glass; // what is on the windshield, at half the size of the panorama
    bool glass_used;       // false when the glass picture is empty
} Cockpit;

// Load the cabin images, write the reason in message when it fails
bool CockpitLoad(Cockpit *cockpit, char *message, size_t size);
void CockpitUnload(Cockpit *cockpit);

// Find the limits of the head pitch, in degrees, where the view stays in the panorama
void CockpitPitchLimits(const Cockpit *cockpit, float *pitch_min, float *pitch_max);

// Draw the splashes on the windshield into the glass picture, before BeginDrawing
void CockpitRenderGlass(Cockpit *cockpit, const VomitBlob *blobs, int count);

// Draw the cabin over an area of the window
// The head yaw and the head pitch are in degrees, the steer is from -1 (full left) to 1 (full right)
// The tint is the light in the cabin, from the time of day
void CockpitDraw(const Cockpit *cockpit, Rectangle destination, float head_yaw, float head_pitch,
                 float steer, Vector3 tint);

#endif
