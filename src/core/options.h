#ifndef CORE_OPTIONS_H
#define CORE_OPTIONS_H

#include <stdbool.h>
#include <stddef.h>

#include "core/difficulty.h"
#include "core/game_mode.h"

// The options of the command line
//   holy_shift [level name] [--debug_fly | --debug_selfdrive]
//              [--shot <file>] [--skip <seconds>] [--yaw <degrees>] [--pitch <degrees>]
//              [--screen <stages | settings | paused | crashed | finished>] [--frame <n>]
//              [--difficulty <easy | medium | hard | ultra>]
// The level name is the name of a folder in levels/, the first saved level is the default
// --debug_fly is a free camera (W A S D to move, the mouse to look)
// --debug_selfdrive lets the car follow the road curve by itself
// You cannot use the two debug modes together
// Without a level name and without a debug mode, the game starts in the main menu
// The other options are for development: --shot saves a picture and quits (at frame 3, or at the frame of --frame),
// --screen opens a menu at the start,
// --skip runs the self drive for some seconds first, --yaw and --pitch set the angles of the view at the start,
// --difficulty is used in place of the difficulty of the settings
// A positive pitch is down
typedef struct Options
{
    const char *level;      // NULL for the default level
    const char *screenshot; // NULL for no picture
    const char *screen;     // NULL for the main menu
    int shot_frame;         // the frame of the picture of --shot
    GameMode mode;
    float skip_seconds;
    float yaw_deg;
    float pitch_deg;
    bool has_pitch; // false when the command line has no --pitch
    Difficulty difficulty;
    bool has_difficulty; // false when the command line has no --difficulty
} Options;

// Read the command line, write the reason in message when it is not valid
bool OptionsParse(Options *options, int argc, char **argv, char *message, size_t size);

#endif
