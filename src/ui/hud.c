#include "ui/hud.h"

#include <math.h>

#include "core/config.h"

#define HUD_MARGIN 5     // view pixels
#define HUD_FONT_SIZE 10 // view pixels
#define HUD_BIG_FONT_SIZE 40
#define RPM_BAR_WIDTH 120 // view pixels
#define RPM_BAR_HEIGHT 6
#define RPM_SHIFT 5600.0f // the shift light is on above this engine speed

static const char *GearName(int gear)
{
    static const char *const names[] = {"R", "N", "1", "2", "3", "4", "5"};
    return names[gear - CAR_GEAR_REVERSE];
}

static const char *FormatTime(float seconds)
{
    int hundredths = (int)(seconds * 100.0f);
    return TextFormat("%d:%02d.%02d", hundredths / 6000, hundredths / 100 % 60, hundredths % 100);
}

static void DrawCentered(const char *text, Rectangle destination, float y, int size)
{
    int width = MeasureText(text, size);
    DrawText(text, (int)(destination.x + (destination.width - (float)width) / 2.0f), (int)y, size, WHITE);
}

void HudDraw(const Car *car, const Stage *stage, Rectangle destination)
{
    float scale = destination.height / VIEW_HEIGHT;
    int size = (int)(HUD_FONT_SIZE * scale);
    int x = (int)(destination.x + HUD_MARGIN * scale);
    int y = (int)(destination.y + HUD_MARGIN * scale);

    float kmh = fabsf(car->speed) / MAP_PIXELS_PER_METER * 3.6f;
    DrawText(TextFormat("%3d km/h  gear %s  %4d rpm", (int)kmh, GearName(car->gear), (int)car->rpm), x, y,
             size, WHITE);

    int bar_y = y + size + (int)(2 * scale);
    int bar_width = (int)(RPM_BAR_WIDTH * scale);
    int bar_height = (int)(RPM_BAR_HEIGHT * scale);
    int red_x = x + (int)((float)bar_width * RPM_SHIFT / CAR_RPM_LIMIT);
    int filled = (int)((float)bar_width * fminf(car->rpm / CAR_RPM_LIMIT, 1.0f));
    bool shift = car->rpm >= RPM_SHIFT && car->gear > CAR_GEAR_NEUTRAL && car->gear < CAR_GEAR_TOP;
    DrawRectangle(x, bar_y, bar_width, bar_height, (Color){0, 0, 0, 140});
    DrawRectangle(red_x, bar_y, x + bar_width - red_x, bar_height, (Color){120, 0, 0, 200});
    DrawRectangle(x, bar_y, filled, bar_height, shift ? RED : WHITE);
    if (shift)
    {
        DrawText("SHIFT", x + bar_width + (int)(4 * scale), bar_y - (size - bar_height) / 2, size, RED);
    }
    if (stage == NULL)
    {
        return;
    }

    DrawText(FormatTime(stage->time), x, bar_y + bar_height + (int)(2 * scale), size, WHITE);
    int big = (int)(HUD_BIG_FONT_SIZE * scale);
    float middle = destination.y + destination.height * 0.3f;
    if (stage->phase == STAGE_COUNTDOWN)
    {
        DrawCentered(TextFormat("%d", (int)ceilf(stage->countdown)), destination, middle, big);
    }
    else if (stage->phase == STAGE_RUNNING && stage->time < 1.0f)
    {
        DrawCentered("GO", destination, middle, big);
    }
    else if (stage->phase == STAGE_FINISHED)
    {
        DrawCentered(TextFormat("FINISH  %s", FormatTime(stage->time)), destination, middle, big);
        DrawCentered("R to drive again", destination, middle + (float)big + 4.0f * scale, size);
    }
}
