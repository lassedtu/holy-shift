#include "core/settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/config.h"
#include "raylib.h"

void SettingsLoad(Settings *settings)
{
    *settings = (Settings){.mouse_sensitivity = 1.0f, .volume = 0.8f, .music_volume = 1.0f, .difficulty = DIFFICULTY_MEDIUM};
    char *text = FileExists(SAVE_PATH) ? LoadFileText(SAVE_PATH) : NULL;
    if (text == NULL)
    {
        return;
    }

    // One value on each line: "<key> <value>", or "best <level> <seconds> <place> <difficulty>"
    // A best line without a difficulty is from before the difficulties, it is medium
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
        int place = 0;
        int difficulty = DIFFICULTY_MEDIUM;
        if (sscanf(line, "best %63s %f %d %d", name, &value, &place, &difficulty) >= 2)
        {
            if (difficulty < 0 || difficulty >= DIFFICULTY_COUNT)
            {
                difficulty = DIFFICULTY_MEDIUM;
            }
            SettingsRecordTime(settings, name, (Difficulty)difficulty, value);
            SettingsRecordPlace(settings, name, (Difficulty)difficulty, place);
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
            else if (strcmp(key, "music_volume") == 0)
            {
                settings->music_volume = value;
            }
            else if (strcmp(key, "fullscreen") == 0)
            {
                settings->fullscreen = value != 0.0f;
            }
            else if (strcmp(key, "difficulty") == 0 && value >= 0.0f && value < (float)DIFFICULTY_COUNT)
            {
                settings->difficulty = (Difficulty)value;
            }
            else if (strcmp(key, "story_step") == 0)
            {
                settings->story_step = (int)value;
            }
            else if (strcmp(key, "story_last_time") == 0)
            {
                settings->story_last_time = value;
            }
        }
        line = next;
    }
    UnloadFileText(text);
}

bool SettingsSave(const Settings *settings)
{
    char text[8192];
    int length = snprintf(text, sizeof(text),
                          "mouse_sensitivity %.2f\ninvert_look %d\nvolume %.2f\nmusic_volume %.2f\nfullscreen %d\ndifficulty %d\n"
                          "story_step %d\nstory_last_time %.3f\n",
                          settings->mouse_sensitivity, settings->invert_look, settings->volume, settings->music_volume,
                          settings->fullscreen,
                          (int)settings->difficulty, settings->story_step, settings->story_last_time);
    for (int i = 0; i < settings->best_count && length < (int)sizeof(text); i++)
    {
        const BestTime *best = &settings->best[i];
        length += snprintf(text + length, sizeof(text) - (size_t)length, "best %s %.3f %d %d\n", best->level,
                           best->seconds, best->place, (int)best->difficulty);
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

// Return the entry of a level on a difficulty, NULL when there is none
static const BestTime *Find(const Settings *settings, const char *level, Difficulty difficulty)
{
    for (int i = 0; i < settings->best_count; i++)
    {
        if (settings->best[i].difficulty == difficulty && strcmp(settings->best[i].level, level) == 0)
        {
            return &settings->best[i];
        }
    }
    return NULL;
}

float SettingsBestTime(const Settings *settings, const char *level, Difficulty difficulty)
{
    const BestTime *best = Find(settings, level, difficulty);
    return best != NULL ? best->seconds : -1.0f;
}

// Return the entry of a level, make a new entry when there is none, NULL when there is no room
static BestTime *Entry(Settings *settings, const char *level, Difficulty difficulty)
{
    BestTime *found = (BestTime *)Find(settings, level, difficulty);
    if (found != NULL)
    {
        return found;
    }
    if (settings->best_count == SETTINGS_MAX_BEST)
    {
        return NULL;
    }
    BestTime *best = &settings->best[settings->best_count++];
    *best = (BestTime){.difficulty = difficulty, .seconds = -1.0f};
    snprintf(best->level, sizeof(best->level), "%s", level);
    return best;
}

bool SettingsRecordTime(Settings *settings, const char *level, Difficulty difficulty, float seconds)
{
    BestTime *best = seconds >= 0.0f ? Entry(settings, level, difficulty) : NULL;
    if (best == NULL || (best->seconds >= 0.0f && seconds >= best->seconds))
    {
        return false;
    }
    best->seconds = seconds;
    return true;
}

bool SettingsRecordPlace(Settings *settings, const char *level, Difficulty difficulty, int place)
{
    BestTime *best = place > 0 ? Entry(settings, level, difficulty) : NULL;
    if (best == NULL || (best->place > 0 && best->place <= place))
    {
        return false;
    }
    best->place = place;
    return true;
}

int SettingsBestPlace(const Settings *settings, const char *level, Difficulty difficulty)
{
    const BestTime *best = Find(settings, level, difficulty);
    return best != NULL ? best->place : 0;
}

void SettingsNewStory(Settings *settings, Difficulty difficulty)
{
    settings->difficulty = difficulty;
    settings->story_step = 0;
    settings->story_last_time = 0.0f;
}
