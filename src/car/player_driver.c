#include "car/player_driver.h"

#include <math.h>

#include "car/car_physics.h"
#include "raylib.h"

#define SHIFT_SECONDS 0.15f
#define REVERSE_MAX_SPEED 5.0f // the reverse gear can only be selected below this speed

static void Shift(PlayerDriver *driver, Car *car, int direction)
{
    int gear = car->gear + direction;
    if (gear < CAR_GEAR_REVERSE || gear > CAR_GEAR_TOP)
    {
        return;
    }
    if (gear == CAR_GEAR_REVERSE && fabsf(car->speed) > REVERSE_MAX_SPEED)
    {
        return;
    }
    car->gear = gear;
    driver->shift_left = SHIFT_SECONDS;
    if (direction < 0)
    {
        CarStartEngine(car);
    }
}

static void PlayerDriverUpdate(CarDriver *base, Car *car, float dt)
{
    PlayerDriver *driver = (PlayerDriver *)base;

    float wheel = GetMouseWheelMove();
    if (IsKeyPressed(KEY_E) || wheel > 0.0f)
    {
        Shift(driver, car, 1);
    }
    if (IsKeyPressed(KEY_Q) || wheel < 0.0f)
    {
        Shift(driver, car, -1);
    }
    driver->shift_left = fmaxf(0.0f, driver->shift_left - dt);

    CarControls controls = {
        .throttle = (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) ? 1.0f : 0.0f,
        .brake = (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) ? 1.0f : 0.0f,
        .steer = (float)((IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) -
                         (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))),
        .clutch = driver->shift_left > 0.0f,
        .handbrake = IsKeyDown(KEY_SPACE),
    };
    if (driver->hold)
    {
        controls.brake = 1.0f;
        controls.clutch = true;
    }
    CarPhysicsStep(car, &controls, SurfaceAt(driver->surface, car->position), dt);
}

void PlayerDriverInit(PlayerDriver *driver, const SurfaceMap *surface, const Track *track, Car *car)
{
    *driver = (PlayerDriver){0};
    driver->base.update = PlayerDriverUpdate;
    driver->surface = surface;

    *car = (Car){0};
    car->position = TrackPoint(track, 0.0f);
    car->heading = TrackHeading(track, 0.0f);
    car->gear = 1;
}
