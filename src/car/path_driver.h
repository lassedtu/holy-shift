#ifndef CAR_PATH_DRIVER_H
#define CAR_PATH_DRIVER_H

#include "car/car.h"
#include "car/car_driver.h"
#include "level/track.h"

// A driver that follows the road curve
// The car slows down before and in curves
typedef struct PathDriver
{
    CarDriver base; // must be the first member
    const Track *track;
    float position; // distance from the start of the track
} PathDriver;

// Put the car at the start of the track
void PathDriverInit(PathDriver *driver, const Track *track, Car *car);

#endif
