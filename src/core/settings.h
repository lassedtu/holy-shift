#ifndef CORE_SETTINGS_H
#define CORE_SETTINGS_H

#include <stdbool.h>

#include "level/level.h"

#define SETTINGS_MAX_BEST 64

typedef struct BestTime
{
    char level[LEVEL_MAX_NAME];
    float seconds;
} BestTime;

// The settings of the player and the best time of each level, kept in SAVE_PATH
typedef struct Settings
{
    float mouse_sensitivity; // 1 is MOUSE_SENSITIVITY_DEG
    bool invert_look;        // true when the mouse moves the view up and down the other way
    float volume;            // from 0 to 1
    bool fullscreen;
    int story_step; // the next step of the story mode
    BestTime best[SETTINGS_MAX_BEST];
    int best_count;
} Settings;

// Read the file, the default values are used for each value that is not in the file
void SettingsLoad(Settings *settings);
bool SettingsSave(const Settings *settings);

// Put the volume and the window mode into effect
void SettingsApply(const Settings *settings);

// Return the best time of a level in seconds, or a negative value when there is none
float SettingsBestTime(const Settings *settings, const char *level);

// Keep a time when it is better than the best time of the level, return true when it is
bool SettingsRecordTime(Settings *settings, const char *level, float seconds);

#endif
