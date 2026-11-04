#include "ui/hud.h"

#define HUD_MARGIN 10
#define HUD_FONT_SIZE 20

void HudDraw(const Car *car, Rectangle destination)
{
    DrawText(TextFormat("speed %d", (int)car->speed), (int)destination.x + HUD_MARGIN,
             (int)destination.y + HUD_MARGIN, HUD_FONT_SIZE, WHITE);
}
