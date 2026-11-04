#ifndef UI_HUD_H
#define UI_HUD_H

#include "car/car.h"
#include "raylib.h"

// Draw the information for the player in an area of the window
void HudDraw(const Car *car, Rectangle destination);

#endif
