#include "audio/voice.h"

#include <ctype.h>
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#include "raylib.h"

#define SAMPLE_RATE 44100
#define BUFFER_FRAMES 512 // about 12 ms, so a letter sounds soon after it is typed
#define TWO_PI 6.2831853f
#define DT (1.0f / SAMPLE_RATE)
#define GLIDE_SECONDS 0.012f // the formants and the pitch move to a new letter in this time
#define NOTE_SECONDS 0.05f   // a vowel sounds this long, or until the next vowel
#define LEVEL_SECONDS 0.006f // the loudness moves this fast, short enough for clear letters, long enough for no clicks
#define VOICE_GAIN 0.33f
#define SYLLABLE_JITTER 0.07f // the largest random change of the pitch for each vowel, before the jitter of the style
#define FALL 0.12f            // a sentence falls by this part of the pitch to its end
#define RISE 0.3f             // a question rises by this part of the pitch at its end

// The targets of the synth, the game thread writes them and the audio thread reads them
static _Atomic float target_pitch;
static _Atomic float target_f1;
static _Atomic float target_f2;
static _Atomic float target_f3;
static _Atomic float target_voiced; // loudness of the buzz
static _Atomic float target_breath;
static atomic_int notes; // each new vowel adds one, the audio thread starts a new note
static AudioStream stream;
static bool ready;

// The state of the audio thread
typedef struct Synth
{
    float pitch, f1, f2, f3, voiced, breath;
    float phase;
    float note; // seconds that the vowel still sounds
    int last_note;
    float formant[3][4]; // the last two inputs and outputs of each resonant filter
    float low;
    uint32_t random;
} Synth;

static Synth synth;

static float Noise(void)
{
    uint32_t x = synth.random;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    synth.random = x;
    return (float)(x >> 8) / 8388608.0f - 1.0f;
}

static float Approach(float value, float target, float seconds)
{
    return value + (target - value) * (1.0f - expf(-DT / seconds));
}

// A band pass filter with a gain of about 1 at its centre, the same filter as the exhaust of the car synth
static float Resonate(float *state, float x, float hz, float width_hz)
{
    float r = expf(-3.1415927f * width_hz * DT);
    float y = 0.5f * (1.0f - r * r) * (x - state[1]) + 2.0f * r * cosf(TWO_PI * hz * DT) * state[2] - r * r * state[3];
    state[1] = state[0];
    state[0] = x;
    state[3] = state[2];
    state[2] = y;
    return y;
}

static void Render(void *buffer, unsigned int frames)
{
    float *out = buffer;
    float pitch = atomic_load(&target_pitch);
    float f1 = atomic_load(&target_f1);
    float f2 = atomic_load(&target_f2);
    float f3 = atomic_load(&target_f3);
    float voiced = atomic_load(&target_voiced);
    float breath = atomic_load(&target_breath);
    int note = atomic_load(&notes);
    if (note != synth.last_note)
    {
        synth.last_note = note;
        synth.note = NOTE_SECONDS;
    }
    for (unsigned int i = 0; i < frames; i++)
    {
        synth.pitch = Approach(synth.pitch, pitch, GLIDE_SECONDS);
        synth.f1 = Approach(synth.f1, f1, GLIDE_SECONDS);
        synth.f2 = Approach(synth.f2, f2, GLIDE_SECONDS);
        synth.f3 = Approach(synth.f3, f3, GLIDE_SECONDS);
        synth.voiced = Approach(synth.voiced, synth.note > 0.0f ? voiced : 0.0f, LEVEL_SECONDS);
        synth.note -= DT;
        synth.breath = Approach(synth.breath, breath, GLIDE_SECONDS);

        // The buzz of the vocal folds: a falling saw wave, with a little breath in it
        synth.phase += synth.pitch * DT;
        synth.phase -= floorf(synth.phase);
        float source = (1.0f - 2.0f * synth.phase) + Noise() * synth.breath;
        float vowel = 3.0f * Resonate(synth.formant[0], source, synth.f1, 90.0f) +
                      2.2f * Resonate(synth.formant[1], source, synth.f2, 120.0f) +
                      1.2f * Resonate(synth.formant[2], source, synth.f3, 160.0f);
        float sample = vowel * synth.voiced;

        // A soft low pass, so the voice is warm and not harsh
        synth.low += (sample - synth.low) * 0.45f;
        out[i] = tanhf(synth.low * VOICE_GAIN);
    }
}

bool VoiceInit(void)
{
    ready = false;
    if (!IsAudioDeviceReady())
    {
        return false;
    }
    synth = (Synth){.pitch = 180.0f, .f1 = 500.0f, .f2 = 1500.0f, .f3 = 2500.0f, .random = 0x2545F491u};
    atomic_store(&target_pitch, 180.0f);
    atomic_store(&target_f1, 500.0f);
    atomic_store(&target_f2, 1500.0f);
    atomic_store(&target_f3, 2500.0f);
    atomic_store(&target_voiced, 0.0f);
    atomic_store(&target_breath, 0.0f);
    atomic_store(&notes, 0);
    SetAudioStreamBufferSizeDefault(BUFFER_FRAMES);
    stream = LoadAudioStream(SAMPLE_RATE, 32, 1);
    SetAudioStreamBufferSizeDefault(0); // the size is for all new streams, the music must keep the normal size
    if (!IsAudioStreamValid(stream))
    {
        return false;
    }
    SetAudioStreamCallback(stream, Render);
    PlayAudioStream(stream);
    ready = true;
    return true;
}

void VoiceUnload(void)
{
    if (ready)
    {
        StopAudioStream(stream);
        UnloadAudioStream(stream);
    }
    ready = false;
}

typedef struct NamedStyle
{
    const char *name;
    VoiceStyle style;
} NamedStyle;

static const NamedStyle SPEAKERS[] = {
    {"rajka", {205.0f, 1.08f, 0.22f, 0.6f, 0.9f}}, // an old woman, a bit breathy and shaky
    {"jelka", {185.0f, 1.05f, 0.12f, 0.35f, 0.9f}},
    {"luka", {165.0f, 1.18f, 0.08f, 0.5f, 0.85f}}, // a boy of fifteen, a small voice that is not high anymore
    {"zlo", {105.0f, 0.9f, 0.04f, 0.15f, 0.95f}},  // deep, smooth and even
};

VoiceStyle VoiceStyleFor(const char *speaker, const char *mood)
{
    VoiceStyle style = {150.0f, 1.0f, 0.05f, 0.15f, 0.8f}; // the narrator and other speakers: neutral and even
    for (size_t i = 0; i < sizeof(SPEAKERS) / sizeof(SPEAKERS[0]); i++)
    {
        if (strcmp(speaker, SPEAKERS[i].name) == 0)
        {
            style = SPEAKERS[i].style;
        }
    }
    if (strcmp(mood, "angry") == 0)
    {
        style.pitch *= 1.12f;
        style.loud *= 1.15f;
        style.jitter += 0.2f;
    }
    else if (strcmp(mood, "sad") == 0 || strcmp(mood, "cold") == 0)
    {
        style.pitch *= 0.9f;
        style.loud *= 0.8f;
        style.jitter *= 0.5f;
    }
    else if (strcmp(mood, "scared") == 0)
    {
        style.pitch *= 1.15f;
        style.jitter += 0.5f;
    }
    else if (strcmp(mood, "sick") == 0)
    {
        style.pitch *= 0.95f;
        style.breath += 0.3f;
        style.loud *= 0.85f;
    }
    else if (strcmp(mood, "smile") == 0 || strcmp(mood, "smug") == 0 || strcmp(mood, "brave") == 0)
    {
        style.pitch *= 1.05f;
    }
    return style;
}

// The formants of the vowels, in Hz, for an adult voice
typedef struct Vowel
{
    char letter;
    float f1, f2, f3;
} Vowel;

static const Vowel VOWELS[] = {
    {'a', 800, 1250, 2500},
    {'e', 500, 1850, 2500},
    {'i', 320, 2300, 3000},
    {'o', 500, 900, 2400},
    {'u', 350, 760, 2300},
    {'y', 320, 2200, 2900},
};

static uint32_t say_random = 0x9E3779B9u;

static float RandomSigned(void)
{
    say_random = say_random * 1664525u + 1013904223u;
    return (float)((say_random >> 8) & 0xFFFF) / 32767.5f - 1.0f;
}

void VoiceSay(const VoiceStyle *style, char letter, float progress, bool question)
{
    if (!ready)
    {
        return;
    }
    char lower = (char)tolower((unsigned char)letter);
    const Vowel *vowel = NULL;
    for (size_t i = 0; i < sizeof(VOWELS) / sizeof(VOWELS[0]); i++)
    {
        if (VOWELS[i].letter == lower)
        {
            vowel = &VOWELS[i];
        }
    }
    if (vowel == NULL)
    {
        // Only the vowels sound, the last vowel goes on through the other letters until its note ends
        return;
    }

    // The sentence falls to its end, a question rises at its end, each vowel moves a little at random
    float melody = question ? 1.0f + RISE * progress * progress : 1.0f - FALL * progress;
    float jitter = SYLLABLE_JITTER * (0.5f + style->jitter);
    atomic_store(&target_pitch, style->pitch * melody * (1.0f + jitter * RandomSigned()));
    atomic_store(&target_f1, vowel->f1 * style->formant);
    atomic_store(&target_f2, vowel->f2 * style->formant);
    atomic_store(&target_f3, vowel->f3 * style->formant);
    atomic_store(&target_voiced, style->loud);
    atomic_store(&target_breath, style->breath);
    atomic_fetch_add(&notes, 1);
}

void VoiceStop(void)
{
    atomic_store(&target_voiced, 0.0f);
}
