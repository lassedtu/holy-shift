#include "camera/head_camera.h"

#include "core/config.h"
#include "raymath.h"

void HeadCameraInit(HeadCamera *camera, float yaw, float pitch_min, float pitch_max)
{
    camera->yaw = Clamp(yaw, -HEAD_MAX_YAW_DEG, HEAD_MAX_YAW_DEG);
    camera->pitch_min = pitch_min;
    camera->pitch_max = pitch_max;
    camera->pitch = Clamp(VIEW_PITCH_DEG, pitch_min, pitch_max);
}

void HeadCameraUpdate(HeadCamera *camera, float mouse_dx, float mouse_dy)
{
    camera->yaw =
        Clamp(camera->yaw + mouse_dx * MOUSE_SENSITIVITY_DEG, -HEAD_MAX_YAW_DEG, HEAD_MAX_YAW_DEG);
    camera->pitch = Clamp(camera->pitch + mouse_dy * MOUSE_SENSITIVITY_DEG, camera->pitch_min,
                          camera->pitch_max);
}
