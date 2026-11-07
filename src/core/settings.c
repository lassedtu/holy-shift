#include "core/settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/config.h"
#include "raylib.h"

void SettingsLoad(Settings *settings)
{
    *settings = (Settings){.mouse_sensitivity = 1.0f, .volume = 0.8f};
    char *text = FileExists(SAVE_PATH) ? LoadFileText(SAVE_PATH) : NULL;
    if (text == NULL)
    {
        return;
    }

    // One value on each line: "<key> <value>", or "best <level> <seconds>"
    char *line = text;
    while (line != NULL && *line != '\0')
    {
        char *next = strchr(line, '\n');
        if (next != NULL)
        {
            *next++ = '\0';
        }
        char key[32];
        char name[LEVEL_MAX_NAME];
        float value;
        if (sscanf(line, "best %63s %f", name, &value) == 2)
        {
            SettingsRecordTime(settings, name, value);
        }
        else if (sscanf(line, "%31s %f", key, &value) == 2)
        {
            if (strcmp(key, "mouse_sensitivity") == 0)
            {
                settings->mouse_sensitivity = value;
            }
            else if (strcmp(key, "invert_look") == 0)
            {
                settings->invert_look = value != 0.0f;
            }
            else if (strcmp(key, "volume") == 0)
            {
                settings->volume = value;
            }
            else if (strcmp(key, "fullscreen") == 0)
            {
                settings->fullscreen = value != 0.0f;
            }
        }
        line = next;
    }
    UnloadFileText(text);
}

bool SettingsSave(const Settings *settings)
{
    char text[8192];
    int length = snprintf(text, sizeof(text), "mouse_sensitivity %.2f\ninvert_look %d\nvolume %.2f\nfullscreen %d\n",
                          settings->mouse_sensitivity, settings->invert_look, settings->volume, settings->fullscreen);
    for (int i = 0; i < settings->best_count && length < (int)sizeof(text); i++)
    {
        length += snprintf(text + length, sizeof(text) - (size_t)length, "best %s %.3f\n", settings->best[i].level,
                           settings->best[i].seconds);
    }
    return SaveFileText(SAVE_PATH, text);
}

void SettingsApply(const Settings *settings)
{
    SetMasterVolume(settings->volume);
    if (settings->fullscreen != IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE))
    {
        ToggleBorderlessWindowed();
    }
}

float SettingsBestTime(const Settings *settings, const char *level)
{
    for (int i = 0; i < settings->best_count; i++)
    {
        if (strcmp(settings->best[i].level, level) == 0)
        {
            return settings->best[i].seconds;
        }
    }
    return -1.0f;
}

bool SettingsRecordTime(Settings *settings, const char *level, float seconds)
{
    for (int i = 0; i < settings->best_count; i++)
    {
        BestTime *best = &settings->best[i];
        if (strcmp(best->level, level) == 0)
        {
            if (seconds >= best->seconds)
            {
                return false;
            }
            best->seconds = seconds;
            return true;
        }
    }
    if (settings->best_count == SETTINGS_MAX_BEST)
    {
        return false;
    }
    BestTime *best = &settings->best[settings->best_count++];
    snprintf(best->level, sizeof(best->level), "%s", level);
    best->seconds = seconds;
    return true;
}
