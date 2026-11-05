#include "util/json_lite.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *JsonFindKey(const char *text, const char *key)
{
    char quoted[64];
    snprintf(quoted, sizeof(quoted), "\"%s\"", key);
    const char *found = strstr(text, quoted);
    if (found == NULL)
    {
        return NULL;
    }
    found = strchr(found, ':');
    return found == NULL ? NULL : found + 1;
}

bool JsonReadNumber(const char *text, const char *key, double *value)
{
    const char *position = JsonFindKey(text, key);
    if (position == NULL)
    {
        return false;
    }
    char *end = NULL;
    *value = strtod(position, &end);
    return end != position;
}

bool JsonReadBool(const char *text, const char *key, bool *value)
{
    const char *position = JsonFindKey(text, key);
    if (position == NULL)
    {
        return false;
    }
    position = JsonSkip(position, "");
    if (strncmp(position, "true", 4) == 0)
    {
        *value = true;
        return true;
    }
    if (strncmp(position, "false", 5) == 0)
    {
        *value = false;
        return true;
    }
    return false;
}

int JsonReadNumberArray(const char *text, const char *key, float *values, int max_count)
{
    const char *position = JsonFindKey(text, key);
    position = position == NULL ? NULL : strchr(position, '[');
    if (position == NULL)
    {
        return -1;
    }
    position++;

    int count = 0;
    for (;;)
    {
        position = JsonSkip(position, ",");
        if (*position == ']')
        {
            return count;
        }
        char *end = NULL;
        float value = strtof(position, &end);
        if (end == position || count >= max_count)
        {
            return -1;
        }
        values[count++] = value;
        position = end;
    }
}

bool JsonArrayHasString(const char *text, const char *key, const char *value, bool *found)
{
    const char *position = JsonFindKey(text, key);
    position = position == NULL ? NULL : strchr(position, '[');
    if (position == NULL)
    {
        return false;
    }
    position++;

    *found = false;
    for (;;)
    {
        position = JsonSkip(position, ",");
        if (*position != '"')
        {
            return *position == ']';
        }
        position++;
        const char *end = strchr(position, '"');
        if (end == NULL)
        {
            return false;
        }
        if (strlen(value) == (size_t)(end - position) && strncmp(position, value, (size_t)(end - position)) == 0)
        {
            *found = true;
        }
        position = end + 1;
    }
}

const char *JsonSkip(const char *position, const char *skip)
{
    while (*position != '\0' &&
           (isspace((unsigned char)*position) || strchr(skip, *position) != NULL))
    {
        position++;
    }
    return position;
}
