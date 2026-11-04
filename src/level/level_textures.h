#ifndef LEVEL_LEVEL_TEXTURES_H
#define LEVEL_LEVEL_TEXTURES_H

#include <stdbool.h>
#include <stddef.h>

#include "level/level.h"
#include "raylib.h"

// The two images of a level
// The logic texture is for the rules of the game (road and ground)
// The visual texture is what the player sees on the ground
typedef struct LevelTextures
{
    Texture2D logic;
    Texture2D visual;
    bool visual_is_logic; // true while the visual texture is the logic texture
} LevelTextures;

// Load the textures of a level, write the reason in message when it fails
bool LevelTexturesLoad(LevelTextures *textures, const Level *level, char *message, size_t size);
void LevelTexturesUnload(LevelTextures *textures);

#endif
