#ifndef CAR_CAR_DRIVER_H
#define CAR_CAR_DRIVER_H

#include "car/car.h"

// A driver moves the car
// The path driver follows the road curve
// The player driver uses the keyboard and the car physics
typedef struct CarDriver CarDriver;
struct CarDriver
{
    void (*update)(CarDriver *driver, Car *car, float dt);
};

static inline void CarDriverUpdate(CarDriver *driver, Car *car, float dt)
{
    driver->update(driver, car, dt);
}

#endif
