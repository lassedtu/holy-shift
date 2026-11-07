#ifndef CAR_PLAYER_DRIVER_H
#define CAR_PLAYER_DRIVER_H

#include <stdbool.h>

#include "car/car.h"
#include "car/car_driver.h"
#include "level/surface.h"
#include "level/track.h"

// A driver that uses the keyboard
// W and S are the throttle and the brake, A and D steer, space is the handbrake
// E and Q or the mouse wheel shift up and down
// The clutch is automatic: it is down for a short time at each shift
typedef struct PlayerDriver
{
    CarDriver base; // must be the first member
    const SurfaceMap *surface;
    float shift_left; // seconds until the clutch is up again after a shift
    bool hold;        // true when the car must stay still, for example before the start
} PlayerDriver;

// Put the car at the start of the track, in first gear
void PlayerDriverInit(PlayerDriver *driver, const SurfaceMap *surface, const Track *track, Car *car);

#endif
