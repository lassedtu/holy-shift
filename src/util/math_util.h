#ifndef UTIL_MATH_UTIL_H
#define UTIL_MATH_UTIL_H

// Return the angle from a to b in radians, from -PI to PI
float AngleDifference(float a, float b);

// Change value towards target by at most max_delta
float MoveToward(float value, float target, float max_delta);

#endif
