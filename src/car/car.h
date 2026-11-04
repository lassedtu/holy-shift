#ifndef CAR_CAR_H
#define CAR_CAR_H

#include "raylib.h"

// The state of the car, in the map
// A driver changes this state, the camera and the cabin only read it
typedef struct Car
{
    Vector2 position; // map pixels
    float heading;    // radians, 0 is towards +x and a positive angle turns right
    float speed;      // map pixels each second
    float steer;      // -1 is full left and 1 is full right
} Car;

#endif
