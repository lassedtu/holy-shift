// Level editor for the road map (raylib)
//
//   Mouse wheel or trackpad : scroll vertically and horizontally
//   Shift + mouse wheel     : scroll horizontally
//   Cmd + mouse wheel       : zoom at the cursor (Ctrl also works)
//   + and - keys            : zoom at the cursor
//   Left click              : add a point at the end of the curve
//   Left click on point 1   : close the curve (at least 3 points)
//   Right click             : delete the point under the cursor
//   Cmd+Z (or Ctrl+Z)       : undo
//   Name box                : click to type the level name
//   Export button           : save to levels/<name>/
//   Open button             : load levels/<name>/level.json for editing
//
// Run from the project root:
//   make -C level-builder run
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "raymath.h"

#define MAP_W 2048
#define MAP_H 2048
#define ROAD_WIDTH 48 // road width in map pixels
#define MAX_POINTS 512
#define MAX_UNDO 256
#define CURVE_STEP 2.0f // maximum distance between curve samples

#define TOOLBAR_H 40
#define SCROLL_SPEED 48.0f // screen pixels for each wheel unit
#define ZOOM_MIN 0.25f
#define ZOOM_MAX 4.0f
#define ZOOM_WHEEL_STEP 1.1f   // zoom factor for each wheel unit
#define ZOOM_KEY_STEP 1.25f    // zoom factor for each key press
#define ZOOM_LOCK_SECONDS 0.4f // time without scroll after a zoom
#define EDGE_MARGIN 200.0f     // screen pixels of map that stay in view
#define POINT_RADIUS 6.0f
#define PICK_RADIUS 12.0f // maximum distance to select a point, in screen pixels
#define MESSAGE_SECONDS 4.0f

#define LEVELS_DIR "levels"
#define MAX_NAME 32
#define JSON_NAME "level.json"
#define PNG_NAME "level_logic.png"

static const Color COLOR_OUTSIDE = {40, 150, 50, 255};
static const Color COLOR_ROAD = {130, 90, 50, 255};

typedef struct
{
    Vector2 points[MAX_POINTS];
    int count;
    bool closed; // the last point connects to the first point
} Level;

static Level undo_stack[MAX_UNDO];
static int undo_count = 0;

// Save the current level before a change
static void PushUndo(const Level *level)
{
    if (undo_count == MAX_UNDO)
    {
        memmove(undo_stack, undo_stack + 1, (MAX_UNDO - 1) * sizeof(Level));
        undo_count--;
    }
    undo_stack[undo_count++] = *level;
}

// Restore the last saved level, return false if there is none
static bool PopUndo(Level *level)
{
    if (undo_count == 0)
        return false;
    *level = undo_stack[--undo_count];
    return true;
}

// Return the number of curve samples between two points
static int SegmentSteps(Vector2 a, Vector2 b)
{
    int steps = (int)ceilf(Vector2Distance(a, b) / CURVE_STEP);
    return steps < 1 ? 1 : steps;
}

// Create the curve samples through all points with a Catmull-Rom spline
// A closed curve also has a segment from the last point to the first point
// The caller must free the result
static Vector2 *BuildCurve(const Level *level, int *out_count)
{
    *out_count = 0;
    if (level->count < 2)
        return NULL;

    int n = level->count;
    int segments = level->closed ? n : n - 1;
    int total = 1;
    for (int i = 0; i < segments; i++)
    {
        total += SegmentSteps(level->points[i], level->points[(i + 1) % n]);
    }

    Vector2 *curve = malloc((size_t)total * sizeof(Vector2));
    if (curve == NULL)
        return NULL;

    int k = 0;
    for (int i = 0; i < segments; i++)
    {
        int i0, i3;
        if (level->closed)
        {
            i0 = (i + n - 1) % n;
            i3 = (i + 2) % n;
        }
        else
        {
            i0 = i > 0 ? i - 1 : 0;
            i3 = i + 2 < n ? i + 2 : n - 1;
        }
        Vector2 p1 = level->points[i];
        Vector2 p2 = level->points[(i + 1) % n];
        int steps = SegmentSteps(p1, p2);
        for (int s = 0; s < steps; s++)
        {
            curve[k++] = GetSplinePointCatmullRom(level->points[i0], p1, p2,
                                                  level->points[i3],
                                                  (float)s / (float)steps);
        }
    }
    curve[k++] = level->closed ? level->points[0] : level->points[n - 1];
    *out_count = k;
    return curve;
}

// Draw the road around the curve into the logic map
static void RenderLogicMap(Image *image, const Vector2 *curve, int count)
{
    ImageClearBackground(image, COLOR_OUTSIDE);
    for (int i = 0; i < count; i++)
    {
        ImageDrawCircleV(image, curve[i], ROAD_WIDTH / 2, COLOR_ROAD);
    }
}

// Save the level file and the logic map in levels/<name>/
// Write the result in message
static bool ExportLevel(const Level *level, const char *name, char *message,
                        size_t size)
{
    if (name[0] == '\0')
    {
        snprintf(message, size, "Enter a level name before export");
        return false;
    }
    if (level->count < 2)
    {
        snprintf(message, size, "Add at least 2 points before export");
        return false;
    }

    const char *directory = TextFormat("%s/%s", LEVELS_DIR, name);
    if (!DirectoryExists(directory) && MakeDirectory(directory) != 0)
    {
        snprintf(message, size, "Could not create %s", directory);
        return false;
    }
    const char *png_path = TextFormat("%s/%s", directory, PNG_NAME);
    const char *json_path = TextFormat("%s/%s", directory, JSON_NAME);

    int curve_count = 0;
    Vector2 *curve = BuildCurve(level, &curve_count);
    if (curve == NULL)
    {
        snprintf(message, size, "Could not build the curve");
        return false;
    }

    Image image = GenImageColor(MAP_W, MAP_H, COLOR_OUTSIDE);
    RenderLogicMap(&image, curve, curve_count);
    free(curve);
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
    bool png_ok = ExportImage(image, png_path);
    UnloadImage(image);
    if (!png_ok)
    {
        snprintf(message, size, "Could not write %s", PNG_NAME);
        return false;
    }

    FILE *file = fopen(json_path, "w");
    if (file == NULL)
    {
        snprintf(message, size, "Could not write %s", JSON_NAME);
        return false;
    }
    fprintf(file, "{\n");
    fprintf(file, "  \"version\": 1,\n");
    fprintf(file, "  \"map_width\": %d,\n", MAP_W);
    fprintf(file, "  \"map_height\": %d,\n", MAP_H);
    fprintf(file, "  \"road_width\": %d,\n", ROAD_WIDTH);
    fprintf(file, "  \"logic_map\": \"%s\",\n", PNG_NAME);
    fprintf(file, "  \"colors\": {\n");
    fprintf(file, "    \"outside\": [%d, %d, %d],\n", COLOR_OUTSIDE.r,
            COLOR_OUTSIDE.g, COLOR_OUTSIDE.b);
    fprintf(file, "    \"road\": [%d, %d, %d]\n", COLOR_ROAD.r, COLOR_ROAD.g,
            COLOR_ROAD.b);
    fprintf(file, "  },\n");
    fprintf(file, "  \"closed\": %s,\n", level->closed ? "true" : "false");
    fprintf(file, "  \"points\": [\n");
    for (int i = 0; i < level->count; i++)
    {
        fprintf(file, "    [%.2f, %.2f]%s\n", level->points[i].x,
                level->points[i].y, i + 1 < level->count ? "," : "");
    }
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
    bool json_ok = fclose(file) == 0;
    if (!json_ok)
    {
        snprintf(message, size, "Could not write %s", JSON_NAME);
        return false;
    }

    snprintf(message, size, "Saved to %s/", directory);
    return true;
}

// Return the text after a key and its colon, or NULL if the key is missing
static const char *FindKey(const char *text, const char *key)
{
    char quoted[64];
    snprintf(quoted, sizeof(quoted), "\"%s\"", key);
    const char *found = strstr(text, quoted);
    if (found == NULL)
        return NULL;
    found = strchr(found, ':');
    return found == NULL ? NULL : found + 1;
}

// Read a number that follows a key, return false if it is missing
static bool ReadNumber(const char *text, const char *key, double *value)
{
    const char *position = FindKey(text, key);
    if (position == NULL)
        return false;
    char *end = NULL;
    *value = strtod(position, &end);
    return end != position;
}

// Skip spaces and the characters in skip, then return the next character
static const char *SkipTo(const char *position, const char *skip)
{
    while (*position != '\0' && (isspace((unsigned char)*position) ||
                                 strchr(skip, *position) != NULL))
    {
        position++;
    }
    return position;
}

// Load levels/<name>/level.json into level, write the result in message
static bool LoadLevel(const char *name, Level *level, char *message,
                      size_t size)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s/%s", LEVELS_DIR, name, JSON_NAME);
    if (name[0] == '\0' || !FileExists(path))
    {
        char list[160] = "";
        FilePathList files = LoadDirectoryFiles(LEVELS_DIR);
        for (unsigned int i = 0; i < files.count; i++)
        {
            if (!DirectoryExists(files.paths[i]))
                continue;
            const char *entry = GetFileName(files.paths[i]);
            if (strlen(list) + strlen(entry) + 3 >= sizeof(list))
                break;
            if (list[0] != '\0')
                strcat(list, ", ");
            strcat(list, entry);
        }
        UnloadDirectoryFiles(files);
        if (list[0] == '\0')
        {
            snprintf(message, size, "Level \"%s\" not found, no saved levels", name);
        }
        else
        {
            snprintf(message, size, "Level \"%s\" not found, saved levels: %s",
                     name, list);
        }
        return false;
    }

    char *text = LoadFileText(path);
    if (text == NULL)
    {
        snprintf(message, size, "Could not read %s", path);
        return false;
    }

    double map_w = 0, map_h = 0, road_width = 0;
    bool valid = ReadNumber(text, "map_width", &map_w) &&
                 ReadNumber(text, "map_height", &map_h) &&
                 ReadNumber(text, "road_width", &road_width);
    if (!valid)
    {
        snprintf(message, size, "%s is not a valid level file", path);
        UnloadFileText(text);
        return false;
    }
    if ((int)map_w != MAP_W || (int)map_h != MAP_H ||
        (int)road_width != ROAD_WIDTH)
    {
        snprintf(message, size, "%s has a different map or road size", path);
        UnloadFileText(text);
        return false;
    }

    Level loaded = {.count = 0, .closed = false};
    const char *closed = FindKey(text, "closed");
    if (closed != NULL)
    {
        loaded.closed = strncmp(SkipTo(closed, ""), "true", 4) == 0;
    }

    const char *position = FindKey(text, "points");
    position = position == NULL ? NULL : strchr(position, '[');
    if (position == NULL)
    {
        snprintf(message, size, "%s has no points", path);
        UnloadFileText(text);
        return false;
    }
    position++;
    for (;;)
    {
        position = SkipTo(position, ",");
        if (*position != '[')
            break;
        char *end = NULL;
        float x = strtof(position + 1, &end);
        const char *after_x = SkipTo(end, ",");
        float y = strtof(after_x, &end);
        if (after_x == end || *SkipTo(end, "") != ']')
        {
            snprintf(message, size, "%s has an invalid point", path);
            UnloadFileText(text);
            return false;
        }
        if (loaded.count >= MAX_POINTS)
        {
            snprintf(message, size, "%s has more than %d points", path,
                     MAX_POINTS);
            UnloadFileText(text);
            return false;
        }
        loaded.points[loaded.count++] = (Vector2){x, y};
        position = SkipTo(end, "") + 1;
    }
    UnloadFileText(text);

    if (loaded.count < 3)
        loaded.closed = false;
    *level = loaded;
    snprintf(message, size, "Opened %s/%s/ with %d points", LEVELS_DIR, name,
             loaded.count);
    return true;
}

// Return the index of the point nearest to a map position, or -1
// Only points within radius (in map pixels) can be selected
static int FindPoint(const Level *level, Vector2 position, float radius)
{
    int best = -1;
    float best_distance = radius;
    for (int i = 0; i < level->count; i++)
    {
        float distance = Vector2Distance(level->points[i], position);
        if (distance <= best_distance)
        {
            best = i;
            best_distance = distance;
        }
    }
    return best;
}

static Vector2 WorldToScreen(Vector2 point, Vector2 camera, float zoom)
{
    return Vector2Scale(Vector2Subtract(point, camera), zoom);
}

static void RemovePoint(Level *level, int index)
{
    for (int i = index; i < level->count - 1; i++)
    {
        level->points[i] = level->points[i + 1];
    }
    level->count--;
    if (level->count < 3)
        level->closed = false;
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Level editor");
    SetTargetFPS(60);

    Level level = {.count = 0, .closed = false};
    Vector2 *curve = NULL;
    int curve_count = 0;

    Image logic = GenImageColor(MAP_W, MAP_H, COLOR_OUTSIDE);
    Texture2D texture = LoadTextureFromImage(logic);
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);

    Rectangle export_button = {10, 6, 100, TOOLBAR_H - 12};
    Rectangle open_button = {120, 6, 80, TOOLBAR_H - 12};
    Rectangle name_box = {210, 6, 220, TOOLBAR_H - 12};
    char name[MAX_NAME + 1] = "level1";
    int name_length = (int)strlen(name);
    bool name_focus = false;
    Vector2 camera = {0, 0}; // top left corner of the view, in map pixels
    float zoom = 1.0f;
    float scroll_lock = 0.0f; // block scroll while this time is above zero
    char message[256] = "";
    float message_time = 0.0f;

    while (!WindowShouldClose())
    {
        int win_w = GetScreenWidth();
        int win_h = GetScreenHeight();

        Vector2 mouse = GetMousePosition();
        bool command = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER) ||
                       IsKeyDown(KEY_LEFT_CONTROL) ||
                       IsKeyDown(KEY_RIGHT_CONTROL);

        // Zoom at the cursor, the map point under the cursor stays in place
        Vector2 wheel = GetMouseWheelMoveV();
        float zoom_factor = 1.0f;
        if (command)
        {
            zoom_factor *= powf(ZOOM_WHEEL_STEP, wheel.y);
            wheel = (Vector2){0, 0};
        }
        if (!name_focus)
        {
            // Read characters instead of key codes to match the keyboard layout
            int key = GetCharPressed();
            while (key > 0)
            {
                if (key == '+' || key == '=')
                    zoom_factor *= ZOOM_KEY_STEP;
                if (key == '-' || key == '_')
                    zoom_factor /= ZOOM_KEY_STEP;
                key = GetCharPressed();
            }
        }
        if (zoom_factor != 1.0f)
        {
            float old_zoom = zoom;
            zoom = Clamp(zoom * zoom_factor, ZOOM_MIN, ZOOM_MAX);
            Vector2 anchor = Vector2Add(Vector2Scale(mouse, 1.0f / old_zoom),
                                        camera);
            camera = Vector2Subtract(anchor, Vector2Scale(mouse, 1.0f / zoom));
        }

        // Ignore scroll input during a zoom and while the trackpad momentum
        // from the zoom continues
        if (zoom_factor != 1.0f)
            scroll_lock = ZOOM_LOCK_SECONDS;
        if (scroll_lock > 0.0f)
        {
            if (wheel.x != 0.0f || wheel.y != 0.0f)
            {
                scroll_lock = ZOOM_LOCK_SECONDS;
            }
            wheel = (Vector2){0, 0};
            scroll_lock -= GetFrameTime();
        }

        // Scroll the view
        if ((IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) &&
            wheel.x == 0.0f)
        {
            wheel.x = wheel.y;
            wheel.y = 0.0f;
        }
        camera.x -= wheel.x * SCROLL_SPEED / zoom;
        camera.y -= wheel.y * SCROLL_SPEED / zoom;
        // The view can leave the map, but a part of the map stays in view
        camera.x = Clamp(camera.x, -(win_w - EDGE_MARGIN) / zoom,
                         MAP_W - EDGE_MARGIN / zoom);
        camera.y = Clamp(camera.y, -(win_h - EDGE_MARGIN) / zoom,
                         MAP_H - EDGE_MARGIN / zoom);

        Vector2 world = Vector2Add(Vector2Scale(mouse, 1.0f / zoom), camera);
        float pick_radius = PICK_RADIUS / zoom;
        bool over_toolbar = mouse.y < TOOLBAR_H;
        bool in_map = world.x >= 0.0f && world.x < MAP_W && world.y >= 0.0f &&
                      world.y < MAP_H;

        // Type the level name, only letters, digits, "_" and "-" are valid
        if (name_focus)
        {
            int key = GetCharPressed();
            while (key > 0)
            {
                bool valid = (key >= 'a' && key <= 'z') ||
                             (key >= 'A' && key <= 'Z') ||
                             (key >= '0' && key <= '9') || key == '_' ||
                             key == '-';
                if (valid && name_length < MAX_NAME)
                {
                    name[name_length++] = (char)key;
                    name[name_length] = '\0';
                }
                key = GetCharPressed();
            }
            if ((IsKeyPressed(KEY_BACKSPACE) ||
                 IsKeyPressedRepeat(KEY_BACKSPACE)) &&
                name_length > 0)
            {
                name[--name_length] = '\0';
            }
            if (IsKeyPressed(KEY_ENTER))
                name_focus = false;
        }

        bool changed = false;
        if (!name_focus && command &&
            (IsKeyPressed(KEY_Z) || IsKeyPressedRepeat(KEY_Z)) &&
            PopUndo(&level))
        {
            changed = true;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            name_focus = CheckCollisionPointRec(mouse, name_box);
            if (CheckCollisionPointRec(mouse, export_button))
            {
                ExportLevel(&level, name, message, sizeof(message));
                message_time = MESSAGE_SECONDS;
            }
            else if (CheckCollisionPointRec(mouse, open_button))
            {
                Level loaded;
                if (LoadLevel(name, &loaded, message, sizeof(message)))
                {
                    PushUndo(&level);
                    level = loaded;
                    changed = true;
                    zoom = 1.0f;
                    if (level.count > 0)
                    {
                        camera = Vector2Subtract(
                            level.points[0], (Vector2){win_w / 2.0f / zoom,
                                                       win_h / 2.0f / zoom});
                    }
                }
                message_time = MESSAGE_SECONDS;
            }
            else if (!over_toolbar && in_map)
            {
                int hit = FindPoint(&level, world, pick_radius);
                if (level.closed)
                {
                    snprintf(message, sizeof(message),
                             "The curve is closed, press Cmd+Z to open it");
                    message_time = MESSAGE_SECONDS;
                }
                else if (hit == 0)
                {
                    if (level.count >= 3)
                    {
                        PushUndo(&level);
                        level.closed = true;
                        changed = true;
                    }
                }
                else if (hit < 0 && level.count < MAX_POINTS)
                {
                    PushUndo(&level);
                    level.points[level.count++] = world;
                    changed = true;
                }
            }
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !over_toolbar)
        {
            int index = FindPoint(&level, world, pick_radius);
            if (index >= 0)
            {
                PushUndo(&level);
                RemovePoint(&level, index);
                changed = true;
            }
        }

        if (changed)
        {
            free(curve);
            curve = BuildCurve(&level, &curve_count);
            RenderLogicMap(&logic, curve, curve_count);
            UpdateTexture(texture, logic.data);
        }
        if (message_time > 0.0f)
            message_time -= GetFrameTime();

        int hover = over_toolbar ? -1 : FindPoint(&level, world, pick_radius);

        BeginDrawing();
        ClearBackground(DARKGRAY);

        Rectangle source = {0, 0, MAP_W, MAP_H};
        Vector2 map_origin = WorldToScreen((Vector2){0, 0}, camera, zoom);
        Rectangle destination = {map_origin.x, map_origin.y, MAP_W * zoom,
                                 MAP_H * zoom};
        DrawTexturePro(texture, source, destination, (Vector2){0, 0}, 0.0f,
                       WHITE);

        for (int i = 0; i + 1 < curve_count; i++)
        {
            DrawLineV(WorldToScreen(curve[i], camera, zoom),
                      WorldToScreen(curve[i + 1], camera, zoom), LIGHTGRAY);
        }
        for (int i = 0; i < level.count; i++)
        {
            Vector2 screen = WorldToScreen(level.points[i], camera, zoom);
            Color fill = i == 0 ? SKYBLUE : YELLOW;
            DrawCircleV(screen, POINT_RADIUS, i == hover ? RED : fill);
            DrawCircleLinesV(screen, POINT_RADIUS, BLACK);
            DrawText(TextFormat("%d", i + 1), (int)screen.x + 9,
                     (int)screen.y - 18, 14, WHITE);
        }

        DrawRectangle(0, 0, win_w, TOOLBAR_H, (Color){30, 30, 30, 255});
        bool over_button = CheckCollisionPointRec(mouse, export_button);
        DrawRectangleRec(export_button,
                         over_button ? (Color){90, 90, 90, 255}
                                     : (Color){60, 60, 60, 255});
        DrawText("Export", (int)export_button.x + 20, (int)export_button.y + 6,
                 18, WHITE);
        bool over_open = CheckCollisionPointRec(mouse, open_button);
        DrawRectangleRec(open_button, over_open ? (Color){90, 90, 90, 255}
                                                : (Color){60, 60, 60, 255});
        DrawText("Open", (int)open_button.x + 20, (int)open_button.y + 6, 18,
                 WHITE);
        DrawRectangleRec(name_box, (Color){15, 15, 15, 255});
        DrawRectangleLinesEx(name_box, 1.0f, name_focus ? YELLOW : GRAY);
        DrawText(name, (int)name_box.x + 8, (int)name_box.y + 6, 18, WHITE);
        if (name_focus && fmodf((float)GetTime(), 1.0f) < 0.5f)
        {
            int cursor_x = (int)name_box.x + 8 + MeasureText(name, 18) + 1;
            DrawRectangle(cursor_x, (int)name_box.y + 5, 2, 20, WHITE);
        }
        DrawText(TextFormat("Points: %d%s   Zoom: %d%%", level.count,
                            level.closed ? " (closed)" : "",
                            (int)roundf(zoom * 100.0f)),
                 450, 11, 18, LIGHTGRAY);
        if (message_time > 0.0f)
        {
            int message_w = MeasureText(message, 18);
            DrawRectangle(0, win_h - 34, message_w + 20, 34,
                          (Color){30, 30, 30, 230});
            DrawText(message, 10, win_h - 26, 18, YELLOW);
        }
        EndDrawing();
    }

    free(curve);
    UnloadTexture(texture);
    UnloadImage(logic);
    CloseWindow();
    return 0;
}
