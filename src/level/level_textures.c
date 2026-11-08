#include "level/level_textures.h"

#include <stdio.h>

#include "core/config.h"
#include "level/ground.h"

static void SetPixelLook(Texture2D texture)
{
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
}

// Use the level texture when the folder has one, so a texture painted by hand takes the place of the painted ground
static bool LoadVisualTexture(LevelTextures *textures, const Level *level, const Image *logic,
                              const Forest *forest)
{
    char path[256];
    LevelFilePath(level, LEVEL_TEXTURE_FILE, path, sizeof(path));
    if (FileExists(path))
    {
        textures->visual = LoadTexture(path);
    }
    else
    {
        Image ground = GroundPaint(logic, level, forest);
        if (ground.data == NULL)
        {
            return false;
        }
        textures->visual = LoadTextureFromImage(ground);
        UnloadImage(ground);
    }
    if (!IsTextureValid(textures->visual))
    {
        return false;
    }
    SetPixelLook(textures->visual);
    return true;
}

bool LevelTexturesLoad(LevelTextures *textures, const Level *level, const Image *logic, const Forest *forest,
                       char *message, size_t size)
{
    *textures = (LevelTextures){0};
    textures->logic = LoadTextureFromImage(*logic);
    if (!IsTextureValid(textures->logic))
    {
        snprintf(message, size, "Could not make the logic texture of \"%s\"", level->name);
        return false;
    }
    SetPixelLook(textures->logic);
    if (!LoadVisualTexture(textures, level, logic, forest))
    {
        snprintf(message, size, "Could not make the level texture of \"%s\"", level->name);
        LevelTexturesUnload(textures);
        return false;
    }
    return true;
}

void LevelTexturesUnload(LevelTextures *textures)
{
    if (IsTextureValid(textures->visual))
    {
        UnloadTexture(textures->visual);
    }
    if (IsTextureValid(textures->logic))
    {
        UnloadTexture(textures->logic);
    }
    *textures = (LevelTextures){0};
}
