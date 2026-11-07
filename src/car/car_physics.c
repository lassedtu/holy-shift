#include "car/car_physics.h"

#include <math.h>

#include "raymath.h"
#include "util/math_util.h"

// Map pixels and seconds, 8 map pixels are about 1 meter
#define WHEELBASE 17.0f
#define STEER_LOCK_RAD 0.6f    // angle of the front wheels at full lock, at low speed
#define STEER_HALF_SPEED 300.0f // the lock is half of STEER_LOCK_RAD at this speed
#define STEER_RATE 4.0f        // steering change each second towards the input, 1 is full lock
#define STEER_RETURN_RATE 4.5f // steering change each second back to the centre

#define RPM_IDLE 900.0f
#define RPM_PEAK 3500.0f // the engine pulls hardest at this speed
#define RPM_LAUNCH 2500.0f // engine speed at full throttle while the clutch slips at the start
#define RPM_FREE_RATE 9000.0f // change each second of an engine that does not turn the wheels

#define DRIVE_FORCE 6000.0f        // acceleration at peak torque times the top speed of the gear
#define ENGINE_BRAKE_FORCE 1000.0f // deceleration at the red line without throttle, times the top speed of the gear
#define OVER_REV_BRAKE 4.0f        // engine braking is this many times stronger above CAR_RPM_LIMIT
#define BRAKE_DECELERATION 80.0f
#define ROLLING_RESISTANCE 3.0f
#define AIR_DRAG 1.35e-4f // deceleration for each speed squared

#define ROAD_GRIP 85.0f  // largest sideways acceleration on the road
#define GRASS_GRIP 45.0f
#define GRASS_DRAG 0.5f  // extra deceleration on grass for each map pixel each second of speed
#define GRASS_BRAKE 0.7f // the brakes are weaker on grass
#define YAW_GRIP 1.1f    // the car can turn this many times faster than the grip can hold, then it slides
#define SLIDE_SCRUB 0.6f // loss of speed for each map pixel each second of sliding

#define HANDBRAKE_YAW 1.8f          // the handbrake makes the car turn this many times faster
#define HANDBRAKE_YAW_GRIP 2.5f     // YAW_GRIP while the handbrake is pulled
#define HANDBRAKE_GRIP 0.35f        // part of the sideways grip while the handbrake is pulled
#define HANDBRAKE_DECELERATION 30.0f

// Index is the gear plus 1: reverse, neutral, then 1 to 5
static const float GEAR_TOP_SPEED[CAR_GEAR_TOP + 2] = {90.0f, 0.0f, 110.0f, 175.0f, 245.0f, 310.0f, 370.0f};

float CarGearTopSpeed(int gear)
{
    if (gear < CAR_GEAR_REVERSE || gear > CAR_GEAR_TOP)
    {
        return 0.0f;
    }
    return GEAR_TOP_SPEED[gear + 1];
}

// Part of the largest pull of the engine at an engine speed
static float Torque(float rpm)
{
    if (rpm < RPM_PEAK)
    {
        return Lerp(0.75f, 1.0f, Clamp((rpm - RPM_IDLE) / (RPM_PEAK - RPM_IDLE), 0.0f, 1.0f));
    }
    return Lerp(1.0f, 0.7f, (rpm - RPM_PEAK) / (CAR_RPM_RED - RPM_PEAK));
}

static void UpdateSteering(Car *car, float input, float dt)
{
    input = Clamp(input, -1.0f, 1.0f);
    bool returning = fabsf(input) < fabsf(car->steer) || input * car->steer < 0.0f;
    car->steer = MoveToward(car->steer, input, (returning ? STEER_RETURN_RATE : STEER_RATE) * dt);
}

// Return the acceleration of the engine along the car, and set the engine speed
// The engine braking is added to resist, because it can only slow the car down
static float UpdateEngine(Car *car, const CarControls *controls, float *resist, float dt)
{
    float top = CarGearTopSpeed(car->gear);
    if (top <= 0.0f || controls->clutch)
    {
        float target = RPM_IDLE + controls->throttle * (CAR_RPM_LIMIT - RPM_IDLE);
        car->rpm = MoveToward(car->rpm, target, RPM_FREE_RATE * dt);
        return 0.0f;
    }

    float wheel_rpm = fabsf(car->speed) * CAR_RPM_RED / top;
    float slip_rpm = RPM_IDLE + controls->throttle * (RPM_LAUNCH - RPM_IDLE);
    car->rpm = fmaxf(wheel_rpm, slip_rpm);

    if (wheel_rpm > slip_rpm)
    {
        float engine_brake = (1.0f - controls->throttle) * ENGINE_BRAKE_FORCE * wheel_rpm / CAR_RPM_RED / top;
        if (wheel_rpm > CAR_RPM_LIMIT)
        {
            engine_brake = OVER_REV_BRAKE * ENGINE_BRAKE_FORCE * wheel_rpm / CAR_RPM_RED / top;
        }
        *resist += engine_brake;
    }
    if (car->rpm >= CAR_RPM_LIMIT)
    {
        return 0.0f;
    }
    float direction = car->gear == CAR_GEAR_REVERSE ? -1.0f : 1.0f;
    return direction * controls->throttle * Torque(car->rpm) * DRIVE_FORCE / top;
}

void CarPhysicsStep(Car *car, const CarControls *controls, Surface surface, float dt)
{
    bool grass = surface == SURFACE_GRASS;
    car->throttle = Clamp(controls->throttle, 0.0f, 1.0f);
    UpdateSteering(car, controls->steer, dt);

    float speed = fabsf(car->speed);
    float resist = ROLLING_RESISTANCE + AIR_DRAG * speed * speed;
    resist += Clamp(controls->brake, 0.0f, 1.0f) * BRAKE_DECELERATION * (grass ? GRASS_BRAKE : 1.0f);
    if (grass)
    {
        resist += GRASS_DRAG * speed;
    }
    if (controls->handbrake)
    {
        resist += HANDBRAKE_DECELERATION;
    }
    car->speed += UpdateEngine(car, controls, &resist, dt) * dt;
    car->speed = MoveToward(car->speed, 0.0f, resist * dt);

    float grip = grass ? GRASS_GRIP : ROAD_GRIP;
    float lock = STEER_LOCK_RAD / (1.0f + fabsf(car->speed) / STEER_HALF_SPEED);
    float yaw_rate = car->speed * tanf(car->steer * lock) / WHEELBASE;
    float yaw_max = YAW_GRIP * grip / fmaxf(fabsf(car->speed), 1.0f);
    float side_grip = grip;
    if (controls->handbrake)
    {
        yaw_rate *= HANDBRAKE_YAW;
        yaw_max = HANDBRAKE_YAW_GRIP * grip / fmaxf(fabsf(car->speed), 1.0f);
        side_grip *= HANDBRAKE_GRIP;
    }
    yaw_rate = Clamp(yaw_rate, -yaw_max, yaw_max);

    // The movement keeps its direction while the car turns, then the grip pulls it along the car
    Vector2 forward = {cosf(car->heading), sinf(car->heading)};
    Vector2 right = {-forward.y, forward.x};
    Vector2 velocity = Vector2Add(Vector2Scale(forward, car->speed), Vector2Scale(right, car->slip));
    car->heading += yaw_rate * dt;
    forward = (Vector2){cosf(car->heading), sinf(car->heading)};
    right = (Vector2){-forward.y, forward.x};
    car->speed = Vector2DotProduct(velocity, forward);
    car->slip = MoveToward(Vector2DotProduct(velocity, right), 0.0f, side_grip * dt);
    car->speed = MoveToward(car->speed, 0.0f, SLIDE_SCRUB * fabsf(car->slip) * dt);

    velocity = Vector2Add(Vector2Scale(forward, car->speed), Vector2Scale(right, car->slip));
    car->position = Vector2Add(car->position, Vector2Scale(velocity, dt));
}
