#ifndef GAME_MAP_EVENTS_H
#define GAME_MAP_EVENTS_H

#include <stdbool.h>

#include "core/difficulty.h"
#include "game/stage.h"
#include "level/level.h"
#include "render/pace_map.h"
#include "render/vomit.h"
#include "ui/speech.h"

#define MAP_EVENTS_MAX_VOMIT 10

// The phases of an event: a warning, the change of the map, the time that it stays, and the way back
typedef enum MapEventPhase
{
    MAP_PHASE_WARN,
    MAP_PHASE_IN,
    MAP_PHASE_HOLD,
    MAP_PHASE_OUT
} MapEventPhase;

// A moment of an event that the characters talk about
typedef enum MapCue
{
    MAP_CUE_NONE,
    MAP_CUE_WARN,  // the event is about to start
    MAP_CUE_START, // the map changed
    MAP_CUE_END    // the map is normal again
} MapCue;

// The trouble that Luka makes with the map during a stage (see "Map events" in plan.md)
// Shaky hands is always on when the level has it, the other events come one at a time at random moments
typedef struct MapEvents
{
    bool allowed[MAP_EVENT_TYPE_COUNT];
    float rate;  // how often the events happen, from the difficulty, 0 for no events
    int active;  // the event that happens now, -1 for none
    MapEventPhase phase;
    float time;  // seconds in the phase
    float hold;  // seconds of the hold phase of the active event
    float wait;  // seconds until the next event can start
    float shake_time;
    unsigned int random;
    unsigned int seed;  // the random numbers start from this value in each run, it comes from the level name
    int used[MAP_EVENT_TYPE_COUNT]; // how many times each event happened in this run
    int last_type;      // the event before, -1 for none
    float time_left;    // an estimate of the seconds until the finish, from the speed so far
    MapPose pose;

    VomitBlob vomit[MAP_EVENTS_MAX_VOMIT]; // on the windshield
    int vomit_count;
    bool sick_done;     // Luka is sick only one time in a stage
    float last_heading; // radians, to find how fast the car turns
    bool has_heading;
    int swing;          // the side of the last strong swing of the car, -1, 0 or 1

    MapCue cue; // the moment of the last update, for the lines of the characters
    int cue_type;
} MapEvents;

// Take the events of the level and the rate of the difficulty
void MapEventsInit(MapEvents *events, const Level *level, Difficulty difficulty);

// Start again with no event and a clean map, for example when the stage starts again
void MapEventsReset(MapEvents *events);

// Move the events on, speed is in map pixels each second and heading is the direction of the car in radians
// Events only start while the stage runs and not near its finish
void MapEventsUpdate(MapEvents *events, const Stage *stage, float speed, float heading, float dt);

// Write a line that a character says about the cue of the last update, return false when there is none
// Some cues have two lines (one from Luka, one from Rajka): index 0 and 1
bool MapEventsLine(MapEvents *events, int index, SpeechLine *line);

#endif
