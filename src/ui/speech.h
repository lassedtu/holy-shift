#ifndef UI_SPEECH_H
#define UI_SPEECH_H

#include <stdbool.h>

#include "raylib.h"
#include "ui/ui.h"

#define SPEECH_QUEUE 6
#define SPEECH_PORTRAITS 16

// One thing that a character says while the player drives
typedef struct SpeechLine
{
    const char *speaker; // "rajka" or "luka", the name of the portrait and the voice
    const char *mood;    // "" or a mood of the portraits, for example "angry"
    const char *text;
    const char *keys;    // a second line in another color that tells the player what to do, "" for none
} SpeechLine;

// The lines of the characters while driving, in a small box at the bottom of the view with a portrait
// The lines are typed and said by the voice synth, one after the other
typedef struct Speech
{
    SpeechLine queue[SPEECH_QUEUE];
    int count;          // lines in the queue, the first one is on the screen
    float typed;        // letters of the first line that are shown
    float letter_wait;  // seconds until the next letter
    float shown;        // seconds since the whole line is shown
    float slide;        // from 0 (out of the view) to 1 (in its place)
    int voiced;         // letters of the first line that the voice has said
    char portrait_names[SPEECH_PORTRAITS][32];
    Texture2D portraits[SPEECH_PORTRAITS];
    int portrait_count;
} Speech;

void SpeechInit(Speech *speech);
void SpeechUnload(Speech *speech);

// Add a line to the queue, the line is dropped when the queue is full
void SpeechSay(Speech *speech, SpeechLine line);

// Take away all the lines, for example when the stage starts again
void SpeechClear(Speech *speech);

void SpeechUpdate(Speech *speech, float dt);
void SpeechDraw(Speech *speech, const Ui *ui);

#endif
