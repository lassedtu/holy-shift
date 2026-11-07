#ifndef UI_UI_H
#define UI_UI_H

#include <stdbool.h>
#include <stddef.h>

#include "raylib.h"

// The menus of the game, drawn with the Kenney Pixel UI Pack (assets/ui/)
// The positions and the sizes are in game pixels (VIEW_WIDTH x VIEW_HEIGHT)
// Each frame: UiBegin, then the items of the screen, then UiEnd
// The items are found again each frame in the same order, the item with the focus is lit
// The mouse and the keyboard both work: W and S or the arrows move the focus, Enter or Space selects,
// A and D or the arrows change a slider
typedef struct Ui
{
    Texture2D panel;
    Texture2D button;
    Texture2D button_focus;
    Texture2D button_pressed;
    Texture2D inlay;

    Rectangle area; // the area of the window for the game view
    float scale;    // window pixels for each game pixel
    int focus;
    int count;      // items in this frame
    int last_count; // items in the last frame
    bool mouse_moved;
    Vector2 mouse;  // in game pixels
    bool up;
    bool down;
    bool left;
    bool right;
    bool select;
    bool back; // Esc, the screen decides what it does
} Ui;

bool UiLoad(Ui *ui, char *message, size_t size);
void UiUnload(Ui *ui);

// Give the focus to the first item, for example when a new screen opens
void UiReset(Ui *ui);

void UiBegin(Ui *ui, Rectangle area);
void UiEnd(Ui *ui);

// Make the game view darker behind a menu, amount is from 0 to 1
void UiDim(const Ui *ui, float amount);

void UiPanel(const Ui *ui, Rectangle rect);
void UiText(const Ui *ui, const char *text, float x, float y, int size, Color color);
void UiTextCentered(const Ui *ui, const char *text, float x, float y, int size, Color color);

// Return true in the frame when the button is selected
bool UiButton(Ui *ui, Rectangle rect, const char *label);

// Return true when the value changes
bool UiSlider(Ui *ui, Rectangle rect, const char *label, float *value, float min, float max, float step,
              const char *format);
bool UiToggle(Ui *ui, Rectangle rect, const char *label, bool *value);

#endif
