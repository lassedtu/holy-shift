#include "car/path_driver.h"

#include <math.h>

#include "raymath.h"
#include "util/math_util.h"

#define SPEED_MAX 260.0f    // map pixels each second on a straight road
#define SPEED_MIN 70.0f     // map pixels each second in a sharp curve
#define ACCELERATION 120.0f // map pixels each second squared
#define BRAKING 260.0f      // map pixels each second squared
#define LOOK_AHEAD 180.0f   // distance in which the curve is checked
#define LOOK_STEP 10.0f     // distance between the checks
#define FULL_TURN_RAD 1.6f  // turn in the look ahead distance for minimum speed
#define HEADING_LOOK 40.0f  // the car looks at the curve this far ahead

#define STEER_LOOK 80.0f    // distance in which the turn sets the steering wheel
#define STEER_FULL_RAD 0.9f // turn in the steering distance for full lock
#define STEER_RATE 2.5f     // steering change each second, 1 is full lock

static void PathDriverUpdate(CarDriver *base, Car *car, float dt)
{
    PathDriver *driver = (PathDriver *)base;
    const Track *track = driver->track;

    float turn =
        Clamp(TrackTurnAhead(track, driver->position, LOOK_AHEAD, LOOK_STEP) / FULL_TURN_RAD, 0.0f,
              1.0f);
    float target_speed = SPEED_MAX - (SPEED_MAX - SPEED_MIN) * turn;
    if (car->speed > target_speed)
    {
        car->speed = fmaxf(target_speed, car->speed - BRAKING * dt);
    }
    else
    {
        car->speed = fminf(target_speed, car->speed + ACCELERATION * dt);
    }

    driver->position += car->speed * dt;
    if (!track->closed && driver->position >= track->length - 1.0f)
    {
        driver->position = 0.0f; // an open track starts again
    }
    driver->position = TrackWrap(track, driver->position);

    Vector2 here = TrackPoint(track, driver->position);
    Vector2 ahead = TrackPoint(track, driver->position + HEADING_LOOK);
    car->position = here;
    if (Vector2Distance(here, ahead) > 0.01f)
    {
        car->heading = atan2f(ahead.y - here.y, ahead.x - here.x);
    }

    float steer_target =
        Clamp(TrackSignedTurn(track, driver->position, STEER_LOOK) / STEER_FULL_RAD, -1.0f, 1.0f);
    car->steer = MoveToward(car->steer, steer_target, STEER_RATE * dt);
}

void PathDriverInit(PathDriver *driver, const Track *track, Car *car)
{
    driver->base.update = PathDriverUpdate;
    driver->track = track;
    driver->position = 0.0f;

    *car = (Car){0};
    car->speed = SPEED_MAX;
    PathDriverUpdate(&driver->base, car, 0.0f);
}
