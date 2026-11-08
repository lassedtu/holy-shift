#ifndef AUDIO_VOICE_H
#define AUDIO_VOICE_H

#include <stdbool.h>

// The voices of the story: a small synth makes a sound for each vowel that the dialogue box types
// A buzz at the pitch of the speaker goes through three resonant filters (the formants), which make the vowels.
// Each vowel is a short note, the other letters, the spaces and the punctuation make no new sound
// The idea comes from the video "I Gave My NPCs Voices Without Recording Anything" by UnitOfTime

// How a speaker sounds
typedef struct VoiceStyle
{
    float pitch;   // Hz
    float formant; // the formants are multiplied by this, a higher value is a smaller (younger) voice
    float breath;  // from 0 to 1, noise in the voice
    float jitter;  // from 0 to 1, how much the pitch moves from syllable to syllable
    float loud;    // the volume of the voice
} VoiceStyle;

// Start the sound stream, nothing plays when there is no audio device
bool VoiceInit(void);
void VoiceUnload(void);

// Return the voice of a speaker ("rajka", "jelka", "luka", "zlo") with a mood ("angry", "sad", ...), the mood can be ""
// A speaker that has no voice of its own, and the narrator (""), get a neutral voice
VoiceStyle VoiceStyleFor(const char *speaker, const char *mood);

// Say one letter, a vowel sounds for a short note, until the next vowel or VoiceStop
// progress is the place of the letter in the line from 0 to 1, question is true when the line ends with "?"
void VoiceSay(const VoiceStyle *style, char letter, float progress, bool question);

// Fade the voice out
void VoiceStop(void);

#endif
