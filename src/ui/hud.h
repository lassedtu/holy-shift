#ifndef UI_HUD_H
#define UI_HUD_H

#include "car/car.h"
#include "game/stage.h"
#include "raylib.h"

// Draw the information for the player in an area of the window
// The stage can be NULL, then only the car is shown
void HudDraw(const Car *car, const Stage *stage, Rectangle destination);

#endif
