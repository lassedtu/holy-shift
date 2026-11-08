#include "game/standings.h"

#include "core/config.h"

#define PLAYER_NAME "Rajka"
#define JELKA_LEAD 1.0f      // seconds that Jelka is ahead on a level with jelka_just_ahead
#define DEFAULT_SPEED 15.0f  // meters each second of the fastest other driver, when the level has no times

// The other drivers, in the order of the times in level.json. Jelka must be the first
static const char *const RIVALS[LEVEL_RIVALS] = {"Jelka", "Dino", "Vesna", "Tomo", "Zdenka", "Marko", "Ivo"};

// The times of the other drivers when the level has no times, as parts of the time of the fastest driver
static const float DEFAULT_FACTORS[LEVEL_RIVALS] = {1.00f, 1.04f, 1.08f, 1.12f, 1.17f, 1.23f, 1.30f};

// Return the time of one of the other drivers
static float RivalTime(const Level *level, float track_length, Difficulty difficulty, int i)
{
    if (i < level->time_count[difficulty])
    {
        return level->times[difficulty][i];
    }
    float scale = DIFFICULTY_RULES[difficulty].rival_time;
    if (i < level->time_count[DIFFICULTY_MEDIUM])
    {
        return level->times[DIFFICULTY_MEDIUM][i] * scale;
    }
    return track_length / MAP_PIXELS_PER_METER / DEFAULT_SPEED * DEFAULT_FACTORS[i] * scale;
}

void StandingsMake(Standings *standings, const Level *level, float track_length, float player_time,
                   Difficulty difficulty)
{
    int count = 0;
    for (int i = 0; i < LEVEL_RIVALS; i++)
    {
        float time = RivalTime(level, track_length, difficulty, i);
        if (i == 0 && level->jelka_just_ahead)
        {
            time = player_time - JELKA_LEAD;
        }
        standings->rows[count++] = (StandingsRow){RIVALS[i], time, false};
    }
    standings->rows[count++] = (StandingsRow){PLAYER_NAME, player_time, true};

    // Sort from the fastest to the slowest, the player is after a driver with the same time
    for (int i = 1; i < count; i++)
    {
        StandingsRow row = standings->rows[i];
        int j = i;
        while (j > 0 && (standings->rows[j - 1].time > row.time ||
                         (standings->rows[j - 1].time == row.time && standings->rows[j - 1].player)))
        {
            standings->rows[j] = standings->rows[j - 1];
            j--;
        }
        standings->rows[j] = row;
    }
    for (int i = 0; i < count; i++)
    {
        if (standings->rows[i].player)
        {
            standings->place = i + 1;
        }
    }
}

const char *StandingsMedal(int place)
{
    static const char *const medals[] = {"GOLD", "SILVER", "BRONZE"};
    return place >= 1 && place <= 3 ? medals[place - 1] : NULL;
}

const char *StandingsPlaceName(int place)
{
    static const char *const names[] = {"1ST", "2ND", "3RD", "4TH", "5TH", "6TH", "7TH", "8TH"};
    return place >= 1 && place <= STANDINGS_DRIVERS ? names[place - 1] : "";
}
