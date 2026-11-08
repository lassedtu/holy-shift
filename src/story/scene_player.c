#include "story/scene_player.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/config.h"
#include "ui/hud.h"

#define PICTURE_WIDTH (VIEW_WIDTH / STORY_PICTURE_SCALE) // picture pixels on the screen
#define DIALOGUE_BAND 120 // height of the dark area for the dialogue at the bottom, in game pixels
#define PICTURE_HEIGHT ((VIEW_HEIGHT - DIALOGUE_BAND) / STORY_PICTURE_SCALE) // picture pixels above the dark area
#define BOARD_TEXT_SIZE 15
#define CROSS_SECONDS 0.5f // a new picture fades in over the last picture in this time
#define FADE_SECONDS 0.6f  // the screen fades to black or back in this time
#define ALONE_SECONDS 1.6f // a new picture shows without the dialogue box for this time, the fade in is part of it
#define BOX_SLIDE_SECONDS 0.25f
#define LETTERS_PER_SECOND 45.0f
#define TEXT_SIZE 27
#define HINT_SIZE 15
#define LINE_SPACING 0.8f // the distance between two lines of text, as part of the size of the text
#define MAX_LINES 8

// The dialogue box and the portrait, in game pixels
#define BOX_MARGIN 16.0f // from the sides of the screen, the box is in the middle of the dark area
#define BOX_HEIGHT 104.0f
#define PORTRAIT_FRAME 88.0f // the portrait is drawn in a sunken frame of this size
#define PORTRAIT_SIZE 80.0f
#define BOX_PADDING 12.0f

#define COLOR_INK ((Color){60, 40, 24, 255})
#define COLOR_HINT ((Color){255, 255, 255, 140})
#define COLOR_MARK ((Color){150, 122, 88, 255}) // the mark that the line is complete
#define BASELINE 0.78f // the bottom of a capital letter, as part of the height of the text
#define COLOR_BACKDROP ((Color){34, 24, 20, 255}) // behind the picture, while it fades in
#define COLOR_LINE ((Color){40, 26, 18, 255})     // the line between the picture and the dialogue area

// Return the index of a loaded texture, load it when it is new, -1 when the file cannot be loaded
static int TextureIndex(ScenePlayer *player, const char *dir, const char *name)
{
    char key[SCENE_MAX_NAME + 16];
    snprintf(key, sizeof(key), "%s/%s", dir, name);
    for (int i = 0; i < player->texture_count; i++)
    {
        if (strcmp(player->names[i], key) == 0)
        {
            return i;
        }
    }
    const char *path = TextFormat("%s/%s", dir, name);
    if (player->texture_count == SCENE_MAX_TEXTURES || !FileExists(path))
    {
        return -1;
    }
    Texture2D texture = LoadTexture(path);
    if (!IsTextureValid(texture))
    {
        return -1;
    }
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    snprintf(player->names[player->texture_count], SCENE_MAX_NAME, "%s", key);
    player->textures[player->texture_count] = texture;
    return player->texture_count++;
}

// Load all the pictures and the portraits of the scene before it starts, so the scene does not stop to load
static bool LoadTextures(ScenePlayer *player, char *message, size_t size)
{
    for (int i = 0; i < player->scene.count; i++)
    {
        const SceneCommand *command = &player->scene.commands[i];
        player->portraits[i] = -1;
        if (command->type == SCENE_IMAGE && TextureIndex(player, STORY_IMAGES_DIR, command->name) < 0)
        {
            snprintf(message, size, "Could not load %s/%s", STORY_IMAGES_DIR, command->name);
            return false;
        }
        if (command->type == SCENE_LINE && command->name[0] != '\0')
        {
            int portrait = TextureIndex(player, STORY_PORTRAITS_DIR, TextFormat("%s.png", command->name));
            if (portrait < 0)
            {
                portrait = TextureIndex(player, STORY_PORTRAITS_DIR, TextFormat("%s.png", command->fallback));
            }
            player->portraits[i] = portrait;
        }
    }
    return true;
}

bool ScenePlayerStart(ScenePlayer *player, const char *name, char *message, size_t size)
{
    *player = (ScenePlayer){0};
    if (!SceneLoad(&player->scene, name, message, size))
    {
        return false;
    }
    player->portraits = malloc((size_t)(player->scene.count > 0 ? player->scene.count : 1) * sizeof(int));
    if (player->portraits == NULL || !LoadTextures(player, message, size))
    {
        if (player->portraits == NULL)
        {
            snprintf(message, size, "Not enough memory for the scene %s", name);
        }
        ScenePlayerStop(player);
        return false;
    }
    player->current.texture = SCENE_PICTURE_NONE;
    player->previous.texture = SCENE_PICTURE_NONE;
    player->cross = 1.0f;
    player->black = 1.0f; // the scene fades in from black
    return true;
}

void ScenePlayerStop(ScenePlayer *player)
{
    for (int i = 0; i < player->texture_count; i++)
    {
        UnloadTexture(player->textures[i]);
    }
    free(player->portraits);
    SceneUnload(&player->scene);
    *player = (ScenePlayer){0};
}

void ScenePlayerSetBoard(ScenePlayer *player, const Standings *standings, const char *title)
{
    player->board = *standings;
    snprintf(player->board_title, sizeof(player->board_title), "%s", title);
    player->has_board = true;
}

bool ScenePlayerDone(const ScenePlayer *player)
{
    return player->done;
}

static Vector2 PictureSlack(const ScenePlayer *player, const ScenePicture *picture)
{
    if (picture->texture < 0)
    {
        return (Vector2){0.0f, 0.0f};
    }
    Texture2D texture = player->textures[picture->texture];
    return (Vector2){fmaxf((float)(texture.width - PICTURE_WIDTH), 0.0f),
                     fmaxf((float)(texture.height - PICTURE_HEIGHT), 0.0f)};
}

static void ShowPicture(ScenePlayer *player, const SceneCommand *command)
{
    player->previous = player->current;
    int texture = command->type == SCENE_BOARD ? SCENE_PICTURE_BOARD
                                               : TextureIndex(player, STORY_IMAGES_DIR, command->name);
    player->current = (ScenePicture){.texture = texture};
    Vector2 slack = PictureSlack(player, &player->current);
    Vector2 offset = {slack.x / 2.0f, slack.y / 2.0f};
    switch (command->anchor)
    {
    case SCENE_ANCHOR_TOP:
        offset.y = 0.0f;
        break;
    case SCENE_ANCHOR_BOTTOM:
        offset.y = slack.y;
        break;
    case SCENE_ANCHOR_LEFT:
        offset.x = 0.0f;
        break;
    case SCENE_ANCHOR_RIGHT:
        offset.x = slack.x;
        break;
    case SCENE_ANCHOR_MIDDLE:
        break;
    }
    player->current.offset = offset;
    player->alone = ALONE_SECONDS;

    // After a fade the new picture comes in from black, else it fades in over the last picture
    if (player->black_target > 0.0f || player->previous.texture == SCENE_PICTURE_NONE)
    {
        player->cross = 1.0f;
        player->black_target = 0.0f;
    }
    else
    {
        player->cross = 0.0f;
    }
}

static void StartPan(ScenePlayer *player, ScenePan pan, float seconds)
{
    ScenePicture *picture = &player->current;
    if (picture->texture < 0)
    {
        return;
    }
    Vector2 slack = PictureSlack(player, picture);
    Vector2 middle = {slack.x / 2.0f, slack.y / 2.0f};
    Vector2 from = middle;
    Vector2 to = middle;
    switch (pan)
    {
    case SCENE_PAN_LEFT:
        from.x = slack.x;
        to.x = 0.0f;
        break;
    case SCENE_PAN_RIGHT:
        from.x = 0.0f;
        to.x = slack.x;
        break;
    case SCENE_PAN_UP:
        from.y = slack.y;
        to.y = 0.0f;
        break;
    case SCENE_PAN_DOWN:
        from.y = 0.0f;
        to.y = slack.y;
        break;
    }
    picture->pan_from = from;
    picture->pan_to = to;
    picture->offset = from;
    picture->pan_time = 0.0f;
    picture->pan_seconds = seconds;
}

// Run the commands that do not wait, until a command that waits or the end of the scene
static void Run(ScenePlayer *player)
{
    while (player->index < player->scene.count)
    {
        const SceneCommand *command = &player->scene.commands[player->index];
        switch (command->type)
        {
        case SCENE_IMAGE:
        case SCENE_BOARD:
            ShowPicture(player, command);
            break;
        case SCENE_PAN:
            StartPan(player, command->pan, command->seconds);
            break;
        case SCENE_FADE:
            player->black_target = 1.0f;
            if (player->black < 1.0f)
            {
                return;
            }
            break;
        case SCENE_MUSIC:
            snprintf(player->music, sizeof(player->music), "%s", command->name);
            break;
        case SCENE_WAIT:
            if (!player->started)
            {
                player->started = true;
                player->wait = command->seconds;
            }
            return;
        case SCENE_LINE:
            if (!player->started)
            {
                player->started = true;
                player->typed = 0.0f;
            }
            return;
        }
        player->index++;
        player->started = false;
    }
    player->done = true;
}

static void Next(ScenePlayer *player)
{
    player->index++;
    player->started = false;
    Run(player);
}

static void UpdatePan(ScenePicture *picture, float dt)
{
    if (picture->texture < 0 || picture->pan_seconds <= 0.0f)
    {
        return;
    }
    picture->pan_time = fminf(picture->pan_time + dt, picture->pan_seconds);
    float t = picture->pan_time / picture->pan_seconds;
    t = t * t * (3.0f - 2.0f * t); // the picture starts and stops slowly
    picture->offset = (Vector2){picture->pan_from.x + (picture->pan_to.x - picture->pan_from.x) * t,
                                picture->pan_from.y + (picture->pan_to.y - picture->pan_from.y) * t};
}

void ScenePlayerUpdate(ScenePlayer *player, float dt, bool advance, bool skip)
{
    if (player->done)
    {
        return;
    }
    if (skip)
    {
        player->done = true;
        return;
    }
    player->time += dt;
    player->cross = fminf(player->cross + dt / CROSS_SECONDS, 1.0f);
    float fade_step = dt / FADE_SECONDS;
    player->black += fmaxf(fminf(player->black_target - player->black, fade_step), -fade_step);
    player->alone = fmaxf(player->alone - dt, 0.0f);
    UpdatePan(&player->current, dt);
    Run(player);
    if (player->done)
    {
        return;
    }

    const SceneCommand *command = &player->scene.commands[player->index];
    if (command->type == SCENE_FADE && player->black >= 1.0f)
    {
        Next(player);
    }
    else if (command->type == SCENE_WAIT)
    {
        player->wait -= dt;
        if (player->wait <= 0.0f || advance)
        {
            Next(player);
        }
    }
    if (command->type != SCENE_LINE)
    {
        player->box_in = 0.0f;
    }
    else if (player->alone > 0.0f)
    {
        if (advance)
        {
            player->alone = 0.0f;
        }
    }
    else if (player->box_in < 1.0f)
    {
        player->box_in = fminf(player->box_in + dt / BOX_SLIDE_SECONDS, 1.0f);
        if (advance)
        {
            player->box_in = 1.0f;
        }
    }
    else
    {
        float length = (float)strlen(command->text);
        player->typed = fminf(player->typed + dt * LETTERS_PER_SECOND, length);
        if (advance && player->typed < length)
        {
            player->typed = length;
        }
        else if (advance)
        {
            Next(player);
        }
    }
}

// A timing board on a dark wall: the place, the name, the time and the gap to the winner of each driver
// The row of the player is lit
static void DrawBoard(const ScenePlayer *player, const Ui *ui, float alpha)
{
    const Color wall = {28, 30, 34, 255};
    const Color board = {14, 15, 18, 255};
    const Color frame = {70, 72, 78, 255};
    const Color amber = {255, 186, 72, 255};
    const Color dim = {150, 112, 52, 255};
    const Color lit = {255, 224, 140, 255};

    float band = VIEW_HEIGHT - DIALOGUE_BAND;
    UiFill(ui, (Rectangle){0, 0, VIEW_WIDTH, band}, Fade(wall, alpha));
    float width = 330.0f;
    float row_height = 19.0f;
    float height = 44.0f + STANDINGS_DRIVERS * row_height + 10.0f;
    Rectangle box = {floorf((VIEW_WIDTH - width) / 2.0f), floorf((band - height) / 2.0f), width, height};
    UiFill(ui, (Rectangle){box.x - 4.0f, box.y - 4.0f, box.width + 8.0f, box.height + 8.0f}, Fade(frame, alpha));
    UiFill(ui, box, Fade(board, alpha));

    float middle = box.x + box.width / 2.0f;
    UiTextCentered(ui, TextFormat("%s - RESULTS", player->board_title), middle, box.y + 8.0f, BOARD_TEXT_SIZE,
                   Fade(amber, alpha));
    UiFill(ui, (Rectangle){box.x + 10.0f, box.y + 34.0f, box.width - 20.0f, 1.0f}, Fade(dim, alpha));
    if (!player->has_board)
    {
        return;
    }
    float left = box.x + 16.0f;
    float right = box.x + box.width - 16.0f;
    float y = box.y + 42.0f;
    float winner = player->board.rows[0].time;
    for (int i = 0; i < STANDINGS_DRIVERS; i++)
    {
        const StandingsRow *row = &player->board.rows[i];
        Color ink = row->player ? board : amber;
        if (row->player)
        {
            UiFill(ui, (Rectangle){box.x + 8.0f, y - 1.0f, box.width - 16.0f, row_height}, Fade(lit, alpha));
        }
        UiText(ui, TextFormat("%d", i + 1), left, y - 2.0f, BOARD_TEXT_SIZE, Fade(ink, alpha));
        UiText(ui, row->name, left + 26.0f, y - 2.0f, BOARD_TEXT_SIZE, Fade(ink, alpha));
        const char *time = HudFormatTime(row->time);
        UiText(ui, time, right - 70.0f - UiTextWidth(ui, time, BOARD_TEXT_SIZE), y - 2.0f, BOARD_TEXT_SIZE,
               Fade(ink, alpha));
        const char *gap = i == 0 ? "" : TextFormat("+%.2f", row->time - winner);
        UiText(ui, gap, right - UiTextWidth(ui, gap, BOARD_TEXT_SIZE), y - 2.0f, BOARD_TEXT_SIZE,
               Fade(row->player ? board : dim, alpha));
        y += row_height;
    }
}

static void DrawPicture(const ScenePlayer *player, const Ui *ui, const ScenePicture *picture, float alpha)
{
    if (picture->texture == SCENE_PICTURE_BOARD && alpha > 0.0f)
    {
        DrawBoard(player, ui, alpha);
        return;
    }
    if (picture->texture < 0 || alpha <= 0.0f)
    {
        return;
    }
    Texture2D texture = player->textures[picture->texture];
    float width = fminf((float)texture.width, PICTURE_WIDTH);
    float height = fminf((float)texture.height, PICTURE_HEIGHT);
    // The offset moves in whole picture pixels, so the pixels do not change their shape
    Rectangle source = {floorf(picture->offset.x), floorf(picture->offset.y), width, height};
    Rectangle rect = {(PICTURE_WIDTH - width) / 2.0f * STORY_PICTURE_SCALE,
                      (PICTURE_HEIGHT - height) / 2.0f * STORY_PICTURE_SCALE, width * STORY_PICTURE_SCALE,
                      height * STORY_PICTURE_SCALE};
    UiTexture(ui, texture, source, rect, Fade(WHITE, alpha));
}

// Break a text into lines that fit a width, return the number of lines
static int WrapText(const Ui *ui, const char *text, float width, char lines[MAX_LINES][SCENE_MAX_TEXT])
{
    int count = 0;
    lines[0][0] = '\0';
    const char *word = text;
    while (*word != '\0' && count < MAX_LINES)
    {
        while (*word == ' ')
        {
            word++;
        }
        size_t length = strcspn(word, " ");
        if (length == 0)
        {
            break;
        }
        char candidate[SCENE_MAX_TEXT];
        const char *line = lines[count];
        snprintf(candidate, sizeof(candidate), "%s%s%.*s", line, line[0] != '\0' ? " " : "", (int)length, word);
        if (line[0] != '\0' && UiTextWidth(ui, candidate, TEXT_SIZE) > width)
        {
            if (++count == MAX_LINES)
            {
                break;
            }
            snprintf(lines[count], SCENE_MAX_TEXT, "%.*s", (int)length, word);
        }
        else
        {
            snprintf(lines[count], SCENE_MAX_TEXT, "%s", candidate);
        }
        word += length;
    }
    return lines[0][0] == '\0' ? 0 : (count < MAX_LINES ? count + 1 : MAX_LINES);
}

static void DrawDialogue(const ScenePlayer *player, const Ui *ui, const SceneCommand *command, int portrait)
{
    float slide = 1.0f - player->box_in;
    slide = slide * slide * DIALOGUE_BAND; // the box slows down at the end
    float band_top = VIEW_HEIGHT - DIALOGUE_BAND;
    Rectangle box = {BOX_MARGIN, band_top + (DIALOGUE_BAND - BOX_HEIGHT) / 2.0f + slide, VIEW_WIDTH - 2.0f * BOX_MARGIN,
                     BOX_HEIGHT};
    UiPanel(ui, box);
    float text_x = box.x + BOX_PADDING;
    if (portrait >= 0)
    {
        float frame_gap = (BOX_HEIGHT - PORTRAIT_FRAME) / 2.0f;
        Rectangle frame = {box.x + frame_gap, box.y + frame_gap, PORTRAIT_FRAME, PORTRAIT_FRAME};
        UiInlay(ui, frame);
        Texture2D texture = player->textures[portrait];
        float inset = (PORTRAIT_FRAME - PORTRAIT_SIZE) / 2.0f;
        UiTexture(ui, texture, (Rectangle){0, 0, (float)texture.width, (float)texture.height},
                  (Rectangle){frame.x + inset, frame.y + inset, PORTRAIT_SIZE, PORTRAIT_SIZE}, WHITE);
        text_x = frame.x + PORTRAIT_FRAME + BOX_PADDING;
    }

    static char lines[MAX_LINES][SCENE_MAX_TEXT];
    float width = box.x + box.width - BOX_PADDING - text_x;
    int count = WrapText(ui, command->text, width, lines);
    float line_height = UiTextHeight(ui, TEXT_SIZE) * LINE_SPACING;
    int left = (int)player->typed;
    for (int i = 0; i < count && left > 0; i++)
    {
        int length = (int)strlen(lines[i]);
        UiText(ui, TextFormat("%.*s", length < left ? length : left, lines[i]), text_x,
               box.y + BOX_PADDING - 4.0f + (float)i * line_height, TEXT_SIZE, COLOR_INK);
        left -= length + 1;
    }

    // A blinking mark when the whole line is shown
    if (player->typed >= (float)strlen(command->text) && fmodf(player->time, 1.0f) < 0.6f)
    {
        // The bottom of the letter is at BASELINE of the height of the text
        float y = box.y + box.height - BOX_PADDING - UiTextHeight(ui, TEXT_SIZE) * BASELINE;
        UiText(ui, ">", box.x + box.width - BOX_PADDING - 8.0f, y, TEXT_SIZE, COLOR_MARK);
    }
}

void ScenePlayerDraw(const ScenePlayer *player, const Ui *ui)
{
    // The dialogue area is a brown panel, with a dark line between it and the picture
    float band_top = VIEW_HEIGHT - DIALOGUE_BAND;
    UiFill(ui, (Rectangle){0, 0, VIEW_WIDTH, band_top}, COLOR_BACKDROP);
    UiPanelDark(ui, (Rectangle){0, band_top, VIEW_WIDTH, DIALOGUE_BAND});
    if (player->cross < 1.0f)
    {
        DrawPicture(player, ui, &player->previous, 1.0f);
    }
    DrawPicture(player, ui, &player->current, player->cross);
    UiFill(ui, (Rectangle){0, band_top - 1.0f, VIEW_WIDTH, 3.0f}, COLOR_LINE);
    if (player->black > 0.0f)
    {
        UiFill(ui, (Rectangle){0, 0, VIEW_WIDTH, VIEW_HEIGHT}, Fade(BLACK, player->black));
    }
    if (!player->done && player->index < player->scene.count)
    {
        const SceneCommand *command = &player->scene.commands[player->index];
        if (command->type == SCENE_LINE && player->started && player->alone <= 0.0f)
        {
            DrawDialogue(player, ui, command, player->portraits[player->index]);
        }
    }
    const char *hint = "ESC to skip";
    UiText(ui, hint, VIEW_WIDTH - 8.0f - UiTextWidth(ui, hint, HINT_SIZE), 6.0f, HINT_SIZE, COLOR_HINT);
}
