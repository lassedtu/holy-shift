#ifndef UI_TEXT_H
#define UI_TEXT_H

#include "raylib.h"

// The text of the game, in the font of the game (FONT_PATH)
// A size is changed to the nearest multiple of FONT_GRID, so each pixel of the font is a whole number of screen pixels
// Each size is loaded the first time it is used
// The window must be open before the first text is drawn

// Release all the loaded sizes of the font
void TextUnloadAll(void);

// Return the size that a text of a size is drawn with
int TextSharpSize(int size);

void TextDraw(const char *text, float x, float y, int size, Color color);
int TextWidth(const char *text, int size);

// Draw a text with its centre at x
void TextDrawCentered(const char *text, float x, float y, int size, Color color);

#endif
