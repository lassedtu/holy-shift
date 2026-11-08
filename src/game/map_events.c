#include "game/map_events.h"

#include <math.h>
#include <stddef.h>

#include "core/config.h"
#include "util/math_util.h"

#define FIRST_EVENT_SECONDS 8.0f  // no event in the first seconds of a stage
#define GAP_SECONDS 22.0f         // the average time between two events at a rate of 1
#define MIN_GAP_SECONDS 8.0f
#define FINISH_SECONDS 6.0f       // the time near the finish without new events, for the plan of the gaps
#define EVENT_SECONDS 5.0f        // about how long an event takes, for the plan of the gaps
#define REPEAT_GAP 1.6f           // when all the events of the level happened, the gaps are this much longer
#define FINISH_MARGIN 320.0f      // map pixels, no new event this near the finish

#define SHAKE_SPEED 240.0f        // map pixels each second, the shaky hands are full at this speed
#define SHAKE_ROLL_DEG 2.5f       // how far the shaky hands turn the map
#define SHAKE_MOVE 0.006f         // how far the shaky hands move the map, in meters
#define WARN_SHAKE 1.0f           // extra shake while Luka is about to do something

#define WARN_SECONDS 0.9f
#define TURN_SECONDS 0.5f
#define UPSIDE_DOWN_MIN 3.0f      // seconds that the map stays upside down
#define UPSIDE_DOWN_MAX 5.0f
#define TOO_CLOSE_MIN 3.0f        // seconds that the map stays near the face of Luka
#define TOO_CLOSE_MAX 5.0f
#define PEEK_SECONDS 1.5f         // seconds that the back of the map shows

// The vomit lands on the windshield, its units are degrees (see render/vomit.h)
#define SPLASH_SECONDS 0.15f      // the vomit lands in this time
#define CLEAN_SECONDS 0.4f        // the end of the sick event after the last splash is gone
#define SPLAT_YAW 4.0f            // the middle of the big splat: over the road ahead, a little to the side of Luka
#define SPLAT_DOWN 2.0f
#define SPLAT_RADIUS 15.0f
#define SIDE_SPLATS 2             // smaller splats at the left and the right of the big splat
#define SIDE_RADIUS 6.5f
#define SWING_ACCEL 28.0f         // map pixels each second squared (about 0.35 g), a stronger side swing counts
#define SWING_LOOSEN 0.32f        // how much one swing loosens a splash, from 1 (stuck) to 0 (loose)
#define SWING_NUDGE 0.8f          // degrees that a swing moves a splash to the side
#define SWING_PUSH 22.0f          // degrees each second that a loose splash gets from a swing
#define DRY_SECONDS 28.0f         // a splash gets loose by itself in this time
#define SLIDE_GRAVITY 35.0f       // degrees each second squared, a loose splash slides down the glass

// The events that can come at random moments. Shaky hands is not one of them, it is always on
// The other events are added here when they are made
static const bool SCHEDULED[MAP_EVENT_TYPE_COUNT] = {
    [MAP_EVENT_UPSIDE_DOWN] = true,
    [MAP_EVENT_SICK] = true,
    [MAP_EVENT_TOO_CLOSE] = true,
    [MAP_EVENT_PEEK] = true,
};

// A xorshift generator, the high bits are used, so the numbers do not repeat in short patterns
static float Random(MapEvents *events)
{
    unsigned int x = events->random;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    events->random = x;
    return (float)(x >> 8) / 16777216.0f;
}

// Return true when each event that can come has happened at least one time
static bool AllUsed(const MapEvents *events);

// Return the number of events that can come and did not happen yet
static int Unused(const MapEvents *events);

static float NextGap(MapEvents *events)
{
    float gap = GAP_SECONDS / events->rate * (0.6f + 0.8f * Random(events));
    if (AllUsed(events))
    {
        gap *= REPEAT_GAP;
    }
    else if (events->time_left > 0.0f)
    {
        // Each event of the level comes at least one time: the gaps get shorter so the rest fit before the finish
        float fit = (events->time_left - FINISH_SECONDS) / ((float)Unused(events) + 0.5f) - EVENT_SECONDS;
        gap = fminf(gap, fit);
    }
    return fmaxf(gap, MIN_GAP_SECONDS);
}

void MapEventsInit(MapEvents *events, const Level *level, Difficulty difficulty)
{
    *events = (MapEvents){0};
    const DifficultyRules *rules = &DIFFICULTY_RULES[difficulty];
    events->rate = rules->map ? rules->map_events : 0.0f;
    for (int i = 0; i < MAP_EVENT_TYPE_COUNT; i++)
    {
        events->allowed[i] = level->map_events[i];
    }
    events->seed = 2166136261u;
    for (const char *c = level->name; *c != '\0'; c++)
    {
        events->seed = (events->seed ^ (unsigned char)*c) * 16777619u;
    }
    events->seed = events->seed != 0 ? events->seed : 0x6D2B79F5u;
    MapEventsReset(events);
}

void MapEventsReset(MapEvents *events)
{
    events->active = -1;
    events->time = 0.0f;
    events->random = events->seed != 0 ? events->seed : 0x6D2B79F5u;
    events->wait = FIRST_EVENT_SECONDS + 6.0f * Random(events);
    events->last_type = -1;
    events->time_left = 0.0f;
    for (int i = 0; i < MAP_EVENT_TYPE_COUNT; i++)
    {
        events->used[i] = 0;
    }
    events->pose = (MapPose){0};
    events->vomit_count = 0;
    events->sick_done = false;
    events->has_heading = false;
    events->swing = 0;
    events->cue = MAP_CUE_NONE;
}

// Return true when an event can come now: the level allows it, it is made, and sick only comes one time
static bool CanCome(const MapEvents *events, int type)
{
    bool used_up = type == MAP_EVENT_SICK && events->sick_done;
    return events->allowed[type] && SCHEDULED[type] && !used_up;
}

static bool AllUsed(const MapEvents *events)
{
    for (int i = 0; i < MAP_EVENT_TYPE_COUNT; i++)
    {
        if (CanCome(events, i) && events->used[i] == 0)
        {
            return false;
        }
    }
    return true;
}

static int Unused(const MapEvents *events)
{
    int count = 0;
    for (int i = 0; i < MAP_EVENT_TYPE_COUNT; i++)
    {
        count += CanCome(events, i) && events->used[i] == 0;
    }
    return count;
}

// Choose one of the events that happened the fewest times, so each event comes before any event comes again
// The event before is not chosen again when there is another one, return -1 when no event can come
static int Choose(MapEvents *events)
{
    int fewest = 1 << 30;
    int others = 0;
    for (int i = 0; i < MAP_EVENT_TYPE_COUNT; i++)
    {
        if (CanCome(events, i) && i != events->last_type)
        {
            others++;
            fewest = events->used[i] < fewest ? events->used[i] : fewest;
        }
    }
    int choices[MAP_EVENT_TYPE_COUNT];
    int count = 0;
    for (int i = 0; i < MAP_EVENT_TYPE_COUNT; i++)
    {
        bool not_last = i != events->last_type || others == 0;
        if (CanCome(events, i) && not_last && (others == 0 || events->used[i] == fewest))
        {
            choices[count++] = i;
        }
    }
    if (count == 0)
    {
        return -1;
    }
    int pick = (int)(Random(events) * (float)count);
    return choices[pick < count ? pick : count - 1];
}

static void SetCue(MapEvents *events, MapCue cue)
{
    events->cue = cue;
    events->cue_type = events->active;
}

static void Start(MapEvents *events, int type)
{
    events->active = type;
    events->used[type]++;
    events->last_type = type;
    events->phase = MAP_PHASE_WARN;
    events->time = 0.0f;
    switch (type)
    {
    case MAP_EVENT_TOO_CLOSE:
        events->hold = TOO_CLOSE_MIN + (TOO_CLOSE_MAX - TOO_CLOSE_MIN) * Random(events);
        break;
    case MAP_EVENT_PEEK:
        events->hold = PEEK_SECONDS;
        break;
    default:
        events->hold = UPSIDE_DOWN_MIN + (UPSIDE_DOWN_MAX - UPSIDE_DOWN_MIN) * Random(events);
        break;
    }
    SetCue(events, MAP_CUE_WARN);
}

// Throw the vomit on the windshield: one big splat over the road ahead, a smaller splat at each side of it,
// and a few drops around them. The streaks of the splats leave gaps to look through
static void Splash(MapEvents *events)
{
    Vector2 middle = {SPLAT_YAW + (Random(events) - 0.5f) * 4.0f, SPLAT_DOWN + (Random(events) - 0.5f) * 2.0f};
    float big = SPLAT_RADIUS * (0.9f + 0.2f * Random(events));
    int count = 0;
    events->vomit[count++] = (VomitBlob){.position = middle, .radius = big, .stick = 1.0f, .seed = events->random,
                                         .alive = true};
    for (int side = 0; side < SIDE_SPLATS; side++)
    {
        Random(events);
        float direction = side == 0 ? -1.0f : 1.0f;
        Vector2 at = {middle.x + direction * big * (1.25f + 0.25f * Random(events)),
                      middle.y + (Random(events) - 0.3f) * big * 0.5f};
        events->vomit[count++] = (VomitBlob){.position = at, .radius = SIDE_RADIUS * (0.8f + 0.4f * Random(events)),
                                             .stick = 1.0f, .seed = events->random, .alive = true};
    }
    int drops = 3 + (int)(Random(events) * 4.0f);
    for (int i = 0; i < drops && count < MAP_EVENTS_MAX_VOMIT; i++)
    {
        float angle = Random(events) * 2.0f * PI;
        float out = big * (1.2f + 0.9f * Random(events));
        Vector2 at = {middle.x + cosf(angle) * out, middle.y + sinf(angle) * out * 0.6f};
        events->vomit[count++] = (VomitBlob){.position = at, .radius = 0.6f + Random(events) * 0.8f, .stick = 1.0f,
                                             .seed = events->random, .alive = true};
    }
    events->vomit_count = count;
}

// Return the side acceleration of the car in map pixels each second squared, from the speed and the turn rate
static float SideAcceleration(MapEvents *events, float speed, float heading, float dt)
{
    float turn = 0.0f;
    if (events->has_heading && dt > 0.0f)
    {
        turn = AngleDifference(events->last_heading, heading) / dt;
    }
    events->last_heading = heading;
    events->has_heading = true;
    return speed * turn;
}

// Each strong swing to the other side loosens the splashes and pushes them that way
// A loose splash slides down and to the side, and is gone when it leaves the windshield
static void Clean(MapEvents *events, float side, float dt)
{
    int side_now = side > SWING_ACCEL ? 1 : (side < -SWING_ACCEL ? -1 : 0);
    bool swung = side_now != 0 && side_now != events->swing;
    if (side_now != 0)
    {
        events->swing = side_now;
    }
    for (int i = 0; i < events->vomit_count; i++)
    {
        VomitBlob *blob = &events->vomit[i];
        if (!blob->alive)
        {
            continue;
        }
        if (swung)
        {
            blob->stick -= SWING_LOOSEN * (0.7f + 0.6f * Random(events));
            blob->position.x += (float)side_now * SWING_NUDGE;
            blob->velocity.x += (float)side_now * SWING_PUSH * (1.0f - fmaxf(blob->stick, 0.0f));
        }
        blob->stick -= dt / DRY_SECONDS;
        if (blob->stick <= 0.0f)
        {
            blob->velocity.y += SLIDE_GRAVITY * dt;
            blob->position.x += blob->velocity.x * dt;
            blob->position.y += blob->velocity.y * dt;
        }
        float r = blob->radius;
        if (blob->position.x < VOMIT_GLASS_LEFT - r || blob->position.x > VOMIT_GLASS_RIGHT + r ||
            blob->position.y > VOMIT_GLASS_BOTTOM + r)
        {
            blob->alive = false;
        }
    }
}

static bool AnyVomit(const MapEvents *events)
{
    for (int i = 0; i < events->vomit_count; i++)
    {
        if (events->vomit[i].alive)
        {
            return true;
        }
    }
    return false;
}

static float PhaseLength(const MapEvents *events)
{
    if (events->active == MAP_EVENT_SICK)
    {
        float lengths[] = {WARN_SECONDS, SPLASH_SECONDS, 0.0f, CLEAN_SECONDS};
        return lengths[events->phase];
    }
    float lengths[] = {WARN_SECONDS, TURN_SECONDS, events->hold, TURN_SECONDS};
    return lengths[events->phase];
}

// Go to the next phase when the time of the phase is over, the hold of sick is over when the map is clean
static void Advance(MapEvents *events)
{
    bool held = events->active == MAP_EVENT_SICK && events->phase == MAP_PHASE_HOLD;
    if (held ? AnyVomit(events) : events->time < PhaseLength(events))
    {
        return;
    }
    events->time = held ? 0.0f : events->time - PhaseLength(events);
    if (events->phase == MAP_PHASE_OUT)
    {
        SetCue(events, MAP_CUE_END);
        if (events->active == MAP_EVENT_SICK)
        {
            events->sick_done = true;
        }
        events->active = -1;
        events->wait = NextGap(events);
        return;
    }
    events->phase = (MapEventPhase)(events->phase + 1);
    if (events->phase == MAP_PHASE_IN)
    {
        SetCue(events, MAP_CUE_START);
        if (events->active == MAP_EVENT_SICK)
        {
            Splash(events);
        }
    }
}

// Return how far the event has changed the map, from 0 (normal) to 1 (fully changed)
static float Amount(const MapEvents *events)
{
    switch (events->phase)
    {
    case MAP_PHASE_WARN:
        return 0.0f;
    case MAP_PHASE_IN:
    {
        float t = fminf(events->time / TURN_SECONDS, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
    case MAP_PHASE_HOLD:
        return 1.0f;
    case MAP_PHASE_OUT:
    {
        float t = fminf(events->time / TURN_SECONDS, 1.0f);
        return 1.0f - t * t * (3.0f - 2.0f * t);
    }
    }
    return 0.0f;
}

void MapEventsUpdate(MapEvents *events, const Stage *stage, float speed, float heading, float dt)
{
    events->pose = (MapPose){0};
    events->cue = MAP_CUE_NONE;
    float side = SideAcceleration(events, speed, heading, dt);
    if (events->rate <= 0.0f && !events->allowed[MAP_EVENT_SHAKE])
    {
        return;
    }

    // The time to the finish at the average speed so far
    bool running = stage->phase == STAGE_RUNNING;
    if (running && stage->progress > 1.0f && stage->time > 1.0f)
    {
        float average = stage->progress / stage->time;
        events->time_left = (stage->track->length - stage->progress) / average;
    }

    // Start an event when it is time, only while the stage runs and not near the finish
    bool near_finish = stage->progress > stage->track->length - FINISH_MARGIN;
    if (events->active < 0 && running && events->rate > 0.0f)
    {
        events->wait -= dt;
        if (events->wait <= 0.0f && !near_finish)
        {
            int type = Choose(events);
            if (type >= 0)
            {
                Start(events, type);
            }
            else
            {
                events->wait = NextGap(events);
            }
        }
    }
    if (events->active >= 0)
    {
        events->time += dt;
        Advance(events);
    }
    if (events->vomit_count > 0)
    {
        Clean(events, side, dt);
    }

    // Shaky hands: a wobble that grows with the speed, and more while Luka is about to do something
    float shake = 0.0f;
    if (events->allowed[MAP_EVENT_SHAKE])
    {
        shake = fminf(fabsf(speed) / SHAKE_SPEED, 1.0f);
    }
    if (events->active >= 0 && events->phase == MAP_PHASE_WARN)
    {
        shake += WARN_SHAKE;
    }
    events->shake_time += dt;
    float t = events->shake_time;
    events->pose.roll_deg = shake * SHAKE_ROLL_DEG * (sinf(t * 17.0f) * 0.6f + sinf(t * 29.0f) * 0.4f);
    events->pose.offset.x = shake * SHAKE_MOVE * sinf(t * 23.0f + 1.0f);
    events->pose.offset.y = shake * SHAKE_MOVE * sinf(t * 19.0f + 2.0f);

    switch (events->active)
    {
    case MAP_EVENT_UPSIDE_DOWN:
        events->pose.roll_deg += 180.0f * Amount(events);
        break;
    case MAP_EVENT_TOO_CLOSE:
        events->pose.closer = Amount(events);
        break;
    case MAP_EVENT_PEEK:
        events->pose.flip = Amount(events);
        break;
    default:
        break;
    }
}

// The lines of the characters for each moment, some moments have a few lines to choose from
typedef struct CueLines
{
    int type;
    MapCue cue;
    SpeechLine first[3]; // one of these is said, the speaker can be NULL for no line
    SpeechLine second;   // said after the first, the speaker can be NULL for no line
} CueLines;

static const CueLines LINES[] = {
    {MAP_EVENT_UPSIDE_DOWN, MAP_CUE_WARN,
     {{"luka", "scared", "Wait, which way is up?", ""}, {"luka", "scared", "Is this the top?", ""},
      {"luka", "", "Hold on, it's the wrong way...", ""}},
     {NULL, NULL, NULL, NULL}},
    {MAP_EVENT_UPSIDE_DOWN, MAP_CUE_START,
     {{"rajka", "angry", "The map, Luka!", ""}, {"rajka", "angry", "Other way round!", ""},
      {"rajka", "angry", "Luka! It's upside down!", ""}},
     {NULL, NULL, NULL, NULL}},
    {MAP_EVENT_UPSIDE_DOWN, MAP_CUE_END,
     {{"luka", "", "Sorry!", ""}, {"luka", "", "Sorry, sorry.", ""}, {"luka", "scared", "Better?", ""}},
     {NULL, NULL, NULL, NULL}},
    {MAP_EVENT_SICK, MAP_CUE_WARN,
     {{"luka", "sick", "Oh no...", ""}, {"luka", "sick", "Grandma, I don't feel good...", ""},
      {"luka", "sick", "Oh no. Oh no no no.", ""}},
     {NULL, NULL, NULL, NULL}},
    {MAP_EVENT_SICK, MAP_CUE_START,
     {{"luka", "sick", "Hhuuurgh!", ""}, {"luka", "sick", "Bleuuugh!", ""}, {"luka", "sick", "Hhuuurgh!", ""}},
     {"rajka", "angry", "Not on the windshield!", "Look at the map, or swing the car left and right to shake it off"}},
    {MAP_EVENT_TOO_CLOSE, MAP_CUE_WARN,
     {{"luka", "scared", "I can't see anything...", ""}, {"luka", "", "Hang on, let me look closer.", ""},
      {"luka", "scared", "Is this a left or a right?", ""}},
     {NULL, NULL, NULL, NULL}},
    {MAP_EVENT_TOO_CLOSE, MAP_CUE_START,
     {{"rajka", "angry", "Not so close!", ""}, {"rajka", "angry", "Luka, I can't see the map!", ""},
      {"rajka", "angry", "Further away!", ""}},
     {NULL, NULL, NULL, NULL}},
    {MAP_EVENT_TOO_CLOSE, MAP_CUE_END,
     {{"luka", "", "Sorry.", ""}, {"luka", "", "There. Better?", ""}, {"luka", "", "Sorry, sorry.", ""}},
     {NULL, NULL, NULL, NULL}},
    {MAP_EVENT_PEEK, MAP_CUE_WARN,
     {{"luka", "", "Maybe if I look just once...", ""}, {"luka", "", "How fast are we going?", ""},
      {"luka", "", "I'll just have a little look...", ""}},
     {NULL, NULL, NULL, NULL}},
    {MAP_EVENT_PEEK, MAP_CUE_START,
     {{"luka", "scared", "AAAH!", ""}, {"luka", "scared", "AAAAAH! TREES!", ""}, {"luka", "scared", "NOPE!", ""}},
     {"rajka", "angry", "Turn it around!", ""}},
    {MAP_EVENT_PEEK, MAP_CUE_END,
     {{"luka", "scared", "I'm never looking again.", ""}, {"luka", "scared", "That was a mistake.", ""},
      {"luka", "scared", "Eyes closed. Eyes closed.", ""}},
     {NULL, NULL, NULL, NULL}},
    {MAP_EVENT_SICK, MAP_CUE_END,
     {{"luka", "sick", "...I'm fine. Left. Probably.", ""}, {"luka", "sick", "I'm okay. I think.", ""},
      {"luka", "sick", "...it's mostly off.", ""}},
     {NULL, NULL, NULL, NULL}},
};

bool MapEventsLine(MapEvents *events, int index, SpeechLine *line)
{
    if (events->cue == MAP_CUE_NONE)
    {
        return false;
    }
    for (size_t i = 0; i < sizeof(LINES) / sizeof(LINES[0]); i++)
    {
        const CueLines *lines = &LINES[i];
        if (lines->type != events->cue_type || lines->cue != events->cue)
        {
            continue;
        }
        if (index == 0)
        {
            *line = lines->first[(int)(Random(events) * 2.999f)];
        }
        else
        {
            *line = lines->second;
        }
        return line->speaker != NULL;
    }
    return false;
}
