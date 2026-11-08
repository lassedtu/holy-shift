#include "render/rain.h"

#include <math.h>

#include "core/config.h"

#define FALL_SPEED 72.0f   // map pixels each second, about 9 meters each second
#define WIND_X 10.0f       // map pixels each second that the wind moves the drops on the map
#define WIND_Y 6.0f
#define STREAK_SECONDS 0.05f // a streak is the way that the drop moves in this time, as seen from the car
#define NEAR 3.0f          // map pixels in front of the camera, nearer drops go away
#define DRAW_NEAR 9.0f     // drops nearer than this are not drawn, they would be very long lines
#define FAR 90.0f          // the drops are put this far in front of the camera at most
#define TOP 40.0f          // the drops are put this high over the ground at most
#define SIDE 1.2f          // the space of the drops is this wide on each side, as a part of the distance to the front
#define RAIN_LIGHTEN 0.5f  // the streaks are this much nearer to white than the fog
#define RAIN_ALPHA 0.8f
#define MAX_STEP 0.05f     // seconds, a longer frame (for example while the level loads) moves the drops only this much

static float Random(Rain *rain)
{
    rain->random = rain->random * 1664525u + 1013904223u;
    return (float)((rain->random >> 8) & 0xFFFF) / 65535.0f;
}

// Put a drop in the space in front of the camera, at the top of the space or at any height
static void Spawn(Rain *rain, RainDrop *drop, Mode7Camera camera, bool any_height, float min_depth)
{
    Vector2 forward = {cosf(camera.heading), sinf(camera.heading)};
    Vector2 right = {-forward.y, forward.x};
    float depth = min_depth + Random(rain) * (FAR - min_depth);
    float side = (Random(rain) * 2.0f - 1.0f) * depth * SIDE;
    drop->position = (Vector3){camera.position.x + forward.x * depth + right.x * side,
                               camera.position.y + forward.y * depth + right.y * side,
                               any_height ? Random(rain) * TOP : TOP};
}

void RainInit(Rain *rain, Mode7Camera camera)
{
    *rain = (Rain){.random = 2463534242u};
    for (int i = 0; i < RAIN_DROPS; i++)
    {
        Spawn(rain, &rain->drops[i], camera, true, NEAR);
    }
}

// Return the distance to the front and to the right of a drop from the camera
static Vector2 FromCamera(Vector3 point, Mode7Camera camera)
{
    float dx = point.x - camera.position.x;
    float dy = point.y - camera.position.y;
    float c = cosf(camera.heading);
    float s = sinf(camera.heading);
    return (Vector2){dx * c + dy * s, -dx * s + dy * c};
}

void RainUpdate(Rain *rain, float dt, Mode7Camera camera, Vector2 velocity)
{
    (void)velocity;
    dt = fminf(dt, MAX_STEP);
    for (int i = 0; i < RAIN_DROPS; i++)
    {
        RainDrop *drop = &rain->drops[i];
        drop->position.x += WIND_X * dt;
        drop->position.y += WIND_Y * dt;
        drop->position.z -= FALL_SPEED * dt;
        Vector2 at = FromCamera(drop->position, camera);
        if (drop->position.z < 0.0f)
        {
            // The drop goes up by the height of the space and a random part more, so the drops never fall together
            float z = drop->position.z + TOP * (1.0f + 0.25f * Random(rain));
            Spawn(rain, drop, camera, false, NEAR);
            drop->position.z = z;
        }
        else if (at.x < NEAR || at.x > FAR || fabsf(at.y) > at.x * SIDE + 10.0f)
        {
            // A drop that left the space comes back at any height: far ahead when the car drives, else anywhere
            Spawn(rain, drop, camera, true, at.x < NEAR ? FAR * 0.6f : NEAR);
        }
    }
}

void RainDraw(const Rain *rain, RenderTexture2D target, Mode7Camera camera, Vector2 velocity, Color fog,
              float fog_end)
{
    Color light = ColorLerp(fog, WHITE, RAIN_LIGHTEN);
    // The movement of a drop as seen from the car: it falls, it moves with the wind, and the car drives into it
    Vector3 move = {(WIND_X - velocity.x) * STREAK_SECONDS, (WIND_Y - velocity.y) * STREAK_SECONDS,
                    -FALL_SPEED * STREAK_SECONDS};
    BeginTextureMode(target);
    for (int i = 0; i < RAIN_DROPS; i++)
    {
        Vector3 end = rain->drops[i].position;
        Vector3 start = {end.x - move.x, end.y - move.y, end.z - move.z};
        Vector2 end_at = FromCamera(end, camera);
        Vector2 start_at = FromCamera(start, camera);
        Vector2 a;
        Vector2 b;
        if (Mode7Project(camera.pitch, end_at.y, end.z - CAMERA_HEIGHT, end_at.x, &b) <= 1.0f ||
            Mode7Project(camera.pitch, start_at.y, start.z - CAMERA_HEIGHT, start_at.x, &a) <= 1.0f)
        {
            continue;
        }
        if (end_at.x < DRAW_NEAR)
        {
            continue;
        }
        float fade = 1.0f - fminf(end_at.x / fminf(FAR, fog_end), 1.0f);
        DrawLineV((Vector2){floorf(a.x), floorf(a.y)}, (Vector2){floorf(b.x), floorf(b.y)},
                  Fade(light, RAIN_ALPHA * fade));
    }
    EndTextureMode();
}
