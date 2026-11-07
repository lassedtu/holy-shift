#ifndef CAMERA_HEAD_CAMERA_H
#define CAMERA_HEAD_CAMERA_H

// The direction in which the driver looks, relative to the car
typedef struct HeadCamera
{
    float yaw;       // degrees, 0 is forward and a positive angle is to the right
    float pitch;     // degrees, a positive angle is down
    float pitch_min; // limits of the pitch, in degrees
    float pitch_max;
} HeadCamera;

// Set the start direction and the limits of the pitch
// The start pitch is VIEW_PITCH_DEG, or the nearest limit when it is not between the limits
void HeadCameraInit(HeadCamera *camera, float yaw, float pitch_min, float pitch_max);

// Turn the head with the mouse movement (in pixels) since the last frame
// The yaw cannot be more than HEAD_MAX_YAW_DEG to the left or to the right
void HeadCameraUpdate(HeadCamera *camera, float mouse_dx, float mouse_dy);

#endif
