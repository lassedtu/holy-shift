#ifndef LEVEL_LEVEL_TEXTURES_H
#define LEVEL_LEVEL_TEXTURES_H

#include <stdbool.h>
#include <stddef.h>

#include "level/forest.h"
#include "level/level.h"
#include "raylib.h"

// The two images of a level
// The logic texture is for the rules of the game (road and ground)
// The visual texture is what the player sees on the ground
typedef struct LevelTextures
{
    Texture2D logic;
    Texture2D visual;
} LevelTextures;

// Load the textures of a level, write the reason in message when it fails
// The visual texture is LEVEL_TEXTURE_FILE when the level has one, else it is painted from the logic map
// The forest puts leaves under the trees of a painted texture, it can be NULL
// The logic map comes from LogicMapLoad
bool LevelTexturesLoad(LevelTextures *textures, const Level *level, const Image *logic, const Forest *forest,
                       char *message, size_t size);
void LevelTexturesUnload(LevelTextures *textures);

#endif
