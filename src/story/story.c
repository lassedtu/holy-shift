#include "story/story.h"

#include <stdio.h>
#include <string.h>

#include "core/config.h"
#include "raylib.h"

bool StoryLoad(Story *story, char *message, size_t size)
{
    *story = (Story){0};
    char *text = FileExists(STORY_ORDER_PATH) ? LoadFileText(STORY_ORDER_PATH) : NULL;
    if (text == NULL)
    {
        snprintf(message, size, "Could not read %s", STORY_ORDER_PATH);
        return false;
    }

    int number = 0;
    bool valid = true;
    char *line = text;
    while (line != NULL && valid)
    {
        char *next = strchr(line, '\n');
        if (next != NULL)
        {
            *next++ = '\0';
        }
        number++;
        char kind[16];
        char name[LEVEL_MAX_NAME];
        int found = sscanf(line, " %15s %63s", kind, name);
        if (found <= 0 || kind[0] == '#')
        {
            line = next;
            continue;
        }
        bool scene = found == 2 && strcmp(kind, "scene") == 0;
        bool stage = found == 2 && strcmp(kind, "stage") == 0;
        if ((!scene && !stage) || story->count == STORY_MAX_STEPS)
        {
            snprintf(message, size, "%s line %d is not valid", STORY_ORDER_PATH, number);
            valid = false;
        }
        else
        {
            StoryStep *step = &story->steps[story->count++];
            step->type = scene ? STORY_SCENE : STORY_STAGE;
            snprintf(step->name, sizeof(step->name), "%s", name);
        }
        line = next;
    }
    UnloadFileText(text);
    return valid;
}
