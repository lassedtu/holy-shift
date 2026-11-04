#include "level/level.h"

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
    bool found = false;
    FilePathList files = LoadDirectoryFiles(LEVELS_DIR);
    for (unsigned int i = 0; i < files.count; i++)
    {
        const char *entry = GetFileName(files.paths[i]);
        if (!DirectoryExists(files.paths[i]) ||
            !FileExists(TextFormat("%s/%s", files.paths[i], LEVEL_FILE)))
        {
            continue;
        }
        if (!found || strcmp(entry, name) < 0)
        {
            snprintf(name, size, "%s", entry);
            found = true;
        }
    }
    UnloadDirectoryFiles(files);
    return found;
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
