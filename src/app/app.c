#include "app/app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/config.h"
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
#define COLOR_INK ((Color){60, 40, 24, 255})
#define COLOR_WARNING ((Color){230, 90, 40, 255})

static void SetScreen(App *app, Screen screen)
{
    app->screen = screen;
    app->screen_changed = true;
    UiReset(&app->ui);
}

static bool LoadStage(App *app, const char *name)
{
    if (app->game_ready && strcmp(app->game.level.name, name) == 0)
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
    app->game_ready = GameInit(&app->game, &options, app->message, sizeof(app->message));
    if (app->game_ready)
    {
        app->game.crash_menu = true;
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
    if (app->scene_playing)
    {
        ScenePlayerStop(&app->scene);
        app->scene_playing = false;
    }
}

// Go to the step of the story mode in the settings, the menu opens after the last step
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

static void SceneScreen(App *app)
{
    bool advance = !app->screen_changed && (app->ui.select || IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    ScenePlayerUpdate(&app->scene, GetFrameTime(), advance, Back(app));
    ScenePlayerDraw(&app->scene, &app->ui);
    if (ScenePlayerDone(&app->scene))
    {
        NextStoryStep(app);
    }
}

// Open the screen of the --screen option, for development
static void OpenStartScreen(App *app, const char *name)
{
    const char *names[] = {"stages", "settings", "paused", "crashed", "finished"};
    Screen screens[] = {SCREEN_STAGES, SCREEN_SETTINGS, SCREEN_PAUSED, SCREEN_CRASHED, SCREEN_FINISHED};
    if (name != NULL && strncmp(name, "scene:", 6) == 0)
    {
        app->scene_playing = ScenePlayerStart(&app->scene, name + 6, app->message, sizeof(app->message));
        if (app->scene_playing)
        {
            SetScreen(app, SCREEN_SCENE);
        }
        return;
    }
    for (int i = 0; name != NULL && i < 5; i++)
    {
        if (strcmp(name, names[i]) == 0)
        {
            SetScreen(app, screens[i]);
            app->settings_back = SCREEN_MENU;
            app->game.crash_time = screens[i] == SCREEN_CRASHED ? 10.0f : -1.0f;
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
    app->level_count = LevelFindAll(app->levels, APP_MAX_LEVELS);
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
        app->new_best = SettingsRecordTime(&app->settings, game->level.name, game->stage.time);
        if (app->new_best)
        {
            SettingsSave(&app->settings);
        }
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

static void MenuScreen(App *app)
{
    Ui *ui = &app->ui;
    UiDim(ui, 0.35f);
    DrawTitle(app, "HOLY SHIFT", 40.0f, TITLE_SIZE, COLOR_TITLE);
    float top = 150.0f;
    ColumnPanel(app, top, 4, BUTTON_WIDTH, 0.0f);
    if (UiButton(ui, ButtonAt(top, 0, BUTTON_WIDTH), "STORY"))
    {
        ContinueStory(app);
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
    float width = 260.0f;
    float top = 100.0f;
    int count = app->level_count;
    ColumnPanel(app, top, count + 1, width, 36.0f);
    UiTextCentered(ui, "FREE PLAY", VIEW_WIDTH / 2.0f, top - 40.0f, HEADING_SIZE, COLOR_INK);
    for (int i = 0; i < count; i++)
    {
        float best = SettingsBestTime(&app->settings, app->levels[i]);
        const char *label = best < 0.0f ? app->levels[i] : TextFormat("%s   %s", app->levels[i], HudFormatTime(best));
        if (UiButton(ui, ButtonAt(top, i, width), label))
        {
            StartStage(app, app->levels[i]);
        }
    }
    if (UiButton(ui, ButtonAt(top, count, width), "BACK") || Back(app))
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
    float top = 100.0f;
    ColumnPanel(app, top, 5, width, 36.0f);
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
    changed |= UiToggle(ui, ButtonAt(top, 3, width), "FULLSCREEN", &settings->fullscreen);
    if (changed)
    {
        SettingsApply(settings);
    }
    if (UiButton(ui, ButtonAt(top, 4, width), "BACK") || Back(app))
    {
        SettingsSave(settings);
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
    float top = 150.0f;
    ColumnPanel(app, top, 2, BUTTON_WIDTH, 36.0f);
    UiTextCentered(ui, "CRASHED", VIEW_WIDTH / 2.0f, top - 40.0f, HEADING_SIZE, COLOR_WARNING);
    if (UiButton(ui, ButtonAt(top, 0, BUTTON_WIDTH), "RETRY") || IsKeyPressed(KEY_R))
    {
        GameRestart(&app->game);
        Resume(app);
    }
    else if (UiButton(ui, ButtonAt(top, 1, BUTTON_WIDTH), "MENU") || Back(app))
    {
        OpenMenu(app);
    }
}

static void FinishedScreen(App *app)
{
    Ui *ui = &app->ui;
    UiDim(ui, 0.4f);
    float top = app->in_story ? 150.0f : 170.0f;
    float heading = 76.0f;
    ColumnPanel(app, top, app->in_story ? 3 : 2, BUTTON_WIDTH, heading);
    float y = top - heading - 2.0f;
    UiTextCentered(ui, "FINISH", VIEW_WIDTH / 2.0f, y, HEADING_SIZE, COLOR_INK);
    UiTextCentered(ui, HudFormatTime(app->game.stage.time), VIEW_WIDTH / 2.0f, y + 28.0f, HEADING_SIZE, COLOR_INK);
    float best = SettingsBestTime(&app->settings, app->game.level.name);
    const char *line = app->new_best ? "NEW BEST TIME!" : (best < 0.0f ? "" : TextFormat("BEST  %s", HudFormatTime(best)));
    UiTextCentered(ui, line, VIEW_WIDTH / 2.0f, y + 56.0f, LABEL_SIZE, app->new_best ? COLOR_WARNING : COLOR_INK);
    if (app->in_story && UiButton(ui, ButtonAt(top, 0, BUTTON_WIDTH), "CONTINUE"))
    {
        NextStoryStep(app);
    }
    else if (UiButton(ui, ButtonAt(top, app->in_story ? 1 : 0, BUTTON_WIDTH), "DRIVE AGAIN") || IsKeyPressed(KEY_R))
    {
        GameRestart(&app->game);
        Resume(app);
    }
    else if (UiButton(ui, ButtonAt(top, app->in_story ? 2 : 1, BUTTON_WIDTH), "MENU") || Back(app))
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
    else if (app->screen == SCREEN_MENU || app->screen == SCREEN_STAGES ||
             (app->screen == SCREEN_SETTINGS && app->settings_back == SCREEN_MENU))
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
    case SCREEN_PLAYING:
        break;
    }
    UiEnd(&app->ui);
    EndDrawing();
}
