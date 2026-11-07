#include "story/scene.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/config.h"
#include "raylib.h"

#define MAX_COMMANDS 512

static char *Trim(char *text)
{
    while (isspace((unsigned char)*text))
    {
        text++;
    }
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
    {
        *--end = '\0';
    }
    return text;
}

static void Lowercase(char *text)
{
    for (; *text != '\0'; text++)
    {
        *text = (char)tolower((unsigned char)*text);
    }
}

// Read "<Speaker> (<mood>): <text>", "<Speaker>: <text>" or ": <text>"
static bool ParseLine(SceneCommand *command, char *line)
{
    char *colon = strchr(line, ':');
    if (colon == NULL)
    {
        return false;
    }
    *colon = '\0';
    command->type = SCENE_LINE;
    snprintf(command->text, sizeof(command->text), "%s", Trim(colon + 1));

    char *speaker = Trim(line);
    char *open = strchr(speaker, '(');
    char *mood = NULL;
    if (open != NULL)
    {
        char *close = strchr(open, ')');
        if (close == NULL)
        {
            return false;
        }
        *open = '\0';
        *close = '\0';
        mood = Trim(open + 1);
        speaker = Trim(speaker);
    }
    Lowercase(speaker);
    snprintf(command->fallback, sizeof(command->fallback), "%s", speaker);
    if (mood != NULL && *mood != '\0')
    {
        Lowercase(mood);
        snprintf(command->name, sizeof(command->name), "%s_%s", speaker, mood);
    }
    else
    {
        snprintf(command->name, sizeof(command->name), "%s", speaker);
    }
    return true;
}

static bool ParsePan(SceneCommand *command, const char *arguments)
{
    static const char *const names[] = {"left", "right", "up", "down"};
    char direction[16];
    if (sscanf(arguments, "%15s %f", direction, &command->seconds) != 2 || command->seconds <= 0.0f)
    {
        return false;
    }
    for (int i = 0; i < 4; i++)
    {
        if (strcmp(direction, names[i]) == 0)
        {
            command->type = SCENE_PAN;
            command->pan = (ScenePan)i;
            return true;
        }
    }
    return false;
}

// Read "<file>" or "<file> <part>"
static bool ParseImage(SceneCommand *command, const char *arguments)
{
    static const char *const parts[] = {"middle", "top", "bottom", "left", "right"};
    char part[16] = "middle";
    command->type = SCENE_IMAGE;
    int found = sscanf(arguments, "%63s %15s", command->name, part);
    if (found < 1)
    {
        return false;
    }
    for (int i = 0; i < 5; i++)
    {
        if (strcmp(part, parts[i]) == 0)
        {
            command->anchor = (SceneAnchor)i;
            return true;
        }
    }
    return false;
}

// Read one line of the file, return false when it is not valid
static bool ParseCommand(SceneCommand *command, char *line)
{
    *command = (SceneCommand){0};
    char word[16] = "";
    int used = 0;
    sscanf(line, "%15s%n", word, &used);
    char *arguments = Trim(line + used);

    if (strcmp(word, "image") == 0)
    {
        return ParseImage(command, arguments);
    }
    if (strcmp(word, "pan") == 0)
    {
        return ParsePan(command, arguments);
    }
    if (strcmp(word, "fade") == 0)
    {
        command->type = SCENE_FADE;
        return true;
    }
    if (strcmp(word, "wait") == 0)
    {
        command->type = SCENE_WAIT;
        return sscanf(arguments, "%f", &command->seconds) == 1 && command->seconds > 0.0f;
    }
    return ParseLine(command, line);
}

bool SceneLoad(Scene *scene, const char *name, char *message, size_t size)
{
    *scene = (Scene){0};
    const char *path = TextFormat("%s/%s.scene", STORY_SCENES_DIR, name);
    char *text = FileExists(path) ? LoadFileText(path) : NULL;
    if (text == NULL)
    {
        snprintf(message, size, "Could not read %s", path);
        return false;
    }
    scene->commands = calloc(MAX_COMMANDS, sizeof(SceneCommand));
    if (scene->commands == NULL)
    {
        UnloadFileText(text);
        snprintf(message, size, "Not enough memory for %s", path);
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
        char *content = Trim(line);
        if (*content != '\0' && *content != '#')
        {
            if (scene->count == MAX_COMMANDS || !ParseCommand(&scene->commands[scene->count], content))
            {
                snprintf(message, size, "%s line %d is not valid", path, number);
                valid = false;
            }
            scene->count++;
        }
        line = next;
    }
    UnloadFileText(text);
    if (!valid)
    {
        SceneUnload(scene);
    }
    return valid;
}

void SceneUnload(Scene *scene)
{
    free(scene->commands);
    *scene = (Scene){0};
}
