#ifndef UI_TYPING_H
#define UI_TYPING_H

#include <string.h>

#define TYPING_LETTER_SECONDS 0.032f // the time of a letter while a line is typed
#define TYPING_COMMA_SECONDS 0.16f   // the pause after , ; and :
#define TYPING_STOP_SECONDS 0.32f    // the pause after . ! and ?
#define TYPING_DASH_SECONDS 0.12f

// Return the time of a letter of a line before the next letter: punctuation makes a pause, so the line reads like speech
// A run of the same mark ("..." or "?!") pauses only after the last mark
static inline float TypingLetterSeconds(const char *text, int index)
{
    char letter = text[index];
    char next = text[index + 1];
    if (strchr(".!?", letter) != NULL)
    {
        return next != '\0' && strchr(".!?", next) != NULL ? TYPING_LETTER_SECONDS * 3.0f : TYPING_STOP_SECONDS;
    }
    if (strchr(",;:", letter) != NULL)
    {
        return TYPING_COMMA_SECONDS;
    }
    if (letter == '-')
    {
        return TYPING_DASH_SECONDS;
    }
    return TYPING_LETTER_SECONDS;
}

#endif
