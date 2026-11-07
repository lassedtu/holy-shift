#include "ui/text.h"

#include <stdbool.h>
#include <stddef.h>

#include "core/config.h"

#define MAX_SIZES 12

typedef struct LoadedFont
{
    int size;
    Font font;
    bool owned; // false for the default font of raylib, which is not unloaded
} LoadedFont;

static LoadedFont loaded[MAX_SIZES];
static int loaded_count;

// Return the font at a pixel size, or the default font of raylib when the file cannot be loaded
static Font FontAt(int size)
{
    for (int i = 0; i < loaded_count; i++)
    {
        if (loaded[i].size == size)
        {
            return loaded[i].font;
        }
    }
    if (loaded_count == MAX_SIZES)
    {
        return loaded[loaded_count - 1].font;
    }
    Font font = LoadFontEx(FONT_PATH, size, NULL, 0);
    bool owned = IsFontValid(font);
    if (owned)
    {
        SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
    }
    else
    {
        font = GetFontDefault();
    }
    loaded[loaded_count++] = (LoadedFont){size, font, owned};
    return font;
}

// Return the nearest size where each pixel of the font is a whole number of screen pixels
static int SharpSize(int size)
{
    int steps = (size + FONT_GRID / 2) / FONT_GRID;
    return (steps < 1 ? 1 : steps) * FONT_GRID;
}

void TextUnloadAll(void)
{
    for (int i = 0; i < loaded_count; i++)
    {
        if (loaded[i].owned)
        {
            UnloadFont(loaded[i].font);
        }
    }
    loaded_count = 0;
}

// The letters of the font have their own space, so no space is added between them
void TextDraw(const char *text, float x, float y, int size, Color color)
{
    size = SharpSize(size);
    DrawTextEx(FontAt(size), text, (Vector2){(float)(int)x, (float)(int)y}, (float)size, 0.0f, color);
}

int TextWidth(const char *text, int size)
{
    size = SharpSize(size);
    return (int)MeasureTextEx(FontAt(size), text, (float)size, 0.0f).x;
}

void TextDrawCentered(const char *text, float x, float y, int size, Color color)
{
    TextDraw(text, x - (float)TextWidth(text, size) / 2.0f, y, size, color);
}
