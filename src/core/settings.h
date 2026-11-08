#ifndef CORE_SETTINGS_H
#define CORE_SETTINGS_H

#include <stdbool.h>

#include "core/difficulty.h"
#include "level/level.h"

#define SETTINGS_MAX_BEST 128

// The best result of a level on one difficulty
typedef struct BestTime
{
    char level[LEVEL_MAX_NAME];
    Difficulty difficulty;
    float seconds; // negative when there is no time
    int place;     // the best place in the standings, 0 when there is none
} BestTime;

// The settings of the player and the best time of each level, kept in SAVE_PATH
typedef struct Settings
{
    float mouse_sensitivity; // 1 is MOUSE_SENSITIVITY_DEG
    bool invert_look;        // true when the mouse moves the view up and down the other way
    float volume;            // from 0 to 1, all the sound of the game
    float music_volume;      // from 0 to 1, the music, the volume multiplies it
    bool fullscreen;
    Difficulty difficulty; // for the story and for free play
    int story_step;        // the next step of the story mode, 0 when there is no story to continue
    float story_last_time; // seconds of the last stage that the player drove in the story mode
    BestTime best[SETTINGS_MAX_BEST];
    int best_count;
} Settings;

// Read the file, the default values are used for each value that is not in the file
void SettingsLoad(Settings *settings);
bool SettingsSave(const Settings *settings);

// Put the volume and the window mode into effect
void SettingsApply(const Settings *settings);

// The best times and places are kept for each difficulty

// Return the best time of a level in seconds, or a negative value when there is none
float SettingsBestTime(const Settings *settings, const char *level, Difficulty difficulty);

// Keep a time when it is better than the best time of the level, return true when it is
bool SettingsRecordTime(Settings *settings, const char *level, Difficulty difficulty, float seconds);

// Keep a place when it is better than the best place of the level, return true when it is
bool SettingsRecordPlace(Settings *settings, const char *level, Difficulty difficulty, int place);

// Return the best place of a level, 0 when there is none
int SettingsBestPlace(const Settings *settings, const char *level, Difficulty difficulty);

// Start a new story on a difficulty, the old story is lost
void SettingsNewStory(Settings *settings, Difficulty difficulty);

#endif
