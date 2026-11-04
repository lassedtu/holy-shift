#include "game/game.h"

#include <stdio.h>
#include <string.h>

#include "core/config.h"
#include "render/present.h"
#include "ui/hud.h"

#define SIMULATION_STEP (1.0f / 60.0f)

bool GameInit(Game *game, const Options *options, char *message, size_t size)
{
    memset(game, 0, sizeof(*game));
    game->mode = options->mode;

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

    bool loaded = LevelTexturesLoad(&game->textures, &game->level, message, size) &&
                  Mode7Init(&game->mode7, message, size) &&
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
    else
    {
        // Nobody drives the car in the other modes, it waits at the start
        game->car.speed = 0.0f;
    }

    game->head.yaw = options->yaw_deg;
    game->fly.position = game->car.position;
    game->fly.heading = game->car.heading + options->yaw_deg * DEG2RAD;
    return true;
}

void GameShutdown(Game *game)
{
    CockpitUnload(&game->cockpit);
    Mode7Unload(&game->mode7);
    LevelTexturesUnload(&game->textures);
    TrackUnload(&game->track);
}

void GameUpdate(Game *game, float dt)
{
    if (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_F11))
    {
        ToggleBorderlessWindowed();
    }

    // The first movement after the cursor is hidden can be a large jump
    float mouse_dx = game->mouse_ready ? GetMouseDelta().x : 0.0f;
    game->mouse_ready = true;

    if (game->mode == GAME_MODE_DEBUG_FLY)
    {
        float forward = (float)(IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
        float right = (float)(IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
        FlyCameraUpdate(&game->fly, forward, right, mouse_dx, dt, (float)game->level.map_width,
                        (float)game->level.map_height);
        return;
    }

    if (game->driver != NULL)
    {
        CarDriverUpdate(game->driver, &game->car, dt);
    }
    HeadCameraUpdate(&game->head, mouse_dx);
}

void GameDraw(Game *game)
{
    bool fly = game->mode == GAME_MODE_DEBUG_FLY;

    // The road view turns with the head, in the same way as the cabin
    Vector2 position = fly ? game->fly.position : game->car.position;
    float heading = fly ? game->fly.heading : game->car.heading + game->head.yaw * DEG2RAD;
    Mode7Render(&game->mode7, game->textures.visual, position, heading);

    Rectangle destination = PresentDestination(GetScreenWidth(), GetScreenHeight());
    BeginDrawing();
    ClearBackground(BLACK);
    Mode7Draw(&game->mode7, destination);
    if (!fly)
    {
        CockpitDraw(&game->cockpit, destination, game->head.yaw, game->car.steer);
        HudDraw(&game->car, destination);
    }
    EndDrawing();
}
