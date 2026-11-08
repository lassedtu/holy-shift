#include "game/hints.h"

#include <math.h>
#include <stddef.h>

#include "core/config.h"

#define HINT_SECONDS 7.0f      // a hint goes away after this time when the player does not do what it says
#define SLIDE_SECONDS 0.2f
#define SHIFT_RPM 5600.0f      // the shift light of the HUD is on above this engine speed
#define MAP_DELAY 4.0f         // seconds after the start before the hint about the map
#define MAP_LOOK_YAW 45.0f     // the player looks at the map when the head yaw is more than this
#define BRAKE_DISTANCE 400.0f  // map pixels before a slow corner where the hint about braking shows
#define BRAKE_GRADE 3          // corners of this grade and slower
#define BRAKE_SPEED 50.0f      // km/h, the hint about braking shows only above this speed
#define HAIRPIN_DISTANCE 320.0f
#define GRASS_SECONDS 1.5f     // seconds on the grass before the hint about the grass
#define ROAD_SECONDS 1.0f      // seconds back on the road before the hint about the grass goes away
#define LAUNCH_AFTER_GO 1.0f   // the hint about the launch stays this long after the start
#define LOOK_AROUND_YAW 30.0f  // the player has looked around after the head turned this far to the left and the right
#define READ_SECONDS 4.0f      // the shortest time of the intro steps that only explain

// The box, in game pixels: smaller than the dialogue of the scenes, so it is not in the way
#define TEXT_SIZE 15
#define PORTRAIT_SIZE 40.0f
#define FRAME_SIZE 46.0f
#define PADDING 6.0f
#define BOTTOM_MARGIN 8.0f
#define COLOR_LINE ((Color){60, 40, 24, 255})
#define COLOR_KEYS ((Color){132, 98, 64, 255})

typedef struct HintText
{
    const char *line; // what Rajka says
    const char *keys; // the keys, "" when there are none
} HintText;

static const HintText TEXTS[HINT_COUNT] = {
    [HINT_INTRO_LOOK] = {"This is our old Jugo. Have a look around.", "Move the mouse to look"},
    [HINT_INTRO_MAP] = {"Luka holds the map. He can't look at the road.", "Look to the right at the map"},
    [HINT_INTRO_NOTES] = {"The map calls the next corner. Red means slow.", "Look at it often while you drive"},
    [HINT_INTRO_GEARS] = {"There is no automatic. You shift the gears.", "Right mouse or E up, left mouse or Q down"},
    [HINT_INTRO_READY] = {"Eyes on the road. Ready when you are.", "Press W to start the countdown"},
    [HINT_STALL] = {"You stalled it. Shift down and it starts again.", "Left mouse or Q"},
    [HINT_LAUNCH] = {"Give it some gas. Not too much.", "Hold W, keep the needle in the green"},
    [HINT_SHIFT_UP] = {"Hear that? Shift up.", "Right mouse or E"},
    [HINT_BRAKE] = {"Brake before the corner, not in it.", "S to brake, left mouse or Q to shift down"},
    [HINT_HANDBRAKE] = {"Tight one. Use the handbrake.", "Space"},
    [HINT_MAP] = {"Check Luka's map for the next corner.", "Look to the right"},
    [HINT_GRASS] = {"That's a field. Get back on the road.", ""},
};

void HintsInit(Hints *hints, bool enabled)
{
    *hints = (Hints){.enabled = enabled};
    HintsReset(hints);
    const char *path = STORY_PORTRAITS_DIR "/rajka.png";
    if (enabled && FileExists(path))
    {
        hints->portrait = LoadTexture(path);
        SetTextureFilter(hints->portrait, TEXTURE_FILTER_POINT);
    }
}

void HintsUnload(Hints *hints)
{
    if (IsTextureValid(hints->portrait))
    {
        UnloadTexture(hints->portrait);
    }
    hints->portrait = (Texture2D){0};
}

void HintsReset(Hints *hints)
{
    for (int i = 0; i < HINT_COUNT; i++)
    {
        hints->shown[i] = false;
    }
    hints->active = -1;
    hints->closing = -1;
    hints->slide = 0.0f;
    hints->grass_time = 0.0f;
    hints->road_time = 0.0f;
    hints->look_left = 0.0f;
    hints->look_right = 0.0f;
    hints->intro_done = false;
}

bool HintsIntroDone(const Hints *hints)
{
    return hints->intro_done;
}

static bool Intro(const HintInput *input)
{
    return input->stage->phase == STAGE_INTRO;
}

static bool IsIntroHint(int id)
{
    return id >= HINT_INTRO_LOOK && id <= HINT_INTRO_READY;
}

static bool Running(const HintInput *input)
{
    return input->stage->phase == STAGE_RUNNING;
}

static float SpeedKmh(const Car *car)
{
    return fabsf(car->speed) / MAP_PIXELS_PER_METER * 3.6f;
}

// Return true when the situation of a hint happens
static bool Starts(const Hints *hints, HintId id, const HintInput *input)
{
    const Car *car = input->car;
    switch (id)
    {
    case HINT_INTRO_LOOK:
        return Intro(input);
    case HINT_INTRO_MAP:
    case HINT_INTRO_NOTES:
    case HINT_INTRO_GEARS:
    case HINT_INTRO_READY:
        return Intro(input) && hints->shown[id - 1];
    case HINT_STALL:
        return Running(input) && car->engine_off;
    case HINT_LAUNCH:
        return input->stage->phase == STAGE_COUNTDOWN;
    case HINT_SHIFT_UP:
        return Running(input) && car->gear >= 1 && car->gear < CAR_GEAR_TOP && car->rpm >= SHIFT_RPM;
    case HINT_BRAKE:
        return Running(input) && input->corner_distance < BRAKE_DISTANCE && input->corner_grade <= BRAKE_GRADE &&
               SpeedKmh(car) > BRAKE_SPEED;
    case HINT_HANDBRAKE:
        return Running(input) && input->corner_hairpin && input->corner_distance < HAIRPIN_DISTANCE;
    case HINT_MAP:
        return Running(input) && input->stage->time >= MAP_DELAY && !input->map_visible;
    case HINT_GRASS:
        return Running(input) && hints->grass_time >= GRASS_SECONDS;
    case HINT_COUNT:
        break;
    }
    return false;
}

// Return true when the player did what the active hint says
static bool Done(const Hints *hints, const HintInput *input)
{
    const Car *car = input->car;
    switch ((HintId)hints->active)
    {
    case HINT_INTRO_LOOK:
        return fminf(hints->look_left, hints->look_right) >= LOOK_AROUND_YAW && hints->time >= 1.0f;
    case HINT_INTRO_MAP:
        return input->map_visible && input->head_yaw > MAP_LOOK_YAW;
    case HINT_INTRO_NOTES:
    case HINT_INTRO_GEARS:
        return hints->time >= READ_SECONDS;
    case HINT_INTRO_READY:
        return input->throttle;
    case HINT_STALL:
        return !car->engine_off;
    case HINT_LAUNCH:
        return Running(input) && input->stage->time >= LAUNCH_AFTER_GO;
    case HINT_SHIFT_UP:
        return car->gear > hints->start_gear;
    case HINT_BRAKE:
        return input->braking;
    case HINT_HANDBRAKE:
        return input->handbrake;
    case HINT_MAP:
        return input->map_visible && input->head_yaw > MAP_LOOK_YAW;
    case HINT_GRASS:
        return hints->road_time >= ROAD_SECONDS;
    case HINT_COUNT:
        break;
    }
    return true;
}

static void Start(Hints *hints, HintId id, const HintInput *input)
{
    if (hints->active >= 0)
    {
        hints->closing = hints->active;
    }
    hints->active = id;
    hints->shown[id] = true;
    hints->time = 0.0f;
    hints->slide = 0.0f;
    hints->start_gear = input->car->gear;
}

void HintsUpdate(Hints *hints, const HintInput *input, float dt)
{
    if (!hints->enabled)
    {
        return;
    }
    hints->grass_time = input->on_grass ? hints->grass_time + dt : 0.0f;
    hints->road_time = input->on_grass ? 0.0f : hints->road_time + dt;
    if (hints->active == HINT_INTRO_LOOK)
    {
        hints->look_left = fmaxf(hints->look_left, -input->head_yaw);
        hints->look_right = fmaxf(hints->look_right, input->head_yaw);
    }

    if (hints->active >= 0)
    {
        hints->time += dt;
        hints->slide = fminf(hints->slide + dt / SLIDE_SECONDS, 1.0f);
        bool timeout = hints->active != HINT_LAUNCH && hints->active != HINT_STALL && !IsIntroHint(hints->active) &&
                       hints->time >= HINT_SECONDS;
        if (Done(hints, input) || timeout)
        {
            if (hints->active == HINT_INTRO_READY)
            {
                hints->intro_done = true;
            }
            if (hints->active == HINT_INTRO_MAP)
            {
                hints->shown[HINT_MAP] = true; // the player knows the map now
            }
            hints->closing = hints->active;
            hints->active = -1;
        }
    }
    else if (hints->closing >= 0)
    {
        hints->slide = fmaxf(hints->slide - dt / SLIDE_SECONDS, 0.0f);
        if (hints->slide <= 0.0f)
        {
            hints->closing = -1;
        }
    }

    // A stall is urgent, it replaces the hint on the screen
    if (!hints->shown[HINT_STALL] && hints->active != HINT_STALL && Starts(hints, HINT_STALL, input))
    {
        Start(hints, HINT_STALL, input);
        hints->closing = -1;
        return;
    }
    if (hints->active >= 0 || hints->closing >= 0)
    {
        return;
    }
    for (int id = 0; id < HINT_COUNT; id++)
    {
        if (!hints->shown[id] && Starts(hints, (HintId)id, input))
        {
            Start(hints, (HintId)id, input);
            return;
        }
    }
}

void HintsDraw(const Hints *hints, const Ui *ui)
{
    int id = hints->active >= 0 ? hints->active : hints->closing;
    if (!hints->enabled || id < 0)
    {
        return;
    }
    const HintText *text = &TEXTS[id];
    float line_height = UiTextHeight(ui, TEXT_SIZE) * 0.8f;
    float text_width = fmaxf(UiTextWidth(ui, text->line, TEXT_SIZE), UiTextWidth(ui, text->keys, TEXT_SIZE));
    bool portrait = IsTextureValid(hints->portrait);
    float height = FRAME_SIZE + 2.0f * PADDING;
    float width = text_width + 3.0f * PADDING + (portrait ? FRAME_SIZE : 0.0f) + 4.0f;

    // The box slides up from below the view, and slows down at the end
    float out = 1.0f - hints->slide;
    float y = VIEW_HEIGHT - BOTTOM_MARGIN - height + out * out * (height + BOTTOM_MARGIN);
    Rectangle box = {floorf((VIEW_WIDTH - width) / 2.0f), floorf(y), floorf(width), height};
    UiPanel(ui, box);

    float text_x = box.x + 2.0f * PADDING;
    if (portrait)
    {
        Rectangle frame = {box.x + PADDING, box.y + PADDING, FRAME_SIZE, FRAME_SIZE};
        UiInlay(ui, frame);
        float inset = (FRAME_SIZE - PORTRAIT_SIZE) / 2.0f;
        UiTexture(ui, hints->portrait, (Rectangle){0, 0, (float)hints->portrait.width, (float)hints->portrait.height},
                  (Rectangle){frame.x + inset, frame.y + inset, PORTRAIT_SIZE, PORTRAIT_SIZE}, WHITE);
        text_x = frame.x + FRAME_SIZE + PADDING;
    }
    float lines = text->keys[0] != '\0' ? 2.0f : 1.0f;
    float text_y = box.y + (box.height - lines * line_height) / 2.0f - 2.0f;
    UiText(ui, text->line, text_x, text_y, TEXT_SIZE, COLOR_LINE);
    if (text->keys[0] != '\0')
    {
        UiText(ui, text->keys, text_x, text_y + line_height, TEXT_SIZE, COLOR_KEYS);
    }
}
