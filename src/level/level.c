#include "level/level.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/config.h"
#include "util/json_lite.h"

void LevelFilePath(const Level *level, const char *file, char *path, size_t size)
{
    snprintf(path, size, "%s/%s/%s", LEVELS_DIR, level->name, file);
}

void LevelListNames(char *list, size_t size)
{
    list[0] = '\0';
    FilePathList files = LoadDirectoryFiles(LEVELS_DIR);
    for (unsigned int i = 0; i < files.count; i++)
    {
        if (!DirectoryExists(files.paths[i]))
        {
            continue;
        }
        const char *entry = GetFileName(files.paths[i]);
        if (strlen(list) + strlen(entry) + 3 >= size)
        {
            break;
        }
        if (list[0] != '\0')
        {
            strcat(list, ", ");
        }
        strcat(list, entry);
    }
    UnloadDirectoryFiles(files);
}

bool LevelFindFirst(char *name, size_t size)
{
    char all[LEVEL_MAX_ORDERED][LEVEL_MAX_NAME];
    if (LevelFindAll(all, LEVEL_MAX_ORDERED) == 0)
    {
        return false;
    }
    snprintf(name, size, "%s", all[0]);
    return true;
}

void LevelReadDisplayName(const char *name, char *display_name, int size)
{
    snprintf(display_name, (size_t)size, "%s", name);
    const char *path = TextFormat("%s/%s/%s", LEVELS_DIR, name, LEVEL_FILE);
    char *text = FileExists(path) ? LoadFileText(path) : NULL;
    if (text == NULL)
    {
        return;
    }
    char found[LEVEL_MAX_NAME];
    if (JsonReadString(text, "display_name", found, sizeof(found)) && found[0] != '\0')
    {
        snprintf(display_name, (size_t)size, "%s", found);
    }
    UnloadFileText(text);
}

// Move the levels in LEVEL_ORDER_PATH to the front of the list, in the order of the file
// Each line of the file is the folder name of a level, a line that starts with # is a comment
static void ApplyOrder(char names[][LEVEL_MAX_NAME], int count)
{
    char *text = FileExists(LEVEL_ORDER_PATH) ? LoadFileText(LEVEL_ORDER_PATH) : NULL;
    if (text == NULL)
    {
        return;
    }
    int placed = 0;
    char *line = text;
    while (line != NULL && *line != '\0' && placed < count)
    {
        char *next = strchr(line, '\n');
        if (next != NULL)
        {
            *next++ = '\0';
        }
        char entry[LEVEL_MAX_NAME];
        if (line[0] != '#' && sscanf(line, "%63s", entry) == 1)
        {
            for (int i = placed; i < count; i++)
            {
                if (strcmp(names[i], entry) == 0)
                {
                    // Move the level to the next place and keep the order of the levels between
                    char moved[LEVEL_MAX_NAME];
                    snprintf(moved, sizeof(moved), "%s", names[i]);
                    for (int k = i; k > placed; k--)
                    {
                        snprintf(names[k], LEVEL_MAX_NAME, "%s", names[k - 1]);
                    }
                    snprintf(names[placed++], LEVEL_MAX_NAME, "%s", moved);
                    break;
                }
            }
        }
        line = next;
    }
    UnloadFileText(text);
}

int LevelFindAll(char names[][LEVEL_MAX_NAME], int max)
{
    int count = 0;
    FilePathList files = LoadDirectoryFiles(LEVELS_DIR);
    for (unsigned int i = 0; i < files.count && count < max; i++)
    {
        if (!DirectoryExists(files.paths[i]) || !FileExists(TextFormat("%s/%s", files.paths[i], LEVEL_FILE)))
        {
            continue;
        }
        // Put the name in alphabetical order among the names found before
        const char *entry = GetFileName(files.paths[i]);
        int at = count;
        while (at > 0 && strcmp(names[at - 1], entry) > 0)
        {
            snprintf(names[at], LEVEL_MAX_NAME, "%s", names[at - 1]);
            at--;
        }
        snprintf(names[at], LEVEL_MAX_NAME, "%s", entry);
        count++;
    }
    UnloadDirectoryFiles(files);
    ApplyOrder(names, count);
    return count;
}

void LevelApplyDifficulty(Level *level, Difficulty difficulty)
{
    level->road_width = (int)lroundf((float)level->road_width * DIFFICULTY_RULES[difficulty].road_width);
}

int LevelTreeVariants(const Level *level, char names[TREE_MAX_VARIANTS][TREE_VARIANT_NAME_MAX])
{
    int count = 0;
    for (int species = 0; species < TREE_SPECIES_COUNT; species++)
    {
        for (int variant = 1; level->trees[species] && variant <= TREE_VARIANTS_PER_SPECIES; variant++)
        {
            snprintf(names[count++], TREE_VARIANT_NAME_MAX, "%s_v%d", TREE_SPECIES_NAMES[species], variant);
        }
    }
    return count;
}

// Read the species of trees, a level without a list of trees has the default species
static void ReadTrees(const char *text, Level *level)
{
    bool found = false;
    if (!JsonArrayHasString(text, "trees", TREE_SPECIES_NAMES[0], &found))
    {
        level->trees[TREE_SPECIES_DEFAULT] = true;
        return;
    }
    for (int species = 0; species < TREE_SPECIES_COUNT; species++)
    {
        JsonArrayHasString(text, "trees", TREE_SPECIES_NAMES[species], &level->trees[species]);
    }
}

// Read the list of points, return false if a point is not valid
static bool ReadPoints(const char *text, Level *level)
{
    const char *position = JsonFindKey(text, "points");
    position = position == NULL ? NULL : strchr(position, '[');
    if (position == NULL)
    {
        return false;
    }
    position++;

    for (;;)
    {
        position = JsonSkip(position, ",");
        if (*position != '[')
        {
            return true;
        }
        char *end = NULL;
        float x = strtof(position + 1, &end);
        const char *after_x = JsonSkip(end, ",");
        float y = strtof(after_x, &end);
        if (after_x == end || *JsonSkip(end, "") != ']' || level->count >= LEVEL_MAX_POINTS)
        {
            return false;
        }
        level->points[level->count++] = (Vector2){x, y};
        position = JsonSkip(end, "") + 1;
    }
}

bool LevelLoad(Level *level, const char *name, char *message, size_t size)
{
    if (strlen(name) >= LEVEL_MAX_NAME)
    {
        snprintf(message, size, "The level name \"%s\" is too long", name);
        return false;
    }

    Level loaded = {0};
    snprintf(loaded.name, sizeof(loaded.name), "%s", name);
    char path[256];
    LevelFilePath(&loaded, LEVEL_FILE, path, sizeof(path));

    char *text = FileExists(path) ? LoadFileText(path) : NULL;
    if (text == NULL)
    {
        char list[160];
        LevelListNames(list, sizeof(list));
        snprintf(message, size, "Level \"%s\" not found, saved levels: %s", name,
                 list[0] != '\0' ? list : "none");
        return false;
    }

    double map_width = 0;
    double map_height = 0;
    double road_width = 0;
    bool valid = JsonReadNumber(text, "map_width", &map_width) &&
                 JsonReadNumber(text, "map_height", &map_height) &&
                 JsonReadNumber(text, "road_width", &road_width);
    if (!valid)
    {
        snprintf(message, size, "%s is not a valid level file", path);
        UnloadFileText(text);
        return false;
    }
    loaded.map_width = (int)map_width;
    loaded.map_height = (int)map_height;
    loaded.road_width = (int)road_width;

    // A level without the closed key has an open curve
    JsonReadBool(text, "closed", &loaded.closed);
    JsonReadBool(text, "tutorial", &loaded.tutorial);
    if (!JsonReadString(text, "display_name", loaded.display_name, LEVEL_MAX_NAME) || loaded.display_name[0] == '\0')
    {
        snprintf(loaded.display_name, sizeof(loaded.display_name), "%s", name);
    }
    JsonReadBool(text, "jelka_just_ahead", &loaded.jelka_just_ahead);
    char theme[32];
    loaded.ground_theme = JsonReadString(text, "theme", theme, sizeof(theme)) ? GroundThemeFind(theme) : GROUND_FOREST;
    char time[32];
    loaded.time_of_day = JsonReadString(text, "time_of_day", time, sizeof(time)) ? TimeOfDayFind(time) : TIME_DAY;
    JsonReadString(text, "music", loaded.music, sizeof(loaded.music));
    for (int i = 0; i < MAP_EVENT_TYPE_COUNT; i++)
    {
        JsonArrayHasString(text, "map_events", MAP_EVENT_NAMES[i], &loaded.map_events[i]);
    }
    for (int d = 0; d < DIFFICULTY_COUNT; d++)
    {
        int times = JsonReadNumberArray(text, DIFFICULTY_RULES[d].times_key, loaded.times[d], LEVEL_RIVALS);
        loaded.time_count[d] = times > 0 ? times : 0;
    }

    loaded.road_color = COLOR_ROAD;
    loaded.outside_color = COLOR_OUTSIDE;
    loaded.water_color = COLOR_WATER;
    float color[3];
    if (JsonReadNumberArray(text, "road", color, 3) == 3)
    {
        loaded.road_color = (Color){(unsigned char)color[0], (unsigned char)color[1], (unsigned char)color[2], 255};
    }
    if (JsonReadNumberArray(text, "water", color, 3) == 3)
    {
        loaded.water_color = (Color){(unsigned char)color[0], (unsigned char)color[1], (unsigned char)color[2], 255};
    }
    if (JsonReadNumberArray(text, "outside", color, 3) == 3)
    {
        loaded.outside_color = (Color){(unsigned char)color[0], (unsigned char)color[1], (unsigned char)color[2], 255};
    }

    ReadTrees(text, &loaded);

    if (!ReadPoints(text, &loaded))
    {
        snprintf(message, size, "%s has no points or an invalid point", path);
        UnloadFileText(text);
        return false;
    }
    UnloadFileText(text);

    if (loaded.count < 2)
    {
        snprintf(message, size, "%s needs at least 2 points", path);
        return false;
    }
    if (loaded.count < 3)
    {
        loaded.closed = false;
    }
    *level = loaded;
    return true;
}
