#ifndef CAMERA_HEAD_CAMERA_H
#define CAMERA_HEAD_CAMERA_H

// The direction in which the driver looks, relative to the car
typedef struct HeadCamera
{
    float yaw; // degrees, 0 is forward and a positive angle is to the right
} HeadCamera;

// Turn the head with the mouse movement (in pixels) since the last frame
// The head cannot turn more than HEAD_MAX_YAW_DEG to the left or to the right
void HeadCameraUpdate(HeadCamera *camera, float mouse_dx);

#endif
