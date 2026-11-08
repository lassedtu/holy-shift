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

bool LevelTexturesLoad(LevelTextures *textures, const Level *level, const Forest *forest, char *message,
                       size_t size)
{
    *textures = (LevelTextures){0};

    char path[256];
    LevelFilePath(level, LEVEL_LOGIC_FILE, path, sizeof(path));
    Image logic = LoadImage(path);
    if (logic.data == NULL)
    {
        snprintf(message, size, "Could not load %s", path);
        return false;
    }
    if (logic.width != level->map_width || logic.height != level->map_height)
    {
        snprintf(message, size, "%s has size %dx%d, the level file says %dx%d", path, logic.width, logic.height,
                 level->map_width, level->map_height);
        UnloadImage(logic);
        return false;
    }
    textures->logic = LoadTextureFromImage(logic);
    if (!IsTextureValid(textures->logic))
    {
        snprintf(message, size, "Could not load %s", path);
        UnloadImage(logic);
        return false;
    }
    SetPixelLook(textures->logic);

    bool loaded = LoadVisualTexture(textures, level, &logic, forest);
    UnloadImage(logic);
    if (!loaded)
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
