#ifndef STORY_STORY_H
#define STORY_STORY_H

#include <stdbool.h>
#include <stddef.h>

#include "level/level.h"

#define STORY_MAX_STEPS 64

// The order of the story mode, from STORY_ORDER_PATH
//
// The file has one step on each line, and a line that starts with # is a comment:
//   scene <name>   play the scene <name>.scene
//   stage <name>   drive the level <name>
typedef enum StoryStepType
{
    STORY_SCENE,
    STORY_STAGE
} StoryStepType;

typedef struct StoryStep
{
    StoryStepType type;
    char name[LEVEL_MAX_NAME];
} StoryStep;

typedef struct Story
{
    StoryStep steps[STORY_MAX_STEPS];
    int count;
} Story;

// Read the order, write the reason in message when it fails
bool StoryLoad(Story *story, char *message, size_t size);

#endif
