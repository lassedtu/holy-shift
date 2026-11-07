#ifndef CORE_GAME_MODE_H
#define CORE_GAME_MODE_H

// The mode decides who moves the camera
// Only one mode is active at a time
typedef enum GameMode
{
    GAME_MODE_NORMAL,           // the player drives the car on the stage
    GAME_MODE_DEBUG_SELFDRIVE,  // the car follows the road curve by itself
    GAME_MODE_DEBUG_FLY         // a free camera without the car, for looking at the level
} GameMode;

#endif
