#include "level/level_textures.h"

#include <stdio.h>

#include "core/config.h"

// The level texture is not made yet, so the visual texture is the logic texture
// Load LEVEL_TEXTURE_FILE here when the level texture exists
static bool LoadVisualTexture(LevelTextures *textures, const Level *level)
{
    (void)level;
    textures->visual = textures->logic;
    textures->visual_is_logic = true;
    return true;
}

bool LevelTexturesLoad(LevelTextures *textures, const Level *level, char *message, size_t size)
{
    *textures = (LevelTextures){0};

    char path[256];
    LevelFilePath(level, LEVEL_LOGIC_FILE, path, sizeof(path));
    textures->logic = LoadTexture(path);
    if (!IsTextureValid(textures->logic))
    {
        snprintf(message, size, "Could not load %s", path);
        return false;
    }
    if (textures->logic.width != level->map_width || textures->logic.height != level->map_height)
    {
        snprintf(message, size, "%s has size %dx%d, the level file says %dx%d", path,
                 textures->logic.width, textures->logic.height, level->map_width,
                 level->map_height);
        LevelTexturesUnload(textures);
        return false;
    }
    SetTextureFilter(textures->logic, TEXTURE_FILTER_POINT);
    SetTextureWrap(textures->logic, TEXTURE_WRAP_CLAMP);

    if (!LoadVisualTexture(textures, level))
    {
        snprintf(message, size, "Could not load the level texture of \"%s\"", level->name);
        LevelTexturesUnload(textures);
        return false;
    }
    return true;
}

void LevelTexturesUnload(LevelTextures *textures)
{
    if (!textures->visual_is_logic && IsTextureValid(textures->visual))
    {
        UnloadTexture(textures->visual);
    }
    if (IsTextureValid(textures->logic))
    {
        UnloadTexture(textures->logic);
    }
    *textures = (LevelTextures){0};
}
