#ifndef GAME_HINTS_H
#define GAME_HINTS_H

#include <stdbool.h>

#include "car/car.h"
#include "game/stage.h"
#include "raylib.h"
#include "ui/ui.h"

// The hints of the tutorial: short lines of Rajka with the keys, in a small box at the bottom of the view
// Each hint shows one time in each drive, when its situation happens, and goes away when the player
// does what it says, or after some seconds
typedef enum HintId
{
    HINT_INTRO_LOOK, // the intro before the countdown, one step after the other
    HINT_INTRO_MAP,
    HINT_INTRO_NOTES,
    HINT_INTRO_GEARS,
    HINT_INTRO_READY,
    HINT_STALL, // the first hints have priority
    HINT_LAUNCH,
    HINT_SHIFT_UP,
    HINT_BRAKE,
    HINT_HANDBRAKE,
    HINT_MAP,
    HINT_GRASS,
    HINT_COUNT
} HintId;

// What the hints need to know about the game in this frame
typedef struct HintInput
{
    const Car *car;
    const Stage *stage;
    float head_yaw;        // degrees, positive is right
    bool map_visible;      // a part of the map is in the view
    float corner_distance; // map pixels to the entry of the next corner, INFINITY when there is none
    int corner_grade;
    bool corner_hairpin;
    bool on_grass;
    bool braking;
    bool handbrake;
    bool throttle;
} HintInput;

typedef struct Hints
{
    bool enabled;
    bool shown[HINT_COUNT];
    int active;    // the hint on the screen, -1 when there is none
    int closing;   // the hint that slides out, -1 when there is none
    float time;    // seconds since the active hint started
    float slide;   // from 0 (out of the view) to 1 (in its place)
    int start_gear;
    float grass_time;
    float road_time;
    float look_left;  // the largest head yaw to the left and to the right in the intro, degrees
    float look_right;
    bool intro_done; // the player finished the last step of the intro
    bool map;        // false when there is no map, then the hints about the map do not show
    Texture2D portrait;
} Hints;

// Load the portrait, the hints show only when enabled is true
// Without a map (ultra hard) the hints about the map are left out
void HintsInit(Hints *hints, bool enabled, bool map);
void HintsUnload(Hints *hints);

// Show all the hints again, for example when the stage starts again
void HintsReset(Hints *hints);

void HintsUpdate(Hints *hints, const HintInput *input, float dt);

// Return true when the player finished the intro, then the countdown can start
bool HintsIntroDone(const Hints *hints);
void HintsDraw(const Hints *hints, const Ui *ui);

#endif
