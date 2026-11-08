#include "game/game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/config.h"
#include "level/logic_map.h"
#include "raymath.h"
#include "render/present.h"
#include "ui/hud.h"

#define SIMULATION_STEP (1.0f / 60.0f)
#define CRASH_FADE_SECONDS 0.35f  // the view fades to black after a crash
#define CRASH_BLACK_SECONDS 0.6f  // the view stays black before the stage starts again
#define RESTART_FADE_SECONDS 0.5f // the view fades in after the stage starts again

static void RestartStage(Game *game, bool fade)
{
    PlayerDriverInit(&game->player_driver, &game->surface, &game->track, &game->car);
    StageStart(&game->stage, &game->track, false);
    PaceMapReset(&game->map, &game->track, &game->car);
    HintsReset(&game->hints);
    MapEventsReset(&game->map_events);
    SpeechClear(&game->speech);
    game->crash_time = -1.0f;
    game->fade_in = fade ? RESTART_FADE_SECONDS : 0.0f;
    CarAudioSetPlaying(&game->audio, true);
}

bool GameInit(Game *game, const Options *options, char *message, size_t size)
{
    memset(game, 0, sizeof(*game));
    game->mode = options->mode;
    game->crash_time = -1.0f;
    game->mouse_ignore = options->screenshot != NULL;
    game->show_hud = true;
    game->mouse_sensitivity = 1.0f;

    char name[LEVEL_MAX_NAME];
    if (options->level != NULL)
    {
        snprintf(name, sizeof(name), "%s", options->level);
    }
    else if (!LevelFindFirst(name, sizeof(name)))
    {
        snprintf(message, size, "No saved levels in %s/", LEVELS_DIR);
        return false;
    }

    if (!LevelLoad(&game->level, name, message, size))
    {
        return false;
    }
    game->difficulty = options->difficulty;
    LevelApplyDifficulty(&game->level, game->difficulty);
    if (options->time_of_day != NULL)
    {
        game->level.time_of_day = TimeOfDayFind(options->time_of_day);
    }
    game->light = &LIGHTING[game->level.time_of_day];
    if (!TrackBuild(&game->track, &game->level))
    {
        snprintf(message, size, "Could not build the track for \"%s\"", name);
        return false;
    }

    PaceNotesBuild(&game->notes, &game->track);

    // The road of the logic map has the width of medium, other difficulties paint it again
    Image logic;
    if (!LogicMapLoad(&logic, &game->level, &game->track, game->difficulty != DIFFICULTY_MEDIUM, message, size))
    {
        GameShutdown(game);
        return false;
    }
    if (!ForestBuild(&game->forest, &game->level, &game->track, &logic))
    {
        snprintf(message, size, "Could not place the trees for \"%s\"", name);
        UnloadImage(logic);
        GameShutdown(game);
        return false;
    }
    bool loaded = LevelTexturesLoad(&game->textures, &game->level, &logic, &game->forest, message, size);
    if (loaded && !SurfaceMapLoad(&game->surface, &game->level, &logic))
    {
        snprintf(message, size, "Not enough memory for the logic map of \"%s\"", name);
        loaded = false;
    }
    UnloadImage(logic);
    loaded = loaded && Mode7Init(&game->mode7, message, size) &&
                  TreesInit(&game->trees, &game->level, message, size) &&
                  CockpitLoad(&game->cockpit, message, size);
    if (loaded && !PaceMapInit(&game->map))
    {
        snprintf(message, size, "Could not make the textures of the map");
        loaded = false;
    }
    if (!loaded)
    {
        GameShutdown(game);
        return false;
    }

    // This puts the car at the start of the track
    PathDriverInit(&game->path_driver, &game->track, &game->car);
    if (game->mode == GAME_MODE_DEBUG_SELFDRIVE)
    {
        game->driver = &game->path_driver.base;
        for (float time = 0.0f; time < options->skip_seconds; time += SIMULATION_STEP)
        {
            CarDriverUpdate(game->driver, &game->car, SIMULATION_STEP);
        }
    }
    else if (game->mode == GAME_MODE_NORMAL)
    {
        game->driver = &game->player_driver.base;
        PlayerDriverInit(&game->player_driver, &game->surface, &game->track, &game->car);
        StageStart(&game->stage, &game->track, game->level.tutorial); // only the first drive has the intro
    }

    float pitch_min;
    float pitch_max;
    CockpitPitchLimits(&game->cockpit, &pitch_min, &pitch_max);
    HeadCameraInit(&game->head, options->yaw_deg, fmaxf(pitch_min, HEAD_MIN_PITCH_DEG),
                   fminf(pitch_max, HEAD_MAX_PITCH_DEG));
    if (options->has_pitch)
    {
        game->head.pitch = Clamp(options->pitch_deg, game->head.pitch_min, game->head.pitch_max);
    }
    PaceMapReset(&game->map, &game->track, &game->car);
    MapEventsInit(&game->map_events, &game->level, game->difficulty);
    SpeechInit(&game->speech);
    RainInit(&game->rain, (Mode7Camera){game->car.position, game->car.heading, game->head.pitch, game->car.heading});
    HintsInit(&game->hints, game->level.tutorial && game->mode == GAME_MODE_NORMAL, DIFFICULTY_RULES[game->difficulty].map);
    CarAudioInit(&game->audio, &game->car);
    CarAudioSetPlaying(&game->audio, game->mode != GAME_MODE_DEBUG_FLY);

    game->fly.position = game->car.position;
    game->fly.heading = game->car.heading + options->yaw_deg * DEG2RAD;
    return true;
}

void GameShutdown(Game *game)
{
    SpeechUnload(&game->speech);
    CarAudioUnload(&game->audio);
    PaceMapUnload(&game->map);
    HintsUnload(&game->hints);
    CockpitUnload(&game->cockpit);
    TreesUnload(&game->trees);
    Mode7Unload(&game->mode7);
    SurfaceMapUnload(&game->surface);
    LevelTexturesUnload(&game->textures);
    ForestUnload(&game->forest);
    TrackUnload(&game->track);
}

static void UpdateHints(Game *game, float dt)
{
    float distance;
    int next = PaceNextCorner(&game->notes, &game->track, game->map.position, &distance);
    const PaceCorner *corner = next >= 0 ? &game->notes.corners[next] : NULL;
    HintInput input = {
        .car = &game->car,
        .stage = &game->stage,
        .head_yaw = game->head.yaw,
        .map_visible = game->map.visible,
        .corner_distance = corner != NULL ? distance : INFINITY,
        .corner_grade = corner != NULL ? corner->grade : PACE_GRADE_STRAIGHT,
        .corner_hairpin = corner != NULL && corner->hairpin,
        .on_grass = SurfaceAt(&game->surface, game->car.position) == SURFACE_GRASS,
        .braking = IsKeyDown(KEY_S),
        .handbrake = IsKeyDown(KEY_SPACE),
        .throttle = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP),
    };
    HintsUpdate(&game->hints, &input, dt);
    if (!game->hints.enabled || HintsIntroDone(&game->hints))
    {
        StageEndIntro(&game->stage);
    }
}

void GameUpdate(Game *game, float dt)
{
    // The first movement after the cursor is hidden can be a large jump
    bool use_mouse = game->mouse_ready && !game->mouse_ignore;
    Vector2 mouse = use_mouse ? GetMouseDelta() : (Vector2){0.0f, 0.0f};
    mouse.x *= game->mouse_sensitivity;
    mouse.y *= game->mouse_sensitivity * (game->invert_look ? -1.0f : 1.0f);
    game->mouse_ready = true;

    if (game->mode == GAME_MODE_DEBUG_FLY)
    {
        float forward = (float)(IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
        float right = (float)(IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
        FlyCameraUpdate(&game->fly, forward, right, mouse.x, dt, (float)game->level.map_width,
                        (float)game->level.map_height);
        return;
    }

    game->fade_in = fmaxf(0.0f, game->fade_in - dt);
    bool crashed = game->crash_time >= 0.0f;
    if (crashed)
    {
        game->crash_time += dt;
        if (GameCrashOver(game) && !game->crash_menu)
        {
            RestartStage(game, true);
        }
    }
    else if (game->mode == GAME_MODE_NORMAL)
    {
        if (IsKeyPressed(KEY_R))
        {
            RestartStage(game, false);
        }
        game->player_driver.hold = game->stage.phase == STAGE_COUNTDOWN || game->stage.phase == STAGE_INTRO;
    }
    if (game->driver != NULL && !crashed)
    {
        CarDriverUpdate(game->driver, &game->car, dt);
    }
    if (game->mode == GAME_MODE_NORMAL && !crashed)
    {
        StageUpdate(&game->stage, &game->car, dt);
        Vector2 forward = {cosf(game->car.heading), sinf(game->car.heading)};
        Vector2 right = {-forward.y, forward.x};
        Vector2 center = Vector2Add(game->car.position, Vector2Add(Vector2Scale(forward, CAR_EYE_BACK),
                                                                   Vector2Scale(right, CAR_EYE_LEFT)));
        bool in_water = SurfaceAt(&game->surface, center) == SURFACE_WATER;
        if (in_water || ForestHit(&game->forest, center, game->car.heading, CAR_LENGTH, CAR_WIDTH))
        {
            game->crash_time = 0.0f;
            game->car.speed = 0.0f;
            game->car.slip = 0.0f;
            CarAudioCrash(&game->audio);
            if (in_water)
            {
                SpeechClear(&game->speech);
                SpeechSay(&game->speech, (SpeechLine){"luka", "scared", "Grandma! The lake!", ""});
            }
        }
    }
    PaceMapUpdate(&game->map, &game->track, &game->car, dt);
    if (game->mode == GAME_MODE_NORMAL && !crashed)
    {
        MapEvents *events = &game->map_events;
        MapEventsUpdate(events, &game->stage, game->car.speed, game->car.heading, dt);
        game->map.pose = events->pose;
        // When the map changes, the reaction comes at once, the warning line of Luka is cut off
        if (events->cue == MAP_CUE_START)
        {
            SpeechClear(&game->speech);
        }
        SpeechLine line;
        for (int i = 0; i < 2; i++)
        {
            if (MapEventsLine(events, i, &line))
            {
                SpeechSay(&game->speech, line);
            }
        }
    }
    if (game->mode == GAME_MODE_NORMAL)
    {
        SpeechUpdate(&game->speech, dt);
    }
    if (!crashed)
    {
        UpdateHints(game, dt);
    }
    CarAudioUpdate(&game->audio, &game->car, SurfaceAt(&game->surface, game->car.position));
    HeadCameraUpdate(&game->head, mouse.x, mouse.y);
}

void GameRestart(Game *game)
{
    RestartStage(game, true);
}

void GameResume(Game *game)
{
    game->mouse_ready = false;
}

bool GameCrashOver(const Game *game)
{
    return game->crash_time >= CRASH_FADE_SECONDS + CRASH_BLACK_SECONDS;
}

void GameRender(Game *game)
{
    bool fly = game->mode == GAME_MODE_DEBUG_FLY;

    // The road view turns with the head, in the same way as the cabin
    Mode7Camera camera = {game->fly.position, game->fly.heading, VIEW_PITCH_DEG, game->fly.heading};
    if (!fly)
    {
        camera = (Mode7Camera){game->car.position, game->car.heading + game->head.yaw * DEG2RAD,
                               game->head.pitch, game->car.heading};
    }
    Mode7Render(&game->mode7, game->textures.visual, camera, game->light);
    CockpitRenderGlass(&game->cockpit, game->map_events.vomit, game->map_events.vomit_count);
    TreesRender(&game->trees, &game->forest, game->mode7.target, camera, game->light);
    // The rain also falls behind the menus, so it moves with the frame time and not with the game
    if (game->light->rain)
    {
        Vector2 velocity = {cosf(game->car.heading) * game->car.speed, sinf(game->car.heading) * game->car.speed};
        RainUpdate(&game->rain, GetFrameTime(), camera, velocity);
        RainDraw(&game->rain, game->mode7.target, camera, velocity, game->light->fog, game->light->fog_end);
    }
    game->map.light = game->light->map;
    // On a difficulty without a map, Luka has no map and nothing is drawn
    if (!fly && DIFFICULTY_RULES[game->difficulty].map)
    {
        PaceMapRender(&game->map, &game->track, &game->notes, &game->car, game->head.yaw,
                      game->head.pitch);
    }
    else
    {
        game->map.visible = false;
    }
}

void GameDrawView(const Game *game, Rectangle destination)
{
    bool fly = game->mode == GAME_MODE_DEBUG_FLY;
    Mode7Draw(&game->mode7, destination);
    if (!fly)
    {
        CockpitDraw(&game->cockpit, destination, game->head.yaw, game->head.pitch,
                    game->car.steer, game->light->cabin);
        PaceMapDraw(&game->map, destination);
        if (game->show_hud)
        {
            HudDraw(&game->car, game->mode == GAME_MODE_NORMAL ? &game->stage : NULL, game->level.display_name,
                    destination);
        }
    }
    float black = game->fade_in / RESTART_FADE_SECONDS;
    if (game->crash_time >= 0.0f)
    {
        black = fminf(game->crash_time / CRASH_FADE_SECONDS, 1.0f);
    }
    if (black > 0.0f)
    {
        DrawRectangleRec(destination, Fade(BLACK, black));
    }
}
