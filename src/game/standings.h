#ifndef GAME_STANDINGS_H
#define GAME_STANDINGS_H

#include <stdbool.h>

#include "level/level.h"

#define STANDINGS_DRIVERS (LEVEL_RIVALS + 1) // the other drivers and the player

typedef struct StandingsRow
{
    const char *name;
    float time; // seconds
    bool player;
} StandingsRow;

// The result of a stage: all the drivers from the fastest to the slowest
typedef struct Standings
{
    StandingsRow rows[STANDINGS_DRIVERS];
    int place; // the place of the player, 1 is the first place
} Standings;

// Make the standings of a level for a time of the player
// The times of the other drivers come from the level, or are made from the length of the track (map pixels)
void StandingsMake(Standings *standings, const Level *level, float track_length, float player_time);

// Return "GOLD", "SILVER" or "BRONZE" for the first three places, NULL for the other places
const char *StandingsMedal(int place);

// Return the place as text, for example "1ST" or "4TH"
const char *StandingsPlaceName(int place);

#endif
