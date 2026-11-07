#ifndef CAR_CAR_PHYSICS_H
#define CAR_CAR_PHYSICS_H

#include <stdbool.h>

#include "car/car.h"
#include "level/surface.h"

// What the driver does with the pedals and the steering wheel
typedef struct CarControls
{
    float throttle; // from 0 to 1
    float brake;    // from 0 to 1
    float steer;    // from -1 to 1, the steering wheel turns towards this value
    bool clutch;    // true when the clutch pedal is down, then the engine does not turn the wheels
    bool handbrake; // true when the handbrake is pulled, then the back of the car slides out
} CarControls;

// Return the speed of the car at the red line of the engine in a gear, in map pixels each second
// Return 0 for neutral
float CarGearTopSpeed(int gear);

// Move the car for a time step, on the surface under the car
// The function does not read input, so it can be tested without a window
void CarPhysicsStep(Car *car, const CarControls *controls, Surface surface, float dt);

#endif
