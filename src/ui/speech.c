#include "ui/speech.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "audio/voice.h"
#include "core/config.h"
#include "ui/typing.h"

#define HOLD_SECONDS 1.6f   // a line stays this long after it is typed
#define SLIDE_SECONDS 0.15f
#define TEXT_SIZE 15
#define PORTRAIT_SIZE 40.0f
#define FRAME_SIZE 46.0f
#define PADDING 6.0f
#define BOTTOM_MARGIN 8.0f
#define COLOR_LINE ((Color){60, 40, 24, 255})
#define COLOR_KEYS ((Color){132, 98, 64, 255})

void SpeechInit(Speech *speech)
{
    *speech = (Speech){0};
}

void SpeechUnload(Speech *speech)
{
    for (int i = 0; i < speech->portrait_count; i++)
    {
        UnloadTexture(speech->portraits[i]);
    }
    *speech = (Speech){0};
}

void SpeechSay(Speech *speech, SpeechLine line)
{
    if (speech->count < SPEECH_QUEUE)
    {
        speech->queue[speech->count++] = line;
    }
}

void SpeechClear(Speech *speech)
{
    speech->count = 0;
    speech->typed = 0.0f;
    speech->letter_wait = 0.0f;
    speech->shown = 0.0f;
    speech->slide = 0.0f;
    speech->voiced = 0;
    VoiceStop();
}

// Take the first line away, the next line starts
static void Next(Speech *speech)
{
    for (int i = 1; i < speech->count; i++)
    {
        speech->queue[i - 1] = speech->queue[i];
    }
    speech->count--;
    speech->typed = 0.0f;
    speech->letter_wait = 0.0f;
    speech->shown = 0.0f;
    speech->voiced = 0;
}

void SpeechUpdate(Speech *speech, float dt)
{
    if (speech->count == 0)
    {
        speech->slide = fmaxf(speech->slide - dt / SLIDE_SECONDS, 0.0f);
        return;
    }
    speech->slide = fminf(speech->slide + dt / SLIDE_SECONDS, 1.0f);
    const SpeechLine *line = &speech->queue[0];
    float length = (float)strlen(line->text);
    if (speech->typed < length)
    {
        speech->letter_wait -= dt;
        while (speech->letter_wait <= 0.0f && speech->typed < length)
        {
            speech->letter_wait += TypingLetterSeconds(line->text, (int)speech->typed);
            speech->typed += 1.0f;
        }
        VoiceStyle style = VoiceStyleFor(line->speaker, line->mood);
        bool question = length > 0.0f && line->text[(int)length - 1] == '?';
        for (; speech->voiced < (int)speech->typed; speech->voiced++)
        {
            VoiceSay(&style, line->text[speech->voiced], (float)speech->voiced / length, question);
        }
        return;
    }
    speech->shown += dt;
    if (speech->shown >= HOLD_SECONDS)
    {
        Next(speech);
    }
}

// Return the portrait of a speaker with a mood, the normal portrait when the mood has none
static Texture2D Portrait(Speech *speech, const SpeechLine *line)
{
    char name[32];
    snprintf(name, sizeof(name), "%s%s%s", line->speaker, line->mood[0] != '\0' ? "_" : "", line->mood);
    for (int i = 0; i < speech->portrait_count; i++)
    {
        if (strcmp(speech->portrait_names[i], name) == 0)
        {
            return speech->portraits[i];
        }
    }
    const char *path = TextFormat("%s/%s.png", STORY_PORTRAITS_DIR, name);
    if (!FileExists(path))
    {
        path = TextFormat("%s/%s.png", STORY_PORTRAITS_DIR, line->speaker);
    }
    Texture2D texture = FileExists(path) ? LoadTexture(path) : (Texture2D){0};
    if (IsTextureValid(texture))
    {
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    }
    if (speech->portrait_count < SPEECH_PORTRAITS)
    {
        snprintf(speech->portrait_names[speech->portrait_count], 32, "%s", name);
        speech->portraits[speech->portrait_count++] = texture;
    }
    return texture;
}

void SpeechDraw(Speech *speech, const Ui *ui)
{
    if (speech->count == 0 || speech->slide <= 0.0f)
    {
        return;
    }
    const SpeechLine *line = &speech->queue[0];
    Texture2D portrait = Portrait(speech, line);
    bool has_keys = line->keys[0] != '\0';

    // The box has the size of the whole line from the start, so it does not grow while the line is typed
    float line_height = UiTextHeight(ui, TEXT_SIZE) * 0.8f;
    float text_width = fmaxf(UiTextWidth(ui, line->text, TEXT_SIZE), UiTextWidth(ui, line->keys, TEXT_SIZE));
    float height = FRAME_SIZE + 2.0f * PADDING;
    float width = text_width + 3.0f * PADDING + FRAME_SIZE + 4.0f;
    float out = 1.0f - speech->slide;
    float y = VIEW_HEIGHT - BOTTOM_MARGIN - height + out * out * (height + BOTTOM_MARGIN);
    Rectangle box = {floorf((VIEW_WIDTH - width) / 2.0f), floorf(y), floorf(width), height};
    UiPanel(ui, box);

    Rectangle frame = {box.x + PADDING, box.y + PADDING, FRAME_SIZE, FRAME_SIZE};
    UiInlay(ui, frame);
    if (IsTextureValid(portrait))
    {
        float inset = (FRAME_SIZE - PORTRAIT_SIZE) / 2.0f;
        UiTexture(ui, portrait, (Rectangle){0, 0, (float)portrait.width, (float)portrait.height},
                  (Rectangle){frame.x + inset, frame.y + inset, PORTRAIT_SIZE, PORTRAIT_SIZE}, WHITE);
    }
    float text_x = frame.x + FRAME_SIZE + PADDING;
    float lines = has_keys ? 2.0f : 1.0f;
    float text_y = box.y + (box.height - lines * line_height) / 2.0f - 2.0f;
    char typed[256];
    snprintf(typed, sizeof(typed), "%.*s", (int)speech->typed, line->text);
    UiText(ui, typed, text_x, text_y, TEXT_SIZE, COLOR_LINE);
    if (has_keys && speech->typed >= (float)strlen(line->text))
    {
        UiText(ui, line->keys, text_x, text_y + line_height, TEXT_SIZE, COLOR_KEYS);
    }
}
