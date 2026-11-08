#include <stdio.h>

#include "core/config.h"
#include "core/options.h"
#include "app/app.h"
#include "raylib.h"
#include "ui/text.h"

#define SCREENSHOT_FRAME 3 // frame of the picture, the first frames can be incomplete

static void SaveScreenshot(const char *path)
{
    Image image = LoadImageFromScreen();
    ExportImage(image, path);
    UnloadImage(image);
}

int main(int argc, char **argv)
{
    Options options;
    char message[256] = "";
    if (!OptionsParse(&options, argc, argv, message, sizeof(message)))
    {
        fprintf(stderr, "%s\n", message);
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(VIEW_WIDTH * WINDOW_SCALE, VIEW_HEIGHT * WINDOW_SCALE, WINDOW_TITLE);
    SetTargetFPS(TARGET_FPS);
    SetExitKey(KEY_NULL); // Esc opens the pause menu
    if (options.screenshot == NULL)
    {
        InitAudioDevice();
    }

    static App app;
    if (!AppInit(&app, &options, message, sizeof(message)))
    {
        fprintf(stderr, "%s\n", message);
        if (IsAudioDeviceReady())
        {
            CloseAudioDevice();
        }
        CloseWindow();
        return 1;
    }

    int frames = 0;
    while (!WindowShouldClose() && !AppShouldQuit(&app))
    {
        AppFrame(&app);

        if (options.screenshot != NULL && ++frames == (options.shot_frame > 0 ? options.shot_frame : SCREENSHOT_FRAME))
        {
            SaveScreenshot(options.screenshot);
            break;
        }
    }

    AppShutdown(&app);
    TextUnloadAll();
    if (IsAudioDeviceReady())
    {
        CloseAudioDevice();
    }
    CloseWindow();
    return 0;
}
