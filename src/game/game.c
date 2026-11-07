#include "game/game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/config.h"
#include "raymath.h"
#include "render/present.h"
#include "ui/hud.h"

#define SIMULATION_STEP (1.0f / 60.0f)

bool GameInit(Game *game, const Options *options, char *message, size_t size)
{
    memset(game, 0, sizeof(*game));
    game->mode = options->mode;
    game->mouse_ignore = options->screenshot != NULL;

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
    if (!TrackBuild(&game->track, &game->level))
    {
        snprintf(message, size, "Could not build the track for \"%s\"", name);
        return false;
    }

    if (!ForestBuild(&game->forest, &game->level, &game->track))
    {
        snprintf(message, size, "Could not place the trees for \"%s\"", name);
        GameShutdown(game);
        return false;
    }

    bool loaded = LevelTexturesLoad(&game->textures, &game->level, message, size) &&
                  SurfaceMapLoad(&game->surface, &game->level, message, size) &&
                  Mode7Init(&game->mode7, message, size) &&
                  TreesInit(&game->trees, &game->level, message, size) &&
                  CockpitLoad(&game->cockpit, message, size);
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
        StageStart(&game->stage, &game->track);
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
    CarAudioInit(&game->audio, &game->car);
    CarAudioSetPlaying(&game->audio, game->mode != GAME_MODE_DEBUG_FLY);

    game->fly.position = game->car.position;
    game->fly.heading = game->car.heading + options->yaw_deg * DEG2RAD;
    return true;
}

void GameShutdown(Game *game)
{
    CarAudioUnload(&game->audio);
    CockpitUnload(&game->cockpit);
    TreesUnload(&game->trees);
    Mode7Unload(&game->mode7);
    SurfaceMapUnload(&game->surface);
    LevelTexturesUnload(&game->textures);
    ForestUnload(&game->forest);
    TrackUnload(&game->track);
}

void GameUpdate(Game *game, float dt)
{
    if (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_F11))
    {
        ToggleBorderlessWindowed();
    }

    // The first movement after the cursor is hidden can be a large jump
    bool use_mouse = game->mouse_ready && !game->mouse_ignore;
    Vector2 mouse = use_mouse ? GetMouseDelta() : (Vector2){0.0f, 0.0f};
    game->mouse_ready = true;

    if (game->mode == GAME_MODE_DEBUG_FLY)
    {
        float forward = (float)(IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
        float right = (float)(IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
        FlyCameraUpdate(&game->fly, forward, right, mouse.x, dt, (float)game->level.map_width,
                        (float)game->level.map_height);
        return;
    }

    if (game->mode == GAME_MODE_NORMAL)
    {
        if (IsKeyPressed(KEY_R))
        {
            PlayerDriverInit(&game->player_driver, &game->surface, &game->track, &game->car);
            StageStart(&game->stage, &game->track);
        }
        game->player_driver.hold = game->stage.phase == STAGE_COUNTDOWN;
    }
    if (game->driver != NULL)
    {
        CarDriverUpdate(game->driver, &game->car, dt);
    }
    if (game->mode == GAME_MODE_NORMAL)
    {
        StageUpdate(&game->stage, &game->car, dt);
    }
    CarAudioUpdate(&game->audio, &game->car, SurfaceAt(&game->surface, game->car.position));
    HeadCameraUpdate(&game->head, mouse.x, mouse.y);
}

void GameDraw(Game *game)
{
    bool fly = game->mode == GAME_MODE_DEBUG_FLY;

    // The road view turns with the head, in the same way as the cabin
    Mode7Camera camera = {game->fly.position, game->fly.heading, VIEW_PITCH_DEG};
    if (!fly)
    {
        camera = (Mode7Camera){game->car.position, game->car.heading + game->head.yaw * DEG2RAD,
                               game->head.pitch};
    }
    Mode7Render(&game->mode7, game->textures.visual, camera);
    TreesRender(&game->trees, &game->forest, game->mode7.target, camera);

    Rectangle destination = PresentDestination(GetScreenWidth(), GetScreenHeight());
    BeginDrawing();
    ClearBackground(BLACK);
    Mode7Draw(&game->mode7, destination);
    if (!fly)
    {
        CockpitDraw(&game->cockpit, destination, game->head.yaw, game->head.pitch,
                    game->car.steer);
        HudDraw(&game->car, game->mode == GAME_MODE_NORMAL ? &game->stage : NULL, destination);
    }
    EndDrawing();
}
