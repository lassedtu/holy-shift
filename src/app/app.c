#include "app/app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "audio/voice.h"
#include "core/config.h"
#include "level/track.h"
#include "render/present.h"
#include "ui/hud.h"

#define MENU_LOOK_DEG 25.0f   // the head turns this far to each side behind the main menu
#define MENU_LOOK_SPEED 0.15f // turns of the head each second, in radians of the sine
#define BUTTON_WIDTH 180.0f
#define BUTTON_HEIGHT 32.0f
#define BUTTON_STEP 38.0f
#define PANEL_MARGIN 12.0f
#define TITLE_SIZE 60
#define HEADING_SIZE 30
#define LABEL_SIZE 15
#define COLOR_TITLE ((Color){255, 204, 0, 255})
#define COLOR_SHADOW ((Color){40, 24, 12, 255})
#define BOARD_DEFAULT_SPEED 13.0f // meters each second of the player on a board, when there is no time
#define COLOR_INK ((Color){60, 40, 24, 255})
#define COLOR_WARNING ((Color){230, 90, 40, 255})
#define COLOR_HIGHLIGHT ((Color){244, 210, 110, 255}) // the row of the player in the standings
#define DIFFICULTY_WIDTH 260.0f
#define DIFFICULTY_STEP 54.0f // the difficulty buttons are higher than the other buttons, for two lines
#define DEV_FINISH_TIME 50.0f // the time on the results of --screen finished
#define VOICE_MAX_JUMP 3 // more new letters than this in one frame is a line shown at once, it is not said
#define QUALIFY_PLACE 3 // the story goes on from this place or better (the final needs the first place)

static void SetScreen(App *app, Screen screen)
{
    app->screen = screen;
    app->screen_changed = true;
    UiReset(&app->ui);
}

// The difficulty of the command line, or the difficulty of the settings
static Difficulty CurrentDifficulty(const App *app)
{
    return app->options.has_difficulty ? app->options.difficulty : app->settings.difficulty;
}

// Write the path of the music that the scene asks for now, "" when it asks for none
static void SceneMusicPath(const App *app, char *path, size_t size)
{
    const char *music = app->screen == SCREEN_SCENE && app->scene_playing ? app->scene.music : "";
    path[0] = '\0';
    if (music[0] != '\0')
    {
        snprintf(path, size, "%s/%s", MUSIC_DIR, music);
    }
}

// Play the music that the scene asks for. A new track starts after the old track has faded out
// The menu track is played by the menu player, so it goes on from the menu into a scene without a break
static void UpdateSceneMusic(App *app, float dt)
{
    char path[256];
    SceneMusicPath(app, path, sizeof(path));
    if (strcmp(path, app->menu_music.path) == 0)
    {
        path[0] = '\0';
    }
    bool same = strcmp(path, app->scene_music.path) == 0;
    if (!same && app->scene_music.volume <= 0.0f)
    {
        MusicPlayerUnload(&app->scene_music);
        MusicPlayerLoad(&app->scene_music, path, MUSIC_SCENE_VOLUME);
        same = true;
    }
    MusicPlayerUpdate(&app->scene_music, same && path[0] != '\0', app->settings.music_volume, dt);
}

// Load the music of the level of the game, when it is not the track that is loaded
static void LoadDriveMusic(App *app)
{
    const char *music = app->game.level.music;
    char path[256] = "";
    if (music[0] != '\0')
    {
        snprintf(path, sizeof(path), "%s/%s", MUSIC_DIR, music);
    }
    if (strcmp(path, app->drive_music.path) != 0)
    {
        MusicPlayerUnload(&app->drive_music);
        MusicPlayerLoad(&app->drive_music, path, MUSIC_DRIVE_VOLUME);
    }
}

static bool LoadStage(App *app, const char *name)
{
    Difficulty difficulty = CurrentDifficulty(app);
    if (app->game_ready && strcmp(app->game.level.name, name) == 0 && app->game.difficulty == difficulty)
    {
        GameRestart(&app->game);
        return true;
    }
    if (app->game_ready)
    {
        GameShutdown(&app->game);
        app->game_ready = false;
    }
    Options options = app->options;
    options.level = name;
    options.mode = GAME_MODE_NORMAL;
    options.difficulty = difficulty;
    app->game_ready = GameInit(&app->game, &options, app->message, sizeof(app->message));
    if (app->game_ready)
    {
        app->game.crash_menu = true;
        LoadDriveMusic(app);
    }
    return app->game_ready;
}

static void StartStage(App *app, const char *name)
{
    if (LoadStage(app, name))
    {
        GameResume(&app->game);
        SetScreen(app, SCREEN_PLAYING);
    }
}

static void OpenMenu(App *app)
{
    GameRestart(&app->game);
    CarAudioSetPlaying(&app->game.audio, false);
    app->game.fade_in = 0.0f; // the game is not updated behind the menu, so the fade would not end
    app->menu_time = 0.0f;
    app->in_story = false;
    SetScreen(app, SCREEN_MENU);
}

static bool Back(const App *app)
{
    return app->ui.back && !app->screen_changed;
}

static void StopScene(App *app)
{
    VoiceStop();
    app->voice_index = -1;
    if (app->scene_playing)
    {
        ScenePlayerStop(&app->scene);
        app->scene_playing = false;
    }
}

// Go to the step of the story mode in the settings, the menu opens after the last step
// Give the board of the scene the standings of its level, with the last time of the player in the story
// Without a time (for example with --screen scene:<name>), the best time or a time from the length is used
static void PrepareBoard(App *app)
{
    const Scene *scene = &app->scene.scene;
    for (int i = 0; i < scene->count; i++)
    {
        if (scene->commands[i].type != SCENE_BOARD)
        {
            continue;
        }
        static Level level;
        Track track;
        char message[256];
        const char *name = scene->commands[i].name;
        if (!LevelLoad(&level, name, message, sizeof(message)) || !TrackBuild(&track, &level))
        {
            fprintf(stderr, "The board of the scene has no level \"%s\"\n", name);
            return;
        }
        float time = app->settings.story_last_time;
        if (time <= 0.0f)
        {
            time = SettingsBestTime(&app->settings, name, CurrentDifficulty(app));
        }
        if (time <= 0.0f)
        {
            time = track.length / MAP_PIXELS_PER_METER / BOARD_DEFAULT_SPEED;
        }
        Standings standings;
        StandingsMake(&standings, &level, track.length, time, CurrentDifficulty(app));
        ScenePlayerSetBoard(&app->scene, &standings, level.display_name);
        TrackUnload(&track);
        return;
    }
}

static void ContinueStory(App *app)
{
    StopScene(app);
    if (!app->has_story)
    {
        app->in_story = true;
        StartStage(app, app->levels[0]);
        return;
    }
    while (app->settings.story_step < app->story.count)
    {
        const StoryStep *step = &app->story.steps[app->settings.story_step];
        if (step->type == STORY_SCENE)
        {
            app->scene_playing = ScenePlayerStart(&app->scene, step->name, app->message, sizeof(app->message));
            if (app->scene_playing)
            {
                PrepareBoard(app);
                app->in_story = true;
                SetScreen(app, SCREEN_SCENE);
                return;
            }
            fprintf(stderr, "%s\n", app->message);
        }
        else
        {
            app->in_story = true;
            StartStage(app, step->name);
            if (app->screen == SCREEN_PLAYING)
            {
                return;
            }
            fprintf(stderr, "%s\n", app->message);
        }
        app->settings.story_step++; // a step that cannot start is left out
    }
    app->settings.story_step = 0; // the story is complete, the next time it starts from the beginning
    SettingsSave(&app->settings);
    OpenMenu(app);
}

// Go to the next step of the story mode
static void NextStoryStep(App *app)
{
    app->settings.story_step++;
    SettingsSave(&app->settings);
    ContinueStory(app);
}

// Say the letters that the dialogue box typed in this frame
// A line that is shown at once (a click) is not said, a line without a speaker has the neutral voice of the narrator
static void SpeakScene(App *app)
{
    const ScenePlayer *player = &app->scene;
    const SceneCommand *command = NULL;
    if (app->scene_playing && !player->done && player->index < player->scene.count && player->started)
    {
        command = &player->scene.commands[player->index];
    }
    if (command == NULL || command->type != SCENE_LINE)
    {
        VoiceStop();
        app->voice_index = -1;
        return;
    }
    if (app->voice_index != player->index)
    {
        app->voice_index = player->index;
        app->voice_typed = 0;
    }
    int typed = (int)player->typed;
    int length = (int)strlen(command->text);
    if (typed - app->voice_typed > VOICE_MAX_JUMP)
    {
        VoiceStop();
        app->voice_typed = typed;
        return;
    }
    size_t speaker = strlen(command->fallback);
    const char *mood = strncmp(command->name, command->fallback, speaker) == 0 && command->name[speaker] == '_'
                           ? command->name + speaker + 1
                           : "";
    VoiceStyle style = VoiceStyleFor(command->fallback, mood);
    bool question = length > 0 && command->text[length - 1] == '?';
    for (int i = app->voice_typed; i < typed; i++)
    {
        VoiceSay(&style, command->text[i], (float)i / (float)length, question);
    }
    app->voice_typed = typed;
}

static void SceneScreen(App *app)
{
    bool advance = !app->screen_changed && (app->ui.select || IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    ScenePlayerUpdate(&app->scene, GetFrameTime(), advance, Back(app));
    SpeakScene(app);
    ScenePlayerDraw(&app->scene, &app->ui);
    if (ScenePlayerDone(&app->scene))
    {
        NextStoryStep(app);
    }
}

// Open the screen of the --screen option, for development
static void OpenStartScreen(App *app, const char *name)
{
    const char *names[] = {"stages", "settings", "paused", "crashed", "finished", "story", "difficulty"};
    Screen screens[] = {SCREEN_STAGES,   SCREEN_SETTINGS, SCREEN_PAUSED,    SCREEN_CRASHED,
                        SCREEN_FINISHED, SCREEN_STORY,    SCREEN_DIFFICULTY};
    if (name != NULL && strncmp(name, "scene:", 6) == 0)
    {
        app->scene_playing = ScenePlayerStart(&app->scene, name + 6, app->message, sizeof(app->message));
        if (app->scene_playing)
        {
            PrepareBoard(app);
            SetScreen(app, SCREEN_SCENE);
        }
        return;
    }
    for (int i = 0; name != NULL && i < 7; i++)
    {
        if (strcmp(name, names[i]) == 0)
        {
            SetScreen(app, screens[i]);
            app->settings_back = SCREEN_MENU;
            app->difficulty_back = SCREEN_MENU;
            app->game.crash_time = screens[i] == SCREEN_CRASHED ? 10.0f : -1.0f;
            if (screens[i] == SCREEN_FINISHED)
            {
                app->game.stage.time = DEV_FINISH_TIME;
                StandingsMake(&app->standings, &app->game.level, app->game.track.length, DEV_FINISH_TIME,
                              app->game.difficulty);
                app->qualified = true;
            }
        }
    }
}

bool AppInit(App *app, const Options *options, char *message, size_t size)
{
    memset(app, 0, sizeof(*app));
    app->options = *options;
    SettingsLoad(&app->settings);
    if (options->screenshot == NULL)
    {
        SettingsApply(&app->settings);
    }
    if (!UiLoad(&app->ui, message, size))
    {
        return false;
    }
    MusicPlayerLoad(&app->menu_music, MUSIC_MENU_PATH, MUSIC_MENU_VOLUME);
    VoiceInit();
    app->voice_index = -1;
    app->level_count = LevelFindAll(app->levels, APP_MAX_LEVELS);
    for (int i = 0; i < app->level_count; i++)
    {
        LevelReadDisplayName(app->levels[i], app->level_titles[i], LEVEL_MAX_NAME);
    }
    char story_message[256];
    app->has_story = StoryLoad(&app->story, story_message, sizeof(story_message));
    if (!app->has_story)
    {
        fprintf(stderr, "%s\n", story_message);
    }

    bool direct = options->level != NULL || options->mode != GAME_MODE_NORMAL;
    if (direct)
    {
        app->game_ready = GameInit(&app->game, options, message, size);
        if (!app->game_ready)
        {
            UiUnload(&app->ui);
            return false;
        }
        app->game.crash_menu = options->mode == GAME_MODE_NORMAL;
        app->screen = SCREEN_PLAYING;
        LoadDriveMusic(app);
        return true;
    }

    if (app->level_count == 0)
    {
        snprintf(message, size, "No saved levels in %s/", LEVELS_DIR);
        UiUnload(&app->ui);
        return false;
    }
    if (!LoadStage(app, app->levels[0]))
    {
        snprintf(message, size, "%s", app->message);
        UiUnload(&app->ui);
        return false;
    }
    OpenMenu(app);
    OpenStartScreen(app, options->screen);
    return true;
}

void AppShutdown(App *app)
{
    StopScene(app);
    MusicPlayerUnload(&app->menu_music);
    MusicPlayerUnload(&app->drive_music);
    MusicPlayerUnload(&app->scene_music);
    VoiceUnload();
    if (app->game_ready)
    {
        GameShutdown(&app->game);
    }
    UiUnload(&app->ui);
}

bool AppShouldQuit(const App *app)
{
    return app->quit;
}

static void UpdateCursor(App *app)
{
    bool hide = app->screen == SCREEN_PLAYING;
    if (hide != app->cursor_hidden && app->options.screenshot == NULL)
    {
        if (hide)
        {
            DisableCursor();
        }
        else
        {
            EnableCursor();
        }
    }
    app->cursor_hidden = hide;
}

// Return true when the story step is the last stage of the story, the final
static bool IsFinalStep(const App *app)
{
    int last = -1;
    for (int i = 0; i < app->story.count; i++)
    {
        if (app->story.steps[i].type == STORY_STAGE)
        {
            last = i;
        }
    }
    return app->has_story && app->settings.story_step == last;
}

static void UpdatePlaying(App *app, float dt)
{
    Game *game = &app->game;
    if (IsKeyPressed(KEY_ESCAPE))
    {
        CarAudioSetPlaying(&game->audio, false);
        SetScreen(app, SCREEN_PAUSED);
        return;
    }
    GameUpdate(game, dt);
    if (game->mode != GAME_MODE_NORMAL)
    {
        return;
    }
    if (GameCrashOver(game))
    {
        SetScreen(app, SCREEN_CRASHED);
    }
    else if (game->stage.phase == STAGE_FINISHED)
    {
        CarAudioSetPlaying(&game->audio, false);
        float time = game->stage.time;
        StandingsMake(&app->standings, &game->level, game->track.length, time, game->difficulty);
        int place = app->standings.place;
        app->new_best = SettingsRecordTime(&app->settings, game->level.name, game->difficulty, time);
        // A tutorial has no standings, the player always goes on
        bool tutorial = game->level.tutorial;
        if (!tutorial)
        {
            SettingsRecordPlace(&app->settings, game->level.name, game->difficulty, place);
        }
        app->final_stage = app->in_story && IsFinalStep(app);
        app->qualified = tutorial || !app->in_story || (app->final_stage ? place == 1 : place <= QUALIFY_PLACE);
        if (app->in_story)
        {
            app->settings.story_last_time = time;
        }
        SettingsSave(&app->settings);
        SetScreen(app, SCREEN_FINISHED);
    }
}

// Return the rectangle of button number i in a column that starts at top
static Rectangle ButtonAt(float top, int i, float width)
{
    return (Rectangle){(VIEW_WIDTH - width) / 2.0f, top + (float)i * BUTTON_STEP, width, BUTTON_HEIGHT};
}

// Draw a panel around a column of buttons, with room for a heading
static void ColumnPanel(App *app, float top, int buttons, float width, float heading_height)
{
    float height = heading_height + (float)buttons * BUTTON_STEP - (BUTTON_STEP - BUTTON_HEIGHT) + 2.0f * PANEL_MARGIN;
    UiPanel(&app->ui, (Rectangle){(VIEW_WIDTH - width) / 2.0f - PANEL_MARGIN, top - heading_height - PANEL_MARGIN,
                                  width + 2.0f * PANEL_MARGIN, height});
}


static void DrawTitle(App *app, const char *text, float y, int size, Color color)
{
    UiTextCentered(&app->ui, text, VIEW_WIDTH / 2.0f + 2.0f, y + 2.0f, size, COLOR_SHADOW);
    UiTextCentered(&app->ui, text, VIEW_WIDTH / 2.0f, y, size, color);
}

static void OpenDifficulty(App *app, DifficultyChoice choice, Screen back)
{
    app->difficulty_choice = choice;
    app->difficulty_back = back;
    SetScreen(app, SCREEN_DIFFICULTY);
}

// A saved story can be continued, else the player chooses the difficulty of a new story
static void OpenStory(App *app)
{
    if (!app->has_story)
    {
        ContinueStory(app);
    }
    else if (app->settings.story_step > 0)
    {
        SetScreen(app, SCREEN_STORY);
    }
    else
    {
        OpenDifficulty(app, CHOICE_NEW_STORY, SCREEN_MENU);
    }
}

// Return the number of the stage of the saved story (1 for the first stage) and the number of stages
static int StoryStage(const App *app, int *count)
{
    int stage = 0;
    *count = 0;
    for (int i = 0; i < app->story.count; i++)
    {
        if (app->story.steps[i].type == STORY_STAGE)
        {
            (*count)++;
            if (i < app->settings.story_step)
            {
                stage++;
            }
        }
    }
    return stage + 1 > *count ? *count : stage + 1;
}

static void StoryScreen(App *app)
{
    Ui *ui = &app->ui;
    UiDim(ui, 0.5f);
    float top = 140.0f;
    ColumnPanel(app, top, 3, BUTTON_WIDTH, 56.0f);
    UiTextCentered(ui, "STORY", VIEW_WIDTH / 2.0f, top - 60.0f, HEADING_SIZE, COLOR_INK);
    int count;
    int stage = StoryStage(app, &count);
    UiTextCentered(ui, TextFormat("STAGE %d OF %d - %s", stage, count, DIFFICULTY_RULES[app->settings.difficulty].label),
                   VIEW_WIDTH / 2.0f, top - 24.0f, LABEL_SIZE, COLOR_INK);
    if (UiButton(ui, ButtonAt(top, 0, BUTTON_WIDTH), "CONTINUE"))
    {
        ContinueStory(app);
    }
    else if (UiButton(ui, ButtonAt(top, 1, BUTTON_WIDTH), "NEW STORY"))
    {
        OpenDifficulty(app, CHOICE_NEW_STORY, SCREEN_STORY);
    }
    else if (UiButton(ui, ButtonAt(top, 2, BUTTON_WIDTH), "BACK") || Back(app))
    {
        SetScreen(app, SCREEN_MENU);
    }
}

static void ChooseDifficulty(App *app, Difficulty difficulty)
{
    if (app->difficulty_choice == CHOICE_NEW_STORY)
    {
        SettingsNewStory(&app->settings, difficulty);
        SettingsSave(&app->settings);
        ContinueStory(app);
        return;
    }
    app->settings.difficulty = difficulty;
    SettingsSave(&app->settings);
    char name[LEVEL_MAX_NAME];
    snprintf(name, sizeof(name), "%s", app->game.level.name);
    StartStage(app, name);
}

static void DifficultyScreen(App *app)
{
    Ui *ui = &app->ui;
    UiDim(ui, 0.5f);
    bool new_story = app->difficulty_choice == CHOICE_NEW_STORY;
    bool replaces = new_story && app->settings.story_step > 0;
    float heading = replaces ? 56.0f : 36.0f;
    float height = heading + DIFFICULTY_COUNT * DIFFICULTY_STEP + BUTTON_HEIGHT;
    float top = floorf((VIEW_HEIGHT - height) / 2.0f) + heading;
    float x = (VIEW_WIDTH - DIFFICULTY_WIDTH) / 2.0f;
    UiPanel(ui, (Rectangle){x - PANEL_MARGIN, top - heading - PANEL_MARGIN, DIFFICULTY_WIDTH + 2.0f * PANEL_MARGIN,
                            height + 2.0f * PANEL_MARGIN});
    UiTextCentered(ui, new_story ? "NEW STORY" : "DIFFICULTY", VIEW_WIDTH / 2.0f, top - heading - 4.0f, HEADING_SIZE,
                   COLOR_INK);
    if (replaces)
    {
        UiTextCentered(ui, "THIS REPLACES YOUR SAVED STORY", VIEW_WIDTH / 2.0f, top - 24.0f, LABEL_SIZE,
                       COLOR_WARNING);
    }
    for (int i = 0; i < DIFFICULTY_COUNT; i++)
    {
        const DifficultyRules *rules = &DIFFICULTY_RULES[i];
        bool current = !new_story && (Difficulty)i == app->settings.difficulty;
        Rectangle rect = {x, top + (float)i * DIFFICULTY_STEP, DIFFICULTY_WIDTH, DIFFICULTY_STEP - 6.0f};
        const char *label = current ? TextFormat("> %s <", rules->label) : rules->label;
        if (UiColorButton(ui, rect, label, rules->description, rules->color, rules->ink))
        {
            ChooseDifficulty(app, (Difficulty)i);
            return;
        }
    }
    Rectangle back = {x, top + DIFFICULTY_COUNT * DIFFICULTY_STEP, DIFFICULTY_WIDTH, BUTTON_HEIGHT};
    if (UiButton(ui, back, "BACK") || Back(app))
    {
        SetScreen(app, app->difficulty_back);
    }
}

static void MenuScreen(App *app)
{
    Ui *ui = &app->ui;
    UiDim(ui, 0.35f);
    DrawTitle(app, "HOLY SHIFT", 40.0f, TITLE_SIZE, COLOR_TITLE);
    float top = 150.0f;
    ColumnPanel(app, top, 4, BUTTON_WIDTH, 0.0f);
    if (UiButton(ui, ButtonAt(top, 0, BUTTON_WIDTH), "STORY"))
    {
        OpenStory(app);
    }
    if (UiButton(ui, ButtonAt(top, 1, BUTTON_WIDTH), "FREE PLAY"))
    {
        SetScreen(app, SCREEN_STAGES);
    }
    if (UiButton(ui, ButtonAt(top, 2, BUTTON_WIDTH), "SETTINGS"))
    {
        app->settings_back = SCREEN_MENU;
        SetScreen(app, SCREEN_SETTINGS);
    }
    if (UiButton(ui, ButtonAt(top, 3, BUTTON_WIDTH), "QUIT"))
    {
        app->quit = true;
    }
    if (app->message[0] != '\0')
    {
        UiTextCentered(ui, app->message, VIEW_WIDTH / 2.0f, VIEW_HEIGHT - 24.0f, LABEL_SIZE, COLOR_WARNING);
    }
}

static void StagesScreen(App *app)
{
    Ui *ui = &app->ui;
    UiDim(ui, 0.5f);
    float width = 280.0f;
    float step = 32.0f; // smaller than BUTTON_STEP, so many levels fit
    float button = 28.0f;
    int count = app->level_count;
    float heading = 36.0f;
    float height = heading + (float)(count + 2) * step - (step - button);
    float top = floorf((VIEW_HEIGHT - height) / 2.0f) + heading;
    UiPanel(ui, (Rectangle){(VIEW_WIDTH - width) / 2.0f - PANEL_MARGIN, top - heading - PANEL_MARGIN,
                            width + 2.0f * PANEL_MARGIN, height + 2.0f * PANEL_MARGIN});
    UiTextCentered(ui, "FREE PLAY", VIEW_WIDTH / 2.0f, top - 40.0f, HEADING_SIZE, COLOR_INK);
    Difficulty difficulty = CurrentDifficulty(app);
    for (int i = 0; i < count; i++)
    {
        float best = SettingsBestTime(&app->settings, app->levels[i], difficulty);
        const char *medal = StandingsMedal(SettingsBestPlace(&app->settings, app->levels[i], difficulty));
        const char *label = best < 0.0f ? app->level_titles[i]
                                        : TextFormat("%s   %s   %s", app->level_titles[i], HudFormatTime(best),
                                                     medal != NULL ? medal : "");
        Rectangle rect = {(VIEW_WIDTH - width) / 2.0f, top + (float)i * step, width, button};
        if (UiButton(ui, rect, label))
        {
            StartStage(app, app->levels[i]);
        }
    }
    Rectangle rect = {(VIEW_WIDTH - width) / 2.0f, top + (float)count * step, width, button};
    if (UiButton(ui, rect, TextFormat("DIFFICULTY: %s", DIFFICULTY_RULES[difficulty].label)))
    {
        app->settings.difficulty = (Difficulty)((difficulty + 1) % DIFFICULTY_COUNT);
        app->options.has_difficulty = false;
        SettingsSave(&app->settings);
    }
    rect.y += step;
    if (UiButton(ui, rect, "BACK") || Back(app))
    {
        SetScreen(app, SCREEN_MENU);
    }
}

static void SettingsScreen(App *app)
{
    Ui *ui = &app->ui;
    Settings *settings = &app->settings;
    UiDim(ui, 0.5f);
    float width = 280.0f;
    float top = 78.0f;
    ColumnPanel(app, top, 7, width, 36.0f);
    UiTextCentered(ui, "SETTINGS", VIEW_WIDTH / 2.0f, top - 40.0f, HEADING_SIZE, COLOR_INK);

    bool changed = false;
    changed |= UiSlider(ui, ButtonAt(top, 0, width), "MOUSE", &settings->mouse_sensitivity, 0.25f, 3.0f, 0.05f,
                        "%.2fx");
    changed |= UiToggle(ui, ButtonAt(top, 1, width), "INVERT LOOK", &settings->invert_look);
    float percent = settings->volume * 100.0f;
    if (UiSlider(ui, ButtonAt(top, 2, width), "VOLUME", &percent, 0.0f, 100.0f, 5.0f, "%.0f%%"))
    {
        settings->volume = percent / 100.0f;
        changed = true;
    }
    float music = settings->music_volume * 100.0f;
    if (UiSlider(ui, ButtonAt(top, 3, width), "MUSIC", &music, 0.0f, 100.0f, 5.0f, "%.0f%%"))
    {
        settings->music_volume = music / 100.0f;
    }
    changed |= UiToggle(ui, ButtonAt(top, 4, width), "FULLSCREEN", &settings->fullscreen);
    if (changed)
    {
        SettingsApply(settings);
    }
    if (UiButton(ui, ButtonAt(top, 5, width), TextFormat("DIFFICULTY: %s", DIFFICULTY_RULES[settings->difficulty].label)))
    {
        settings->difficulty = (Difficulty)((settings->difficulty + 1) % DIFFICULTY_COUNT);
        app->options.has_difficulty = false;
    }
    if (UiButton(ui, ButtonAt(top, 6, width), "BACK") || Back(app))
    {
        SettingsSave(settings);
        // A new difficulty changes the road, so a stage that is paused starts again
        if (app->settings_back == SCREEN_PAUSED && app->game.difficulty != settings->difficulty)
        {
            char name[LEVEL_MAX_NAME];
            snprintf(name, sizeof(name), "%s", app->game.level.name);
            LoadStage(app, name);
        }
        SetScreen(app, app->settings_back);
    }
}

static void Resume(App *app)
{
    CarAudioSetPlaying(&app->game.audio, true);
    GameResume(&app->game);
    SetScreen(app, SCREEN_PLAYING);
}

static void PausedScreen(App *app)
{
    Ui *ui = &app->ui;
    UiDim(ui, 0.5f);
    float top = 120.0f;
    ColumnPanel(app, top, 4, BUTTON_WIDTH, 36.0f);
    UiTextCentered(ui, "PAUSED", VIEW_WIDTH / 2.0f, top - 40.0f, HEADING_SIZE, COLOR_INK);
    if (UiButton(ui, ButtonAt(top, 0, BUTTON_WIDTH), "RESUME") || Back(app))
    {
        Resume(app);
    }
    else if (UiButton(ui, ButtonAt(top, 1, BUTTON_WIDTH), "RESTART"))
    {
        GameRestart(&app->game);
        Resume(app);
    }
    else if (UiButton(ui, ButtonAt(top, 2, BUTTON_WIDTH), "SETTINGS"))
    {
        app->settings_back = SCREEN_PAUSED;
        SetScreen(app, SCREEN_SETTINGS);
    }
    else if (UiButton(ui, ButtonAt(top, 3, BUTTON_WIDTH), "MENU"))
    {
        OpenMenu(app);
    }
}

static void CrashedScreen(App *app)
{
    Ui *ui = &app->ui;
    float top = 140.0f;
    float width = 220.0f;
    ColumnPanel(app, top, 3, width, 36.0f);
    UiTextCentered(ui, "CRASHED", VIEW_WIDTH / 2.0f, top - 40.0f, HEADING_SIZE, COLOR_WARNING);
    if (UiButton(ui, ButtonAt(top, 0, width), "RETRY") || IsKeyPressed(KEY_R))
    {
        GameRestart(&app->game);
        Resume(app);
    }
    else if (UiButton(ui, ButtonAt(top, 1, width),
                      TextFormat("DIFFICULTY: %s", DIFFICULTY_RULES[app->game.difficulty].label)))
    {
        OpenDifficulty(app, CHOICE_RETRY, SCREEN_CRASHED);
    }
    else if (UiButton(ui, ButtonAt(top, 2, width), "MENU") || Back(app))
    {
        OpenMenu(app);
    }
}

static Color MedalColor(int place)
{
    static const Color colors[] = {{232, 184, 48, 255}, {200, 204, 214, 255}, {200, 124, 64, 255}};
    return place >= 1 && place <= 3 ? colors[place - 1] : COLOR_INK;
}

// The result of a tutorial: only the time of the player, no standings
static void TutorialFinishedScreen(App *app)
{
    Ui *ui = &app->ui;
    UiDim(ui, 0.4f);
    int buttons = app->in_story ? 3 : 2;
    float heading = 92.0f;
    float top = floorf((VIEW_HEIGHT - heading - (float)buttons * BUTTON_STEP) / 2.0f) + heading;
    ColumnPanel(app, top, buttons, BUTTON_WIDTH, heading);
    float middle = VIEW_WIDTH / 2.0f;
    float y = top - heading - 2.0f;
    UiTextCentered(ui, app->game.level.display_name, middle, y, LABEL_SIZE, COLOR_INK);
    UiTextCentered(ui, "FINISH", middle, y + 16.0f, HEADING_SIZE, COLOR_INK);
    UiTextCentered(ui, HudFormatTime(app->game.stage.time), middle, y + 44.0f, HEADING_SIZE, COLOR_INK);
    float best = SettingsBestTime(&app->settings, app->game.level.name, app->game.difficulty);
    const char *line = app->new_best ? "NEW BEST TIME!" : (best >= 0.0f ? TextFormat("BEST  %s", HudFormatTime(best)) : "");
    UiTextCentered(ui, line, middle, y + 72.0f, LABEL_SIZE, app->new_best ? COLOR_WARNING : COLOR_INK);

    int button = 0;
    if (app->in_story && UiButton(ui, ButtonAt(top, button++, BUTTON_WIDTH), "CONTINUE"))
    {
        NextStoryStep(app);
    }
    else if (UiButton(ui, ButtonAt(top, button++, BUTTON_WIDTH), "DRIVE AGAIN") || IsKeyPressed(KEY_R))
    {
        GameRestart(&app->game);
        Resume(app);
    }
    else if (UiButton(ui, ButtonAt(top, button++, BUTTON_WIDTH), "MENU") || Back(app))
    {
        OpenMenu(app);
    }
}

// The results of the stage: the place, the medal, the standings of all drivers and the buttons
static void FinishedScreen(App *app)
{
    if (app->game.level.tutorial)
    {
        TutorialFinishedScreen(app);
        return;
    }
    Ui *ui = &app->ui;
    const Standings *standings = &app->standings;
    int place = standings->place;
    UiDim(ui, 0.5f);

    bool story_next = app->in_story && app->qualified;
    int buttons = story_next ? 3 : 2;
    float row_height = 14.0f;
    float message = app->in_story && !app->qualified ? 16.0f : 0.0f;
    float height = 12.0f + 16.0f + 30.0f + 18.0f + 6.0f + STANDINGS_DRIVERS * row_height + 8.0f + message +
                   (float)buttons * BUTTON_STEP - (BUTTON_STEP - BUTTON_HEIGHT) + 12.0f;
    float width = 300.0f;
    Rectangle panel = {(VIEW_WIDTH - width) / 2.0f, floorf((VIEW_HEIGHT - height) / 2.0f), width, height};
    UiPanel(ui, panel);
    float middle = VIEW_WIDTH / 2.0f;
    float y = panel.y + 10.0f;
    UiTextCentered(ui, TextFormat("%s - %s", app->game.level.display_name, DIFFICULTY_RULES[app->game.difficulty].label),
                   middle, y, LABEL_SIZE, COLOR_INK);
    y += 16.0f;

    // The place, with a medal for the first three places
    const char *place_text = TextFormat("%s PLACE", StandingsPlaceName(place));
    UiTextCentered(ui, place_text, middle, y, HEADING_SIZE, COLOR_INK);
    if (StandingsMedal(place) != NULL)
    {
        float half = UiTextWidth(ui, place_text, HEADING_SIZE) / 2.0f;
        Vector2 center = {middle - half - 14.0f, y + 15.0f};
        UiCircle(ui, center, 8.0f, MedalColor(place), COLOR_INK);
        UiTextCentered(ui, TextFormat("%d", place), center.x, center.y - 7.0f, LABEL_SIZE, COLOR_INK);
    }
    y += 30.0f;
    const char *line = TextFormat("%s   %s", HudFormatTime(app->game.stage.time),
                                  app->new_best ? "NEW BEST TIME!" : (StandingsMedal(place) != NULL ?
                                  TextFormat("%s MEDAL", StandingsMedal(place)) : "NO MEDAL"));
    UiTextCentered(ui, line, middle, y, LABEL_SIZE, app->new_best ? COLOR_WARNING : COLOR_INK);
    y += 18.0f + 6.0f;

    // The standings, the row of the player is lit
    float left = panel.x + 18.0f;
    float right = panel.x + panel.width - 18.0f;
    for (int i = 0; i < STANDINGS_DRIVERS; i++)
    {
        const StandingsRow *row = &standings->rows[i];
        if (row->player)
        {
            UiFill(ui, (Rectangle){left - 6.0f, y - 1.0f, right - left + 12.0f, row_height}, COLOR_HIGHLIGHT);
        }
        UiText(ui, TextFormat("%d.", i + 1), left, y - 3.0f, LABEL_SIZE, COLOR_INK);
        UiText(ui, row->name, left + 24.0f, y - 3.0f, LABEL_SIZE, COLOR_INK);
        const char *time = HudFormatTime(row->time);
        UiText(ui, time, right - UiTextWidth(ui, time, LABEL_SIZE), y - 3.0f, LABEL_SIZE, COLOR_INK);
        y += row_height;
    }
    y += 8.0f;
    if (message > 0.0f)
    {
        UiTextCentered(ui, app->final_stage ? "ONLY A WIN COUNTS IN THE FINAL" : "TOP 3 TO GO ON", middle, y - 2.0f,
                       LABEL_SIZE, COLOR_WARNING);
        y += message;
    }

    int button = 0;
    if (story_next && UiButton(ui, ButtonAt(y, button++, BUTTON_WIDTH), "CONTINUE"))
    {
        NextStoryStep(app);
    }
    else if (UiButton(ui, ButtonAt(y, button++, BUTTON_WIDTH), "DRIVE AGAIN") || IsKeyPressed(KEY_R))
    {
        GameRestart(&app->game);
        Resume(app);
    }
    else if (UiButton(ui, ButtonAt(y, button++, BUTTON_WIDTH), "MENU") || Back(app))
    {
        OpenMenu(app);
    }
}

void AppFrame(App *app)
{
    float dt = GetFrameTime();
    app->screen_changed = false;
    if (IsKeyPressed(KEY_F11))
    {
        app->settings.fullscreen = !app->settings.fullscreen;
        SettingsApply(&app->settings);
        SettingsSave(&app->settings);
    }

    Game *game = &app->game;
    game->mouse_sensitivity = app->settings.mouse_sensitivity;
    game->invert_look = app->settings.invert_look;
    if (app->screen == SCREEN_PLAYING)
    {
        UpdatePlaying(app, dt);
    }
    // The screens of the main menu, the game behind them stands still
    bool in_menu = app->screen == SCREEN_MENU || app->screen == SCREEN_STAGES || app->screen == SCREEN_STORY ||
                   (app->screen == SCREEN_DIFFICULTY && app->difficulty_choice == CHOICE_NEW_STORY) ||
                   (app->screen == SCREEN_SETTINGS && app->settings_back == SCREEN_MENU);
    char scene_path[256];
    SceneMusicPath(app, scene_path, sizeof(scene_path));
    bool menu_track = in_menu || strcmp(scene_path, app->menu_music.path) == 0;
    MusicPlayerUpdate(&app->menu_music, menu_track, app->settings.music_volume, dt);
    // The driving music also goes on in the pause menu, it stops after a crash and at the finish
    bool driving = app->screen == SCREEN_PLAYING || app->screen == SCREEN_PAUSED ||
                   (app->screen == SCREEN_SETTINGS && app->settings_back == SCREEN_PAUSED);
    MusicPlayerUpdate(&app->drive_music, driving, app->settings.music_volume, dt);
    UpdateSceneMusic(app, dt);
    if (in_menu)
    {
        app->menu_time += dt;
        game->head.yaw = MENU_LOOK_DEG * sinf(app->menu_time * MENU_LOOK_SPEED);
    }
    UpdateCursor(app);
    game->show_hud = app->screen == SCREEN_PLAYING || app->screen == SCREEN_PAUSED;

    GameRender(game);
    Rectangle area = PresentDestination(GetScreenWidth(), GetScreenHeight());
    BeginDrawing();
    ClearBackground(BLACK);
    GameDrawView(game, area);
    UiBegin(&app->ui, area);
    if (app->screen == SCREEN_PLAYING)
    {
        HintsDraw(&game->hints, &app->ui);
    }
    switch (app->screen)
    {
    case SCREEN_MENU:
        MenuScreen(app);
        break;
    case SCREEN_STAGES:
        StagesScreen(app);
        break;
    case SCREEN_SETTINGS:
        SettingsScreen(app);
        break;
    case SCREEN_PAUSED:
        PausedScreen(app);
        break;
    case SCREEN_CRASHED:
        CrashedScreen(app);
        break;
    case SCREEN_FINISHED:
        FinishedScreen(app);
        break;
    case SCREEN_SCENE:
        SceneScreen(app);
        break;
    case SCREEN_STORY:
        StoryScreen(app);
        break;
    case SCREEN_DIFFICULTY:
        DifficultyScreen(app);
        break;
    case SCREEN_PLAYING:
        break;
    }
    UiEnd(&app->ui);
    EndDrawing();
}
