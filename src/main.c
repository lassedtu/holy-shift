#include <stdio.h>

#include "core/config.h"
#include "core/options.h"
#include "game/game.h"
#include "raylib.h"

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
    DisableCursor(); // the mouse movement turns the camera
    if (options.screenshot == NULL)
    {
        InitAudioDevice();
    }

    static Game game;
    if (!GameInit(&game, &options, message, sizeof(message)))
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
    while (!WindowShouldClose())
    {
        GameUpdate(&game, GetFrameTime());
        GameDraw(&game);

        if (options.screenshot != NULL && ++frames == SCREENSHOT_FRAME)
        {
            SaveScreenshot(options.screenshot);
            break;
        }
    }

    GameShutdown(&game);
    if (IsAudioDeviceReady())
    {
        CloseAudioDevice();
    }
    CloseWindow();
    return 0;
}
