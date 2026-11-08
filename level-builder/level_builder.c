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
//   Tree buttons            : choose the species of trees around the road
//   Road slider             : drag to set the width of the road
//   Ground button           : click to choose the look of the ground (forest, autumn, mud, lakeside)
//   Time of day button      : click to choose the light of the stage (day, overcast, dusk, night, rain)
//   All the buttons are in the panel at the right side of the window
//
// Run from the project root:
//   make -C level-builder run
// For development: --open <name> opens a level at the start, --shot <file> saves a picture of the window and quits
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/difficulty.h"
#include "level/ground_theme.h"
#include "level/map_event_type.h"
#include "level/time_of_day.h"
#include "level/tree_species.h"
#include "raylib.h"
#include "raymath.h"

#define MAP_W 2048
#define MAP_H 2048
#define ROAD_WIDTH_DEFAULT 64 // road width of a new level, in map pixels
#define ROAD_WIDTH_MIN 32
#define ROAD_WIDTH_MAX 128
#define ROAD_WIDTH_STEP 4
#define MAP_PIXELS_PER_METER 8.0f
#define MAX_POINTS 512
#define MAX_UNDO 256
#define CURVE_STEP 2.0f // maximum distance between curve samples

#define PANEL_W 280      // the panel with the settings of the level, at the right side of the window
#define PANEL_PAD 16
#define ROW_H 30         // height of a button or a text box in the panel
#define LABEL_GAP 20     // a label is this far over its button
#define SECTION_GAP 18   // space between two groups of settings
#define FONT_LABEL 16
#define FONT_FIELD 18
#define COLOR_PANEL ((Color){30, 30, 30, 255})
#define COLOR_BUTTON ((Color){60, 60, 60, 255})
#define COLOR_BUTTON_HOVER ((Color){90, 90, 90, 255})
#define COLOR_LABEL ((Color){150, 150, 150, 255})
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
#define MAX_DISPLAY_NAME 40
#define MAX_RIVALS 7 // the builder does not change the times of the other drivers, it only keeps them

static const Color COLOR_OUTSIDE = {40, 150, 50, 255};
static const Color COLOR_ROAD = {130, 90, 50, 255};

typedef struct
{
    Vector2 points[MAX_POINTS];
    int count;
    bool closed;   // the last point connects to the first point
    bool tutorial; // the game shows the hints of the tutorial on this level
    char display_name[MAX_DISPLAY_NAME + 1]; // the name that the player sees, "" for the folder name
    float times[DIFFICULTY_COUNT][MAX_RIVALS]; // seconds of the other drivers on each difficulty, kept as they are
    int time_count[DIFFICULTY_COUNT];
    bool jelka_just_ahead;
    bool trees[TREE_SPECIES_COUNT]; // the species of trees around the road
    GroundTheme theme;              // the look of the ground in the game
    TimeOfDay time_of_day;          // the light of the stage in the game
    char music[64];                 // the music file in assets/music/ while driving, "" for no music, kept as it is
    bool map_events[MAP_EVENT_TYPE_COUNT]; // Luka's map events, kept as they are
    int road_width; // map pixels
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
static void RenderLogicMap(Image *image, const Vector2 *curve, int count, int road_width)
{
    ImageClearBackground(image, COLOR_OUTSIDE);
    for (int i = 0; i < count; i++)
    {
        ImageDrawCircleV(image, curve[i], road_width / 2, COLOR_ROAD);
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
    RenderLogicMap(&image, curve, curve_count, level->road_width);
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
    fprintf(file, "  \"road_width\": %d,\n", level->road_width);
    fprintf(file, "  \"logic_map\": \"%s\",\n", PNG_NAME);
    fprintf(file, "  \"colors\": {\n");
    fprintf(file, "    \"outside\": [%d, %d, %d],\n", COLOR_OUTSIDE.r,
            COLOR_OUTSIDE.g, COLOR_OUTSIDE.b);
    fprintf(file, "    \"road\": [%d, %d, %d]\n", COLOR_ROAD.r, COLOR_ROAD.g,
            COLOR_ROAD.b);
    fprintf(file, "  },\n");
    fprintf(file, "  \"closed\": %s,\n", level->closed ? "true" : "false");
    if (level->display_name[0] != '\0')
    {
        fprintf(file, "  \"display_name\": \"%s\",\n", level->display_name);
    }
    if (level->tutorial)
    {
        fprintf(file, "  \"tutorial\": true,\n");
    }
    if (level->jelka_just_ahead)
    {
        fprintf(file, "  \"jelka_just_ahead\": true,\n");
    }
    fprintf(file, "  \"theme\": \"%s\",\n", GROUND_THEME_NAMES[level->theme]);
    fprintf(file, "  \"time_of_day\": \"%s\",\n", LIGHTING[level->time_of_day].name);
    if (level->music[0] != '\0')
    {
        fprintf(file, "  \"music\": \"%s\",\n", level->music);
    }
    bool any_event = false;
    for (int i = 0; i < MAP_EVENT_TYPE_COUNT; i++)
        any_event = any_event || level->map_events[i];
    if (any_event)
    {
        fprintf(file, "  \"map_events\": [");
        bool first = true;
        for (int i = 0; i < MAP_EVENT_TYPE_COUNT; i++)
        {
            if (level->map_events[i])
            {
                fprintf(file, "%s\"%s\"", first ? "" : ", ", MAP_EVENT_NAMES[i]);
                first = false;
            }
        }
        fprintf(file, "],\n");
    }
    for (int d = 0; d < DIFFICULTY_COUNT; d++)
    {
        if (level->time_count[d] == 0)
            continue;
        fprintf(file, "  \"%s\": [", DIFFICULTY_RULES[d].times_key);
        for (int i = 0; i < level->time_count[d]; i++)
        {
            fprintf(file, "%s%.2f", i > 0 ? ", " : "", level->times[d][i]);
        }
        fprintf(file, "],\n");
    }
    fprintf(file, "  \"trees\": [");
    bool first_tree = true;
    for (int i = 0; i < TREE_SPECIES_COUNT; i++)
    {
        if (level->trees[i])
        {
            fprintf(file, "%s\"%s\"", first_tree ? "" : ", ", TREE_SPECIES_NAMES[i]);
            first_tree = false;
        }
    }
    fprintf(file, "],\n");
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
    if ((int)map_w != MAP_W || (int)map_h != MAP_H)
    {
        snprintf(message, size, "%s has a different map size", path);
        UnloadFileText(text);
        return false;
    }
    if ((int)road_width < ROAD_WIDTH_MIN || (int)road_width > ROAD_WIDTH_MAX)
    {
        snprintf(message, size, "%s has a road width outside %d to %d", path,
                 ROAD_WIDTH_MIN, ROAD_WIDTH_MAX);
        UnloadFileText(text);
        return false;
    }

    Level loaded = {.count = 0, .closed = false, .road_width = (int)road_width};
    const char *closed = FindKey(text, "closed");
    const char *tutorial = FindKey(text, "tutorial");
    loaded.tutorial = tutorial != NULL && strncmp(SkipTo(tutorial, ""), "true", 4) == 0;
    const char *theme = FindKey(text, "theme");
    if (theme != NULL && *SkipTo(theme, "") == '"')
    {
        char theme_name[32] = "";
        sscanf(SkipTo(theme, "") + 1, "%31[^\"]", theme_name);
        loaded.theme = GroundThemeFind(theme_name);
    }
    const char *time = FindKey(text, "time_of_day");
    if (time != NULL && *SkipTo(time, "") == '"')
    {
        char time_name[32] = "";
        sscanf(SkipTo(time, "") + 1, "%31[^\"]", time_name);
        loaded.time_of_day = TimeOfDayFind(time_name);
    }
    const char *events = FindKey(text, "map_events");
    events = events == NULL ? NULL : strchr(events, '[');
    if (events != NULL)
    {
        const char *events_end = strchr(events, ']');
        for (int i = 0; i < MAP_EVENT_TYPE_COUNT; i++)
        {
            const char *entry = strstr(events, TextFormat("\"%s\"", MAP_EVENT_NAMES[i]));
            loaded.map_events[i] = entry != NULL && events_end != NULL && entry < events_end;
        }
    }
    const char *music = FindKey(text, "music");
    if (music != NULL && *SkipTo(music, "") == '"')
    {
        sscanf(SkipTo(music, "") + 1, "%63[^\"]", loaded.music);
    }
    const char *display = FindKey(text, "display_name");
    if (display != NULL && *SkipTo(display, "") == '"')
    {
        const char *start = SkipTo(display, "") + 1;
        int length = 0;
        while (start[length] != '\0' && start[length] != '"' && length < MAX_DISPLAY_NAME)
        {
            loaded.display_name[length] = start[length];
            length++;
        }
        loaded.display_name[length] = '\0';
    }
    const char *ahead = FindKey(text, "jelka_just_ahead");
    loaded.jelka_just_ahead = ahead != NULL && strncmp(SkipTo(ahead, ""), "true", 4) == 0;
    for (int d = 0; d < DIFFICULTY_COUNT; d++)
    {
        const char *times = FindKey(text, DIFFICULTY_RULES[d].times_key);
        if (times == NULL || *SkipTo(times, "") != '[')
            continue;
        const char *position = SkipTo(times, "") + 1;
        while (loaded.time_count[d] < MAX_RIVALS)
        {
            char *end;
            float value = strtof(SkipTo(position, ","), &end);
            if (end == SkipTo(position, ","))
            {
                break;
            }
            loaded.times[d][loaded.time_count[d]++] = value;
            position = end;
        }
    }
    if (closed != NULL)
    {
        loaded.closed = strncmp(SkipTo(closed, ""), "true", 4) == 0;
    }

    // A level without a list of trees has the default species
    const char *trees = FindKey(text, "trees");
    trees = trees == NULL ? NULL : strchr(trees, '[');
    if (trees == NULL)
    {
        loaded.trees[TREE_SPECIES_DEFAULT] = true;
    }
    else
    {
        const char *list_end = strchr(trees, ']');
        for (int i = 0; i < TREE_SPECIES_COUNT; i++)
        {
            const char *entry = strstr(trees, TextFormat("\"%s\"", TREE_SPECIES_NAMES[i]));
            loaded.trees[i] = entry != NULL && list_end != NULL && entry < list_end;
        }
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

static void DrawPanelButton(Rectangle rect, const char *label, bool hover, bool on)
{
    Color fill = on ? (Color){40, 120, 60, 255} : COLOR_BUTTON;
    if (hover)
        fill = on ? (Color){60, 150, 80, 255} : COLOR_BUTTON_HOVER;
    DrawRectangleRec(rect, fill);
    if (on)
        DrawRectangleLinesEx(rect, 1.0f, (Color){140, 220, 150, 255});
    int width = MeasureText(label, FONT_FIELD);
    DrawText(label, (int)(rect.x + (rect.width - (float)width) / 2.0f), (int)(rect.y + (rect.height - FONT_FIELD) / 2.0f),
             FONT_FIELD, WHITE);
}

// A text box, the hint shows in grey when the text is empty, a blinking cursor shows when it has the focus
static void DrawTextBox(Rectangle rect, const char *text, const char *hint, bool focus)
{
    DrawRectangleRec(rect, (Color){15, 15, 15, 255});
    DrawRectangleLinesEx(rect, 1.0f, focus ? YELLOW : GRAY);
    int text_y = (int)(rect.y + (rect.height - FONT_FIELD) / 2.0f);
    if (text[0] == '\0' && !focus)
        DrawText(hint, (int)rect.x + 8, text_y, FONT_FIELD, DARKGRAY);
    else
        DrawText(text, (int)rect.x + 8, text_y, FONT_FIELD, WHITE);
    if (focus && fmodf((float)GetTime(), 1.0f) < 0.5f)
        DrawRectangle((int)rect.x + 8 + MeasureText(text, FONT_FIELD) + 1, text_y - 1, 2, FONT_FIELD + 2, WHITE);
}

int main(int argc, char **argv)
{
    const char *open_name = NULL;
    const char *shot = NULL;
    for (int i = 1; i + 1 < argc; i++)
    {
        if (strcmp(argv[i], "--open") == 0)
            open_name = argv[++i];
        else if (strcmp(argv[i], "--shot") == 0)
            shot = argv[++i];
    }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Level editor");
    SetTargetFPS(60);

    Level level = {.count = 0, .closed = false, .road_width = ROAD_WIDTH_DEFAULT};
    level.trees[TREE_SPECIES_DEFAULT] = true;
    Vector2 *curve = NULL;
    int curve_count = 0;

    Image logic = GenImageColor(MAP_W, MAP_H, COLOR_OUTSIDE);
    Texture2D texture = LoadTextureFromImage(logic);
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);

    Rectangle export_button;
    Rectangle open_button;
    Rectangle name_box;
    Rectangle tree_buttons[TREE_SPECIES_COUNT];
    Rectangle road_slider; // the area that takes clicks
    Rectangle display_box;
    Rectangle tutorial_box;
    Rectangle theme_button;
    Rectangle time_button;
    float label_y[6];      // the labels over the groups: folder, display name, road, ground, time of day, trees
    bool display_focus = false;
    bool road_drag = false;
    char name[MAX_NAME + 1] = "level1";
    int name_length = (int)strlen(name);
    bool name_focus = false;
    Vector2 camera = {0, 0}; // top left corner of the view, in map pixels
    float zoom = 1.0f;
    float scroll_lock = 0.0f; // block scroll while this time is above zero
    char message[256] = "";
    float message_time = 0.0f;
    int frames = 0;
    if (open_name != NULL)
    {
        snprintf(name, sizeof(name), "%s", open_name);
        name_length = (int)strlen(name);
        bool opened = LoadLevel(name, &level, message, sizeof(message));
        message_time = MESSAGE_SECONDS;
        if (opened)
        {
            free(curve);
            curve = BuildCurve(&level, &curve_count);
            RenderLogicMap(&logic, curve, curve_count, level.road_width);
            UpdateTexture(texture, logic.data);
            zoom = 0.33f;
        }
    }

    while (!WindowShouldClose())
    {
        int win_w = GetScreenWidth();
        int win_h = GetScreenHeight();

        // The panel: each group has a label and the buttons under it, from the top down
        float panel_x = (float)(win_w - PANEL_W);
        float x = panel_x + PANEL_PAD;
        float w = PANEL_W - 2.0f * PANEL_PAD;
        float half = (w - 8.0f) / 2.0f;
        float y = 56.0f;
        label_y[0] = y;
        name_box = (Rectangle){x, y + LABEL_GAP, w, ROW_H};
        y = name_box.y + ROW_H + 8.0f;
        open_button = (Rectangle){x, y, half, ROW_H};
        export_button = (Rectangle){x + half + 8.0f, y, half, ROW_H};
        y += ROW_H + SECTION_GAP;
        label_y[1] = y;
        display_box = (Rectangle){x, y + LABEL_GAP, w, ROW_H};
        y = display_box.y + ROW_H + 8.0f;
        tutorial_box = (Rectangle){x, y, w, ROW_H};
        y += ROW_H + SECTION_GAP;
        label_y[2] = y;
        road_slider = (Rectangle){x, y + LABEL_GAP, w, ROW_H};
        y = road_slider.y + ROW_H + SECTION_GAP;
        label_y[3] = y;
        theme_button = (Rectangle){x, y + LABEL_GAP, w, ROW_H};
        y = theme_button.y + ROW_H + SECTION_GAP;
        label_y[4] = y;
        time_button = (Rectangle){x, y + LABEL_GAP, w, ROW_H};
        y = time_button.y + ROW_H + SECTION_GAP;
        label_y[5] = y;
        for (int i = 0; i < TREE_SPECIES_COUNT; i++)
        {
            tree_buttons[i] = (Rectangle){x + (float)(i % 2) * (half + 8.0f), y + LABEL_GAP + (float)(i / 2) * (ROW_H + 6.0f),
                                          half, ROW_H};
        }

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
        if (!name_focus && !display_focus)
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
        bool over_panel = mouse.x >= (float)(win_w - PANEL_W);
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

        // Type the display name, all printable characters but quotes and backslashes are valid
        if (display_focus)
        {
            int length = (int)strlen(level.display_name);
            int key = GetCharPressed();
            while (key > 0)
            {
                if (key >= 32 && key < 127 && key != '"' && key != '\\' && length < MAX_DISPLAY_NAME)
                {
                    level.display_name[length++] = (char)key;
                    level.display_name[length] = '\0';
                }
                key = GetCharPressed();
            }
            if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && length > 0)
            {
                level.display_name[--length] = '\0';
            }
            if (IsKeyPressed(KEY_ENTER))
                display_focus = false;
        }

        bool changed = false;
        if (!name_focus && !display_focus && command &&
            (IsKeyPressed(KEY_Z) || IsKeyPressedRepeat(KEY_Z)) &&
            PopUndo(&level))
        {
            changed = true;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            name_focus = CheckCollisionPointRec(mouse, name_box);
            display_focus = CheckCollisionPointRec(mouse, display_box);
            int clicked_tree = -1;
            for (int i = 0; i < TREE_SPECIES_COUNT; i++)
            {
                if (CheckCollisionPointRec(mouse, tree_buttons[i]))
                    clicked_tree = i;
            }
            if (CheckCollisionPointRec(mouse, road_slider))
            {
                PushUndo(&level);
                road_drag = true;
            }
            else if (clicked_tree >= 0)
            {
                PushUndo(&level);
                level.trees[clicked_tree] = !level.trees[clicked_tree];
            }
            else if (CheckCollisionPointRec(mouse, tutorial_box))
            {
                PushUndo(&level);
                level.tutorial = !level.tutorial;
            }
            else if (CheckCollisionPointRec(mouse, theme_button))
            {
                PushUndo(&level);
                level.theme = (GroundTheme)((level.theme + 1) % GROUND_THEME_COUNT);
            }
            else if (CheckCollisionPointRec(mouse, time_button))
            {
                PushUndo(&level);
                level.time_of_day = (TimeOfDay)((level.time_of_day + 1) % TIME_OF_DAY_COUNT);
            }
            else if (CheckCollisionPointRec(mouse, export_button))
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
                            level.points[0], (Vector2){(win_w - PANEL_W) / 2.0f / zoom,
                                                       win_h / 2.0f / zoom});
                    }
                }
                message_time = MESSAGE_SECONDS;
            }
            else if (!over_panel && in_map)
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
        if (road_drag && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            float t = Clamp((mouse.x - road_slider.x) / road_slider.width, 0.0f, 1.0f);
            int steps = (int)roundf(t * (ROAD_WIDTH_MAX - ROAD_WIDTH_MIN) / ROAD_WIDTH_STEP);
            int width = ROAD_WIDTH_MIN + steps * ROAD_WIDTH_STEP;
            if (width != level.road_width)
            {
                level.road_width = width;
                changed = true;
            }
        }
        else
        {
            road_drag = false;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !over_panel)
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
            RenderLogicMap(&logic, curve, curve_count, level.road_width);
            UpdateTexture(texture, logic.data);
        }
        if (message_time > 0.0f)
            message_time -= GetFrameTime();

        int hover = over_panel ? -1 : FindPoint(&level, world, pick_radius);

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

        DrawRectangle((int)panel_x, 0, PANEL_W, win_h, COLOR_PANEL);
        DrawRectangle((int)panel_x, 0, 1, win_h, (Color){70, 70, 70, 255});
        DrawText("Level", (int)x, 18, 24, WHITE);
        const char *labels[6] = {"Folder name", "Display name", "", "Ground", "Time of day", "Trees"};
        for (int i = 0; i < 6; i++)
        {
            DrawText(labels[i], (int)x, (int)label_y[i], FONT_LABEL, COLOR_LABEL);
        }
        DrawText(TextFormat("Road width   %.1f m", (float)level.road_width / MAP_PIXELS_PER_METER), (int)x,
                 (int)label_y[2], FONT_LABEL, COLOR_LABEL);

        DrawTextBox(name_box, name, "", name_focus);
        DrawPanelButton(open_button, "Open", CheckCollisionPointRec(mouse, open_button), false);
        DrawPanelButton(export_button, "Export", CheckCollisionPointRec(mouse, export_button), false);
        DrawTextBox(display_box, level.display_name, "Same as the folder", display_focus);

        bool over_tutorial = CheckCollisionPointRec(mouse, tutorial_box);
        DrawRectangleRec(tutorial_box, over_tutorial ? COLOR_BUTTON_HOVER : COLOR_BUTTON);
        Rectangle check = {tutorial_box.x + 8, tutorial_box.y + 8, 14, 14};
        DrawRectangleLinesEx(check, 2.0f, WHITE);
        if (level.tutorial)
        {
            DrawRectangle((int)check.x + 4, (int)check.y + 4, 6, 6, YELLOW);
        }
        DrawText("Tutorial (hints in the game)", (int)check.x + 22, (int)tutorial_box.y + 7, FONT_LABEL, WHITE);

        float knob_t = (float)(level.road_width - ROAD_WIDTH_MIN) / (ROAD_WIDTH_MAX - ROAD_WIDTH_MIN);
        float track_y = road_slider.y + road_slider.height / 2.0f;
        bool over_road = road_drag || CheckCollisionPointRec(mouse, road_slider);
        DrawRectangle((int)road_slider.x, (int)track_y - 2, (int)road_slider.width, 4, (Color){90, 90, 90, 255});
        DrawCircleV((Vector2){road_slider.x + knob_t * road_slider.width, track_y}, 8.0f,
                    over_road ? YELLOW : LIGHTGRAY);

        DrawPanelButton(theme_button, TextFormat("%s   >", GROUND_THEME_NAMES[level.theme]),
                        CheckCollisionPointRec(mouse, theme_button), false);
        DrawPanelButton(time_button, TextFormat("%s   >", LIGHTING[level.time_of_day].name),
                        CheckCollisionPointRec(mouse, time_button), false);
        for (int i = 0; i < TREE_SPECIES_COUNT; i++)
        {
            char label[TREE_VARIANT_NAME_MAX];
            snprintf(label, sizeof(label), "%s", TREE_SPECIES_NAMES[i]);
            label[0] = (char)toupper((unsigned char)label[0]);
            DrawPanelButton(tree_buttons[i], label, CheckCollisionPointRec(mouse, tree_buttons[i]), level.trees[i]);
        }

        // Help at the bottom of the panel
        const char *help[] = {"Click: add a point", "Click point 1: close the road", "Right click: delete a point",
                              "Cmd+Z: undo", "Scroll: move,  Cmd+scroll: zoom"};
        for (int i = 0; i < 5; i++)
        {
            DrawText(help[i], (int)x, win_h - 150 + i * 20, FONT_LABEL, COLOR_LABEL);
        }
        const char *status = TextFormat("%d points%s,  zoom %d%%", level.count,
                                        level.closed ? " (closed)" : "", (int)roundf(zoom * 100.0f));
        DrawText(status, (int)x, win_h - 40, 18, LIGHTGRAY);
        if (message_time > 0.0f)
        {
            int message_w = MeasureText(message, 18);
            DrawRectangle(0, win_h - 34, message_w + 20, 34,
                          (Color){30, 30, 30, 230});
            DrawText(message, 10, win_h - 26, 18, YELLOW);
        }
        EndDrawing();
        if (shot != NULL && ++frames == 5)
        {
            Image picture = LoadImageFromScreen();
            ExportImage(picture, shot);
            UnloadImage(picture);
            break;
        }
    }

    free(curve);
    UnloadTexture(texture);
    UnloadImage(logic);
    CloseWindow();
    return 0;
}
