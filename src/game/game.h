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
#include "game/hints.h"
#include "game/map_events.h"
#include "game/stage.h"
#include "level/forest.h"
#include "level/level.h"
#include "level/level_textures.h"
#include "level/pace_notes.h"
#include "level/surface.h"
#include "level/track.h"
#include "render/cockpit.h"
#include "render/mode7.h"
#include "render/rain.h"
#include "ui/speech.h"
#include "render/pace_map.h"
#include "render/trees.h"

// Everything of the running game
// The car is moved by a driver, the other parts only read the state of the car
// In the normal mode the player drives a stage, R starts the stage again
// When the car hits a tree, the view fades to black, then the stage starts again or a menu opens (crash_menu)
// In the fly mode a free camera is used and the car is not shown
typedef struct Game
{
    GameMode mode;
    Difficulty difficulty;
    const Lighting *light; // the time of day of the stage
    Level level;
    Track track;
    Forest forest;
    LevelTextures textures;
    SurfaceMap surface;
    Stage stage;
    PaceNotes notes;

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
    bool crash_menu;   // true when a menu decides what happens after a crash, false to start again
    bool show_hud;
    float mouse_sensitivity; // 1 is MOUSE_SENSITIVITY_DEG
    bool invert_look;

    Mode7 mode7;
    Trees trees;
    Rain rain;
    Cockpit cockpit;
    PaceMap map;
    MapEvents map_events;
    Speech speech; // what Rajka and Luka say while driving
    Hints hints; // the hints of a tutorial level
    CarAudio audio;
} Game;

// Load the level and the images, write the reason in message when it fails
// The window must be open, the game has sound when the audio device is ready
bool GameInit(Game *game, const Options *options, char *message, size_t size);
void GameShutdown(Game *game);

void GameUpdate(Game *game, float dt);

// Draw the views of the game into their textures, before BeginDrawing
void GameRender(Game *game);

// Draw the rendered game into an area of the window, between BeginDrawing and EndDrawing
void GameDrawView(const Game *game, Rectangle destination);

// Start the stage again with a fade in
void GameRestart(Game *game);

// Call this when the game continues after a menu, so the first mouse movement does not turn the head
void GameResume(Game *game);

// Return true when the view is black after a crash
bool GameCrashOver(const Game *game);

#endif
