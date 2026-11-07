#include "ui/ui.h"

#include <math.h>
#include <stdio.h>

#include "core/config.h"
#include "ui/text.h"

#define SLICE 16 // size of a corner of a 9-slice image, the images are 3 x 3 slices
#define TEXT_SIZE 15
#define COLOR_LABEL ((Color){60, 40, 24, 255})
#define COLOR_LABEL_LIGHT ((Color){250, 236, 200, 255})

static bool LoadImageTexture(Texture2D *texture, const char *name, char *message, size_t size)
{
    const char *path = TextFormat("%s/%s", UI_DIR, name);
    *texture = LoadTexture(path);
    if (!IsTextureValid(*texture))
    {
        snprintf(message, size, "Could not load %s", path);
        return false;
    }
    SetTextureFilter(*texture, TEXTURE_FILTER_POINT);
    return true;
}

bool UiLoad(Ui *ui, char *message, size_t size)
{
    *ui = (Ui){0};
    bool loaded = LoadImageTexture(&ui->panel, "panel.png", message, size) &&
                  LoadImageTexture(&ui->button, "button.png", message, size) &&
                  LoadImageTexture(&ui->button_focus, "button_focus.png", message, size) &&
                  LoadImageTexture(&ui->button_pressed, "button_pressed.png", message, size) &&
                  LoadImageTexture(&ui->inlay, "inlay.png", message, size);
    if (!loaded)
    {
        UiUnload(ui);
    }
    return loaded;
}

void UiUnload(Ui *ui)
{
    Texture2D *textures[] = {&ui->panel, &ui->button, &ui->button_focus, &ui->button_pressed, &ui->inlay};
    for (size_t i = 0; i < sizeof(textures) / sizeof(textures[0]); i++)
    {
        if (IsTextureValid(*textures[i]))
        {
            UnloadTexture(*textures[i]);
        }
    }
    *ui = (Ui){0};
}

void UiReset(Ui *ui)
{
    ui->focus = 0;
}

void UiBegin(Ui *ui, Rectangle area)
{
    ui->area = area;
    ui->scale = area.height / VIEW_HEIGHT;
    Vector2 mouse = GetMousePosition();
    ui->mouse = (Vector2){(mouse.x - area.x) / ui->scale, (mouse.y - area.y) / ui->scale};
    Vector2 delta = GetMouseDelta();
    ui->mouse_moved = delta.x != 0.0f || delta.y != 0.0f;
    ui->up = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
    ui->down = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);
    ui->left = IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT);
    ui->right = IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT);
    ui->select = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    ui->back = IsKeyPressed(KEY_ESCAPE);

    if (ui->last_count > 0)
    {
        if (ui->up)
        {
            ui->focus = (ui->focus + ui->last_count - 1) % ui->last_count;
        }
        if (ui->down)
        {
            ui->focus = (ui->focus + 1) % ui->last_count;
        }
    }
    ui->count = 0;
}

void UiEnd(Ui *ui)
{
    ui->last_count = ui->count;
    if (ui->count > 0 && ui->focus >= ui->count)
    {
        ui->focus = ui->count - 1;
    }
}

static Rectangle ToWindow(const Ui *ui, Rectangle rect)
{
    return (Rectangle){ui->area.x + rect.x * ui->scale, ui->area.y + rect.y * ui->scale, rect.width * ui->scale,
                       rect.height * ui->scale};
}

void UiDim(const Ui *ui, float amount)
{
    DrawRectangleRec(ui->area, Fade(BLACK, amount));
}

// Draw one part of a 9-slice image, repeated over an area so the pixels are not stretched
static void DrawTiled(const Ui *ui, Texture2D texture, Rectangle source, Rectangle area)
{
    for (float y = 0.0f; y < area.height; y += source.height)
    {
        float h = fminf(source.height, area.height - y);
        for (float x = 0.0f; x < area.width; x += source.width)
        {
            float w = fminf(source.width, area.width - x);
            Rectangle part = {source.x, source.y, w, h};
            Rectangle target = ToWindow(ui, (Rectangle){area.x + x, area.y + y, w, h});
            DrawTexturePro(texture, part, target, (Vector2){0, 0}, 0.0f, WHITE);
        }
    }
}

static void DrawNineSlice(const Ui *ui, Texture2D texture, Rectangle rect)
{
    rect = (Rectangle){floorf(rect.x), floorf(rect.y), floorf(rect.width), floorf(rect.height)};
    float s = SLICE;
    float inner_w = fmaxf(rect.width - 2.0f * s, 0.0f);
    float inner_h = fmaxf(rect.height - 2.0f * s, 0.0f);
    float xs[3] = {rect.x, rect.x + s, rect.x + s + inner_w};
    float ys[3] = {rect.y, rect.y + s, rect.y + s + inner_h};
    float ws[3] = {s, inner_w, s};
    float hs[3] = {s, inner_h, s};
    for (int row = 0; row < 3; row++)
    {
        for (int column = 0; column < 3; column++)
        {
            if (ws[column] <= 0.0f || hs[row] <= 0.0f)
            {
                continue;
            }
            Rectangle source = {(float)column * s, (float)row * s, s, s};
            DrawTiled(ui, texture, source, (Rectangle){xs[column], ys[row], ws[column], hs[row]});
        }
    }
}

void UiPanel(const Ui *ui, Rectangle rect)
{
    DrawNineSlice(ui, ui->panel, rect);
}

void UiInlay(const Ui *ui, Rectangle rect)
{
    DrawNineSlice(ui, ui->inlay, rect);
}

void UiPanelDark(const Ui *ui, Rectangle rect)
{
    DrawNineSlice(ui, ui->button, rect);
}

void UiFill(const Ui *ui, Rectangle rect, Color color)
{
    DrawRectangleRec(ToWindow(ui, rect), color);
}

void UiTexture(const Ui *ui, Texture2D texture, Rectangle source, Rectangle rect, Color tint)
{
    DrawTexturePro(texture, source, ToWindow(ui, rect), (Vector2){0, 0}, 0.0f, tint);
}

float UiTextHeight(const Ui *ui, int size)
{
    return (float)TextSharpSize((int)((float)size * ui->scale)) / ui->scale;
}

float UiTextWidth(const Ui *ui, const char *text, int size)
{
    return (float)TextWidth(text, (int)((float)size * ui->scale)) / ui->scale;
}

void UiText(const Ui *ui, const char *text, float x, float y, int size, Color color)
{
    TextDraw(text, ui->area.x + x * ui->scale, ui->area.y + y * ui->scale, (int)((float)size * ui->scale), color);
}

void UiTextCentered(const Ui *ui, const char *text, float x, float y, int size, Color color)
{
    TextDrawCentered(text, ui->area.x + x * ui->scale, ui->area.y + y * ui->scale,
                     (int)((float)size * ui->scale), color);
}

// Find the focus of the next item, return true when it has the focus
static bool Focus(Ui *ui, Rectangle rect)
{
    int index = ui->count++;
    if (ui->mouse_moved && CheckCollisionPointRec(ui->mouse, rect))
    {
        ui->focus = index;
    }
    return ui->focus == index;
}

static bool Clicked(const Ui *ui, Rectangle rect)
{
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(ui->mouse, rect);
}

static void DrawButtonBack(const Ui *ui, Rectangle rect, bool focused)
{
    bool pressed = focused && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(ui->mouse, rect);
    Texture2D texture = pressed ? ui->button_pressed : (focused ? ui->button_focus : ui->button);
    DrawNineSlice(ui, texture, rect);
}

static float TextTop(Rectangle rect)
{
    return rect.y + (rect.height - (float)TEXT_SIZE) / 2.0f - 1.0f;
}

bool UiButton(Ui *ui, Rectangle rect, const char *label)
{
    bool focused = Focus(ui, rect);
    DrawButtonBack(ui, rect, focused);
    UiTextCentered(ui, label, rect.x + rect.width / 2.0f, TextTop(rect), TEXT_SIZE,
                   focused ? COLOR_LABEL : COLOR_LABEL_LIGHT);
    return Clicked(ui, rect) || (focused && ui->select);
}

bool UiSlider(Ui *ui, Rectangle rect, const char *label, float *value, float min, float max, float step,
              const char *format)
{
    bool focused = Focus(ui, rect);
    float old = *value;
    Rectangle track = {rect.x + rect.width * 0.5f, rect.y + 6.0f, rect.width * 0.5f - 8.0f, rect.height - 12.0f};
    if (focused && ui->left)
    {
        *value -= step;
    }
    if (focused && ui->right)
    {
        *value += step;
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && focused && CheckCollisionPointRec(ui->mouse, rect) &&
        ui->mouse.x >= track.x - 4.0f)
    {
        float t = (ui->mouse.x - track.x) / track.width;
        *value = min + roundf(fminf(fmaxf(t, 0.0f), 1.0f) * (max - min) / step) * step;
    }
    *value = fminf(fmaxf(*value, min), max);

    DrawButtonBack(ui, rect, focused);
    Color ink = focused ? COLOR_LABEL : COLOR_LABEL_LIGHT;
    UiText(ui, label, rect.x + 10.0f, TextTop(rect), TEXT_SIZE, ink);
    float t = (*value - min) / (max - min);
    DrawRectangleRec(ToWindow(ui, (Rectangle){track.x, track.y + track.height / 2.0f - 2.0f, track.width, 4.0f}),
                     Fade(COLOR_LABEL, 0.6f));
    DrawRectangleRec(ToWindow(ui, (Rectangle){track.x + t * track.width - 3.0f, track.y, 6.0f, track.height}), ink);
    UiText(ui, TextFormat(format, *value), track.x - 34.0f, TextTop(rect), TEXT_SIZE, ink);
    return *value != old;
}

bool UiToggle(Ui *ui, Rectangle rect, const char *label, bool *value)
{
    bool focused = Focus(ui, rect);
    bool changed = Clicked(ui, rect) || (focused && (ui->select || ui->left || ui->right));
    if (changed)
    {
        *value = !*value;
    }
    DrawButtonBack(ui, rect, focused);
    Color ink = focused ? COLOR_LABEL : COLOR_LABEL_LIGHT;
    UiText(ui, label, rect.x + 10.0f, TextTop(rect), TEXT_SIZE, ink);
    const char *state = *value ? "ON" : "OFF";
    float width = UiTextWidth(ui, state, TEXT_SIZE);
    UiText(ui, state, rect.x + rect.width - 10.0f - width, TextTop(rect), TEXT_SIZE, ink);
    return changed;
}
