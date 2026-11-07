#ifndef STORY_SCENE_H
#define STORY_SCENE_H

#include <stdbool.h>
#include <stddef.h>

#define SCENE_MAX_NAME 64
#define SCENE_MAX_TEXT 512

// A scene of the story: pictures with lines of dialogue, from a .scene file in STORY_SCENES_DIR
//
// The file has one command on each line, and a line that starts with # is a comment:
//   image <file> [<part>]           show a picture from STORY_IMAGES_DIR, it fades in over the last picture
//                                   the part (top, bottom, left or right) is the part of a large picture that
//                                   shows, the middle when there is no part
//   pan <left|right|up|down> <s>    move the view over a picture that is larger than the screen, in <s> seconds
//   fade                            fade to black, the next picture fades in from black
//   wait <s>                        show the picture for <s> seconds without text
//   <Speaker>: <text>               a line of dialogue, with the portrait <speaker>.png from STORY_PORTRAITS_DIR
//   <Speaker> (<mood>): <text>      a line with the portrait <speaker>_<mood>.png, or <speaker>.png when it is missing
//   : <text>                        a line without a speaker, for example for a narrator
typedef enum SceneCommandType
{
    SCENE_IMAGE,
    SCENE_PAN,
    SCENE_FADE,
    SCENE_WAIT,
    SCENE_LINE
} SceneCommandType;

typedef enum ScenePan
{
    SCENE_PAN_LEFT,
    SCENE_PAN_RIGHT,
    SCENE_PAN_UP,
    SCENE_PAN_DOWN
} ScenePan;

typedef enum SceneAnchor
{
    SCENE_ANCHOR_MIDDLE,
    SCENE_ANCHOR_TOP,
    SCENE_ANCHOR_BOTTOM,
    SCENE_ANCHOR_LEFT,
    SCENE_ANCHOR_RIGHT
} SceneAnchor;

typedef struct SceneCommand
{
    SceneCommandType type;
    char name[SCENE_MAX_NAME];     // the file of a picture, or the portrait of a line ("" when there is no speaker)
    char fallback[SCENE_MAX_NAME]; // the portrait of a line when the portrait of the mood is missing
    char text[SCENE_MAX_TEXT];
    ScenePan pan;
    SceneAnchor anchor;
    float seconds;
} SceneCommand;

typedef struct Scene
{
    SceneCommand *commands;
    int count;
} Scene;

// Read a scene by its name (the file name without .scene), write the reason in message when it fails
bool SceneLoad(Scene *scene, const char *name, char *message, size_t size);
void SceneUnload(Scene *scene);

#endif
