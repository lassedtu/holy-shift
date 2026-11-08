#ifndef CORE_DIFFICULTY_H
#define CORE_DIFFICULTY_H

#include <stdbool.h>

#include "raylib.h"

// The difficulty of the game, the player chooses it for a new story, in the settings and after a crash
// The levels are designed on medium
// The level builder also uses this file
typedef enum Difficulty
{
    DIFFICULTY_EASY,
    DIFFICULTY_MEDIUM,
    DIFFICULTY_HARD,
    DIFFICULTY_ULTRA,
    DIFFICULTY_COUNT
} Difficulty;

typedef struct DifficultyRules
{
    const char *name;        // the name in the save file
    const char *label;       // the name that the player sees
    const char *description; // one short line for the menu
    const char *times_key;   // the key of the times of the other drivers in level.json
    float road_width;        // the road width of the level is multiplied by this
    float rival_time;        // the times of the other drivers are multiplied by this when the level has no times
                             // for this difficulty
    bool surfaces;           // false when the surfaces of the road (mud) do not change the grip
    float map_events;        // how often Luka's map events happen, 1 is the normal amount
    Color color;             // the color of the difficulty in the menus
    Color ink;               // the color of a text on the color
} DifficultyRules;

static const DifficultyRules DIFFICULTY_RULES[DIFFICULTY_COUNT] = {
    [DIFFICULTY_EASY] = {"easy", "EASY", "Wide roads, slow rivals, no mud", "times_easy", 1.4f, 1.18f, false, 0.5f,
                         {70, 160, 70, 255}, {250, 246, 230, 255}},
    [DIFFICULTY_MEDIUM] = {"medium", "MEDIUM", "The rally as it is meant to be", "times", 1.0f, 1.0f, true, 1.0f,
                           {236, 192, 40, 255}, {60, 40, 24, 255}},
    [DIFFICULTY_HARD] = {"hard", "HARD", "Narrow roads, fast rivals", "times_hard", 0.75f, 0.93f, true, 1.5f,
                         {196, 54, 44, 255}, {250, 236, 220, 255}},
    [DIFFICULTY_ULTRA] = {"ultra", "ULTRA HARD", "Very thin roads, the best rivals", "times_ultra", 0.5f, 0.98f, true,
                          2.0f, {34, 32, 36, 255}, {236, 230, 220, 255}},
};

#endif
