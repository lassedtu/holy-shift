#ifndef APP_APP_H
#define APP_APP_H

#include <stdbool.h>
#include <stddef.h>

#include "audio/music.h"
#include "core/options.h"
#include "core/settings.h"
#include "game/game.h"
#include "game/standings.h"
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
    SCREEN_SCENE,     // a scene of the story mode
    SCREEN_STORY,     // continue the saved story or start a new story
    SCREEN_DIFFICULTY // choose the difficulty for a new story or after a crash
} Screen;

// What happens after the player chooses a difficulty
typedef enum DifficultyChoice
{
    CHOICE_NEW_STORY, // a new story starts, the saved story is lost
    CHOICE_RETRY      // the stage starts again on the new difficulty
} DifficultyChoice;

// The whole program: the menus and the game behind them
// Without a level name and without a debug mode on the command line, the program starts in the main menu
// The main menu shows the first level behind it
typedef struct App
{
    Options options;
    Settings settings;
    Ui ui;
    MusicPlayer menu_music;  // plays on the screens of the main menu
    MusicPlayer drive_music; // plays while the player drives a stage
    MusicPlayer scene_music; // plays when a scene asks for it
    int voice_index;         // the line of the scene that the voice says, -1 for none
    int voice_typed;         // the letters of that line that the voice has said
    Game game;
    bool game_ready;

    Screen screen;
    Screen settings_back; // the screen that the settings go back to
    Screen difficulty_back; // the screen that the choice of the difficulty goes back to
    DifficultyChoice difficulty_choice;
    bool screen_changed;  // true in the frame when the screen changed, so one key press does not act twice
    bool cursor_hidden;
    float menu_time;
    bool new_best;
    Standings standings; // the result of the last stage
    bool final_stage;    // the stage was the last stage of the story, only a win goes on
    bool qualified;      // the place is good enough to go on in the story
    bool quit;

    Story story;
    bool has_story;  // false when the story file cannot be read, then the story mode starts the first level
    bool in_story;   // true while the player plays the story mode
    ScenePlayer scene;
    bool scene_playing;

    char levels[APP_MAX_LEVELS][LEVEL_MAX_NAME];        // the folder names
    char level_titles[APP_MAX_LEVELS][LEVEL_MAX_NAME];  // the display names
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
