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

// A sunken frame, for example around a picture
void UiInlay(const Ui *ui, Rectangle rect);

// A brown panel, for example behind a paper panel
void UiPanelDark(const Ui *ui, Rectangle rect);

void UiFill(const Ui *ui, Rectangle rect, Color color);

// A filled circle with a dark outline, the center and the radius are in game pixels
void UiCircle(const Ui *ui, Vector2 center, float radius, Color color, Color outline);

// Draw a part of a texture into a rectangle
void UiTexture(const Ui *ui, Texture2D texture, Rectangle source, Rectangle rect, Color tint);

// Return the width of a text in game pixels
float UiTextWidth(const Ui *ui, const char *text, int size);

// Return the height of a text in game pixels, as it is drawn
float UiTextHeight(const Ui *ui, int size);
void UiText(const Ui *ui, const char *text, float x, float y, int size, Color color);
void UiTextCentered(const Ui *ui, const char *text, float x, float y, int size, Color color);

// Return true in the frame when the button is selected
bool UiButton(Ui *ui, Rectangle rect, const char *label);

// Return true when the value changes
bool UiSlider(Ui *ui, Rectangle rect, const char *label, float *value, float min, float max, float step,
              const char *format);
bool UiToggle(Ui *ui, Rectangle rect, const char *label, bool *value);

#endif
