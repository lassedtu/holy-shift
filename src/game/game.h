#ifndef GAME_GAME_H
#define GAME_GAME_H

#include <stdbool.h>
#include <stddef.h>

#include "audio/car_audio.h"
#include "camera/fly_camera.h"
#include "camera/head_camera.h"
#include "car/car.h"
#include "car/car_driver.h"
#include "car/path_driver.h"
#include "car/player_driver.h"
#include "core/game_mode.h"
#include "core/options.h"
#include "game/stage.h"
#include "level/forest.h"
#include "level/level.h"
#include "level/level_textures.h"
#include "level/surface.h"
#include "level/track.h"
#include "render/cockpit.h"
#include "render/mode7.h"
#include "render/trees.h"

// Everything of the running game
// The car is moved by a driver, the other parts only read the state of the car
// In the normal mode the player drives a stage, R starts the stage again
// When the car hits a tree, the view fades to black and the stage starts again
// In the fly mode a free camera is used and the car is not shown
typedef struct Game
{
    GameMode mode;
    Level level;
    Track track;
    Forest forest;
    LevelTextures textures;
    SurfaceMap surface;
    Stage stage;

    Car car;
    CarDriver *driver; // who moves the car, NULL when nobody moves it
    PathDriver path_driver;
    PlayerDriver player_driver;

    HeadCamera head;
    FlyCamera fly;
    bool mouse_ready;  // false until the first frame, the first mouse movement is not valid
    bool mouse_ignore; // true when the mouse must not move the view, for a picture of --shot
    float crash_time;  // seconds since the car hit a tree, negative when there is no crash
    float fade_in;     // seconds left of the fade in after the stage starts again

    Mode7 mode7;
    Trees trees;
    Cockpit cockpit;
    CarAudio audio;
} Game;

// Load the level and the images, write the reason in message when it fails
// The window must be open, the game has sound when the audio device is ready
bool GameInit(Game *game, const Options *options, char *message, size_t size);
void GameShutdown(Game *game);

void GameUpdate(Game *game, float dt);
void GameDraw(Game *game);

#endif
