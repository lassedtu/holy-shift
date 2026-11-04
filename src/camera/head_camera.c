#include "camera/head_camera.h"

#include "core/config.h"
#include "raymath.h"

void HeadCameraUpdate(HeadCamera *camera, float mouse_dx)
{
    camera->yaw =
        Clamp(camera->yaw + mouse_dx * MOUSE_SENSITIVITY_DEG, -HEAD_MAX_YAW_DEG, HEAD_MAX_YAW_DEG);
}
