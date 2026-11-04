#include "core/options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool OptionsParse(Options *options, int argc, char **argv, char *message, size_t size)
{
    *options = (Options){0};
    bool fly = false;
    bool selfdrive = false;

    for (int i = 1; i < argc; i++)
    {
        bool has_value = i + 1 < argc;
        if (strcmp(argv[i], "--debug_fly") == 0)
        {
            fly = true;
        }
        else if (strcmp(argv[i], "--debug_selfdrive") == 0)
        {
            selfdrive = true;
        }
        else if (has_value && strcmp(argv[i], "--shot") == 0)
        {
            options->screenshot = argv[++i];
        }
        else if (has_value && strcmp(argv[i], "--skip") == 0)
        {
            options->skip_seconds = (float)atof(argv[++i]);
        }
        else if (has_value && strcmp(argv[i], "--yaw") == 0)
        {
            options->yaw_deg = (float)atof(argv[++i]);
        }
        else if (argv[i][0] != '-' && argv[i][0] != '\0')
        {
            options->level = argv[i];
        }
    }

    if (fly && selfdrive)
    {
        snprintf(message, size, "--debug_fly and --debug_selfdrive cannot be used together");
        return false;
    }
    if (fly)
    {
        options->mode = GAME_MODE_DEBUG_FLY;
    }
    else if (selfdrive)
    {
        options->mode = GAME_MODE_DEBUG_SELFDRIVE;
    }
    return true;
}
