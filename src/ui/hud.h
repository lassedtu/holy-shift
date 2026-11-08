#ifndef UI_HUD_H
#define UI_HUD_H

#include "car/car.h"
#include "game/stage.h"
#include "raylib.h"

// Draw the information for the player in an area of the window
// The stage can be NULL, then only the car is shown
// The title is the name of the stage, it shows during the countdown, NULL for no title
void HudDraw(const Car *car, const Stage *stage, const char *title, Rectangle destination);

// Return a time in seconds as text, for example "1:02.37"
const char *HudFormatTime(float seconds);

#endif
