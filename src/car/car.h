#ifndef CAR_CAR_H
#define CAR_CAR_H

#include "raylib.h"

#define CAR_GEAR_REVERSE -1
#define CAR_GEAR_NEUTRAL 0
#define CAR_GEAR_TOP 5
#define CAR_RPM_RED 6000.0f   // the engine pulls less above this speed
#define CAR_RPM_LIMIT 6300.0f // the engine stops pulling above this speed

// The state of the car, in the map
// A driver changes this state, the camera and the cabin only read it
typedef struct Car
{
    Vector2 position; // map pixels
    float heading;    // radians, 0 is towards +x and a positive angle turns right
    float speed;      // map pixels each second to the front, negative when the car moves back
    float slip;       // map pixels each second to the right, when the car slides
    float steer;      // -1 is full left and 1 is full right
    int gear;         // CAR_GEAR_REVERSE, CAR_GEAR_NEUTRAL or 1 to CAR_GEAR_TOP
    float rpm;        // engine speed
    float throttle;   // from 0 to 1
} Car;

#endif
