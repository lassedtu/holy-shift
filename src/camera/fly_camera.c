#include "camera/fly_camera.h"

#include <math.h>

#include "core/config.h"
#include "raymath.h"

void FlyCameraUpdate(FlyCamera *camera, float forward, float right, float mouse_dx, float dt,
                     float map_width, float map_height)
{
    camera->heading += mouse_dx * MOUSE_SENSITIVITY_DEG * DEG2RAD;

    // The right side of the heading, when the map y axis points down
    Vector2 front = {cosf(camera->heading), sinf(camera->heading)};
    Vector2 side = {-front.y, front.x};
    Vector2 move = Vector2Add(Vector2Scale(front, forward), Vector2Scale(side, right));

    // A diagonal move must not be faster than a straight move
    if (Vector2Length(move) > 1.0f)
    {
        move = Vector2Normalize(move);
    }

    camera->position = Vector2Add(camera->position, Vector2Scale(move, FLY_SPEED * dt));
    camera->position.x = Clamp(camera->position.x, 0.0f, map_width);
    camera->position.y = Clamp(camera->position.y, 0.0f, map_height);
}
