#ifndef APP_APP_H
#define APP_APP_H

#include <stdbool.h>
#include <stddef.h>

#include "core/options.h"
#include "core/settings.h"
#include "game/game.h"
#include "story/scene_player.h"
#include "story/story.h"
#include "ui/ui.h"

#define APP_MAX_LEVELS 32

typedef enum Screen
{
    SCREEN_MENU,
    SCREEN_STAGES, // the list of levels for free play
    SCREEN_SETTINGS,
    SCREEN_PLAYING,
    SCREEN_PAUSED,
    SCREEN_CRASHED,
    SCREEN_FINISHED,
    SCREEN_SCENE // a scene of the story mode
} Screen;

// The whole program: the menus and the game behind them
// Without a level name and without a debug mode on the command line, the program starts in the main menu
// The main menu shows the first level behind it
typedef struct App
{
    Options options;
    Settings settings;
    Ui ui;
    Game game;
    bool game_ready;

    Screen screen;
    Screen settings_back; // the screen that the settings go back to
    bool screen_changed;  // true in the frame when the screen changed, so one key press does not act twice
    bool cursor_hidden;
    float menu_time;
    bool new_best;
    bool quit;

    Story story;
    bool has_story;  // false when the story file cannot be read, then the story mode starts the first level
    bool in_story;   // true while the player plays the story mode
    ScenePlayer scene;
    bool scene_playing;

    char levels[APP_MAX_LEVELS][LEVEL_MAX_NAME];
    int level_count;
    char message[256]; // an error that the menu shows
} App;

// The window must be open, write the reason in message when it fails
bool AppInit(App *app, const Options *options, char *message, size_t size);
void AppShutdown(App *app);

// Update and draw one frame
void AppFrame(App *app);

bool AppShouldQuit(const App *app);

#endif
