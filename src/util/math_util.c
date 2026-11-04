#include "util/math_util.h"

#include <math.h>

#include "raylib.h"

float AngleDifference(float a, float b)
{
    float difference = fmodf(b - a, 2.0f * PI);
    if (difference > PI)
    {
        difference -= 2.0f * PI;
    }
    if (difference < -PI)
    {
        difference += 2.0f * PI;
    }
    return difference;
}

float MoveToward(float value, float target, float max_delta)
{
    if (fabsf(target - value) <= max_delta)
    {
        return target;
    }
    return value + (target > value ? max_delta : -max_delta);
}
