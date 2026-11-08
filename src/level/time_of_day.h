#ifndef LEVEL_TIME_OF_DAY_H
#define LEVEL_TIME_OF_DAY_H

#include <stdbool.h>
#include <string.h>

#include "raylib.h"

// The light of a level, the "time_of_day" key of level.json
// A stage keeps one time of day from the start to the finish
// The level builder also uses this file
typedef enum TimeOfDay
{
    TIME_DAY,
    TIME_OVERCAST,
    TIME_DUSK,
    TIME_NIGHT,
    TIME_RAIN,
    TIME_OF_DAY_COUNT
} TimeOfDay;

typedef struct Lighting
{
    const char *name; // the name in level.json
    Color sky;        // the sky high over the horizon
    Color fog;        // the far ground, the far trees and the sky at the horizon
    Vector3 tint;     // multiplies the colors of the ground and the trees
    float fog_start;  // map pixels where the fog starts, it is full at fog_end
    float fog_end;
    float shadow;     // from 0 to 1, the strength of the shadows of the trees
    float headlights; // from 0 to 1, the light of the headlights on the ground and the trees
    Vector3 cabin;    // multiplies the colors of the cabin
    Color map;        // the light on Luka's map
    bool rain;        // rain falls
    float wetness;    // from 0 to 1, how wet and shiny the road is
    Color glint;      // the sparkle of the sun (or the moon) on water, the alpha is how often it sparkles
} Lighting;

static const Lighting LIGHTING[TIME_OF_DAY_COUNT] = {
    [TIME_DAY] = {"day", {90, 160, 230, 255}, {75, 120, 135, 255}, {1.0f, 1.0f, 1.0f}, 80.0f, 500.0f, 1.0f, 0.0f,
                  {1.0f, 1.0f, 1.0f}, {200, 196, 186, 255}, false, 0.0f,
                  {255, 250, 225, 255}},
    [TIME_OVERCAST] = {"overcast", {150, 158, 166, 255}, {124, 132, 138, 255}, {0.80f, 0.83f, 0.86f}, 60.0f, 420.0f,
                       0.25f, 0.0f, {0.84f, 0.86f, 0.90f}, {186, 186, 182, 255}, false, 0.0f,
                       {200, 204, 208, 60}},
    [TIME_DUSK] = {"dusk", {96, 74, 132, 255}, {214, 128, 86, 255}, {1.0f, 0.70f, 0.55f}, 70.0f, 450.0f, 0.6f, 0.0f,
                   {0.92f, 0.68f, 0.52f}, {214, 168, 128, 255}, false, 0.0f,
                   {255, 190, 110, 255}},
    [TIME_NIGHT] = {"night", {8, 12, 28, 255}, {16, 22, 40, 255}, {0.16f, 0.20f, 0.34f}, 40.0f, 320.0f, 0.0f, 1.0f,
                    {0.24f, 0.27f, 0.38f}, {176, 160, 120, 255}, false, 0.0f,
                    {150, 165, 200, 70}},
    [TIME_RAIN] = {"rain", {98, 106, 114, 255}, {88, 96, 104, 255}, {0.62f, 0.66f, 0.70f}, 30.0f, 260.0f, 0.0f, 0.0f,
                   {0.70f, 0.72f, 0.76f}, {176, 178, 178, 255}, true, 1.0f,
                   {170, 178, 186, 25}},
};

// Return the time of day with a name, or TIME_DAY when no time of day has the name
static inline TimeOfDay TimeOfDayFind(const char *name)
{
    for (int i = 0; i < TIME_OF_DAY_COUNT; i++)
    {
        if (strcmp(name, LIGHTING[i].name) == 0)
        {
            return (TimeOfDay)i;
        }
    }
    return TIME_DAY;
}

#endif
