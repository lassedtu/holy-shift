#ifndef STORY_SCENE_PLAYER_H
#define STORY_SCENE_PLAYER_H

#include <stdbool.h>
#include <stddef.h>

#include "game/standings.h"
#include "level/level.h"
#include "raylib.h"
#include "story/scene.h"
#include "ui/ui.h"

#define SCENE_MAX_TEXTURES 64
#define SCENE_PICTURE_NONE -1
#define SCENE_PICTURE_BOARD -2 // the results board in place of a picture

// A picture of a scene on the screen
typedef struct ScenePicture
{
    int texture;    // index in the textures of the player, SCENE_PICTURE_NONE or SCENE_PICTURE_BOARD
    Vector2 offset; // the top left corner of the part of the picture on the screen, in picture pixels
    Vector2 pan_from;
    Vector2 pan_to;
    float pan_time;
    float pan_seconds; // 0 when the picture does not move
} ScenePicture;

// Plays a scene: the pictures fill a wide area at the top at STORY_PICTURE_SCALE, the lines are typed in a box
// in a dark area below the picture, so the box never covers the picture
// A new picture shows alone for a moment, then the box slides in
// A click, Space or Enter shows the full line, and then goes to the next line. Esc skips the scene
typedef struct ScenePlayer
{
    Scene scene;
    char names[SCENE_MAX_TEXTURES][SCENE_MAX_NAME];
    Texture2D textures[SCENE_MAX_TEXTURES];
    int texture_count;
    int *portraits; // the portrait texture of each command, -1 when there is none

    int index;    // the command that is running
    bool started; // false until the command at index has started
    ScenePicture current;
    ScenePicture previous;
    float cross; // from 0 to 1 while the current picture fades in over the previous picture
    float black; // from 0 (no black) to 1 (all black)
    float black_target;
    float typed; // letters of the line that are shown
    float alone; // seconds left that a new picture shows without the dialogue box
    float box_in; // from 0 (below the screen) to 1 (in its place) while the dialogue box slides in
    float wait;  // seconds left of a wait command
    float time;
    bool done;
    char music[SCENE_MAX_NAME]; // the music file that the scene plays now, "" for no music
    Standings board; // the standings that a board command shows
    char board_title[LEVEL_MAX_NAME];
    bool has_board;
} ScenePlayer;

// Load a scene and its pictures, write the reason in message when it fails
bool ScenePlayerStart(ScenePlayer *player, const char *name, char *message, size_t size);
void ScenePlayerStop(ScenePlayer *player);

// Set the standings and the name of the level that the board command shows
// Without them, the board shows no rows
void ScenePlayerSetBoard(ScenePlayer *player, const Standings *standings, const char *title);

// advance is true in the frame when the player clicks or presses Space or Enter, skip when the player presses Esc
void ScenePlayerUpdate(ScenePlayer *player, float dt, bool advance, bool skip);
void ScenePlayerDraw(const ScenePlayer *player, const Ui *ui);

bool ScenePlayerDone(const ScenePlayer *player);

#endif
