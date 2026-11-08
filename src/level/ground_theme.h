#ifndef LEVEL_GROUND_THEME_H
#define LEVEL_GROUND_THEME_H

#include <string.h>

// The look of the ground of a level, the "theme" key of level.json
// The level builder also uses this file
typedef enum GroundTheme
{
    GROUND_FOREST,   // gravel road, green grass, moss and pine needles
    GROUND_AUTUMN,   // light gravel, yellow grass and many fallen leaves
    GROUND_MUD,      // wet dark road with deep ruts and puddles
    GROUND_LAKESIDE, // gravel road, light grass and sand at the side of the road
    GROUND_THEME_COUNT
} GroundTheme;

static const char *const GROUND_THEME_NAMES[GROUND_THEME_COUNT] = {"forest", "autumn", "mud", "lakeside"};

// Return the theme with a name, or GROUND_FOREST when no theme has the name
static inline GroundTheme GroundThemeFind(const char *name)
{
    for (int i = 0; i < GROUND_THEME_COUNT; i++)
    {
        if (strcmp(name, GROUND_THEME_NAMES[i]) == 0)
        {
            return (GroundTheme)i;
        }
    }
    return GROUND_FOREST;
}

#endif
