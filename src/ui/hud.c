#include "ui/hud.h"

#include <math.h>

#include "core/config.h"
#include "ui/text.h"

#define HUD_MARGIN 5     // view pixels
#define HUD_FONT_SIZE 10 // view pixels
#define HUD_BIG_FONT_SIZE 40
#define RPM_BAR_WIDTH 120 // view pixels
#define RPM_BAR_HEIGHT 6
#define RPM_SHIFT 5600.0f // the shift light is on above this engine speed
#define LAUNCH_TEXT_SECONDS 1.5f
#define SWEET_COLOR ((Color){40, 200, 80, 255})

static const char *GearName(int gear)
{
    static const char *const names[] = {"R", "N", "1", "2", "3", "4", "5"};
    return names[gear - CAR_GEAR_REVERSE];
}

const char *HudFormatTime(float seconds)
{
    int hundredths = (int)(seconds * 100.0f);
    return TextFormat("%d:%02d.%02d", hundredths / 6000, hundredths / 100 % 60, hundredths % 100);
}

static void DrawCentered(const char *text, Rectangle destination, float y, int size, Color color)
{
    int width = TextWidth(text, size);
    TextDraw(text, (int)(destination.x + (destination.width - (float)width) / 2.0f), (int)y, size, color);
}

void HudDraw(const Car *car, const Stage *stage, const char *title, Rectangle destination)
{
    float scale = destination.height / VIEW_HEIGHT;
    int size = (int)(HUD_FONT_SIZE * scale);
    int x = (int)(destination.x + HUD_MARGIN * scale);
    int y = (int)(destination.y + HUD_MARGIN * scale);

    float kmh = fabsf(car->speed) / MAP_PIXELS_PER_METER * 3.6f;
    TextDraw(TextFormat("%3d km/h  gear %s  %4d rpm", (int)kmh, GearName(car->gear), (int)car->rpm), x, y,
             size, WHITE);

    int bar_y = y + size + (int)(2 * scale);
    int bar_width = (int)(RPM_BAR_WIDTH * scale);
    int bar_height = (int)(RPM_BAR_HEIGHT * scale);
    int red_x = x + (int)((float)bar_width * RPM_SHIFT / CAR_RPM_LIMIT);
    int filled = (int)((float)bar_width * fminf(car->rpm / CAR_RPM_LIMIT, 1.0f));
    bool countdown = stage != NULL && stage->phase == STAGE_COUNTDOWN;
    bool shift = !countdown && car->rpm >= RPM_SHIFT && car->gear > CAR_GEAR_NEUTRAL && car->gear < CAR_GEAR_TOP;
    bool sweet = car->rpm >= CAR_LAUNCH_SWEET_LOW && car->rpm <= CAR_LAUNCH_SWEET_HIGH;
    DrawRectangle(x, bar_y, bar_width, bar_height, (Color){0, 0, 0, 140});
    DrawRectangle(red_x, bar_y, x + bar_width - red_x, bar_height, (Color){120, 0, 0, 200});
    if (countdown)
    {
        // The best engine speed for the launch
        int sweet_x = x + (int)((float)bar_width * CAR_LAUNCH_SWEET_LOW / CAR_RPM_LIMIT);
        int sweet_end = x + (int)((float)bar_width * CAR_LAUNCH_SWEET_HIGH / CAR_RPM_LIMIT);
        DrawRectangle(sweet_x, bar_y, sweet_end - sweet_x, bar_height, (Color){20, 90, 40, 220});
    }
    Color fill = shift ? RED : WHITE;
    if (countdown && sweet)
    {
        fill = SWEET_COLOR;
    }
    DrawRectangle(x, bar_y, filled, bar_height, fill);
    if (car->engine_off)
    {
        const char *text = car->starting > 0.0f ? "STARTING" : "STALLED - shift down";
        TextDraw(text, x + bar_width + (int)(4 * scale), bar_y - (size - bar_height) / 2, size, RED);
    }
    else if (shift)
    {
        TextDraw("SHIFT", x + bar_width + (int)(4 * scale), bar_y - (size - bar_height) / 2, size, RED);
    }
    if (stage == NULL)
    {
        return;
    }

    TextDraw(HudFormatTime(stage->time), x, bar_y + bar_height + (int)(2 * scale), size, WHITE);
    int big = (int)(HUD_BIG_FONT_SIZE * scale);
    float middle = destination.y + destination.height * 0.3f;
    if (stage->phase == STAGE_INTRO && title != NULL)
    {
        DrawCentered(title, destination, middle - (float)size * 2.5f, size * 2, WHITE);
    }
    else if (stage->phase == STAGE_COUNTDOWN)
    {
        DrawCentered(TextFormat("%d", (int)ceilf(stage->countdown)), destination, middle, big, WHITE);
        if (title != NULL)
        {
            DrawCentered(title, destination, middle - (float)size * 2.5f, size * 2, WHITE);
        }
    }
    else if (stage->phase == STAGE_RUNNING && stage->time < LAUNCH_TEXT_SECONDS)
    {
        if (stage->time < 1.0f)
        {
            DrawCentered("GO", destination, middle, big, WHITE);
        }
        float below = middle + (float)big + 4.0f * scale;
        if (car->launch_quality >= 0.95f)
        {
            DrawCentered("PERFECT START", destination, below, size * 2, SWEET_COLOR);
        }
        else if (car->launch_quality >= 0.6f)
        {
            DrawCentered("GOOD START", destination, below, size * 2, YELLOW);
        }
    }
}
