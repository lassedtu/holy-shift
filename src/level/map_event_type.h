#ifndef LEVEL_MAP_EVENT_TYPE_H
#define LEVEL_MAP_EVENT_TYPE_H

// The kinds of trouble that Luka makes with the map, the "map_events" list of level.json
// The level builder also uses this file
typedef enum MapEventType
{
    MAP_EVENT_SHAKE,       // shaky hands: the map wobbles more at high speed, all the time
    MAP_EVENT_UPSIDE_DOWN, // Luka turns the map the wrong way for a few seconds
    MAP_EVENT_SICK,        // Luka is sick on the map
    MAP_EVENT_TOO_CLOSE,   // Luka holds the map near his face
    MAP_EVENT_PEEK,        // Luka turns the map around, the player sees its back
    MAP_EVENT_TYPE_COUNT
} MapEventType;

static const char *const MAP_EVENT_NAMES[MAP_EVENT_TYPE_COUNT] = {"shake", "upside_down", "sick", "too_close",
                                                                   "peek"};

#endif
