#include "audio/car_audio.h"

#include <math.h>
#include <stdatomic.h>

#include "audio/car_synth.h"
#include "core/config.h"

#define STREAM_BUFFER_FRAMES 1024 // about 23 ms of sound

// The audio thread reads these values, the game thread writes them
static CarSynth synth;
static float *screech_samples;
static float *gear_samples;
static _Atomic float shared_rpm;
static _Atomic float shared_throttle;
static _Atomic float shared_speed;
static _Atomic float shared_slip;
static atomic_bool shared_grass;
static _Atomic float shared_stall;
static atomic_bool shared_running;
static atomic_bool shared_cranking;
static atomic_int clunk_requests;

static void Render(void *buffer, unsigned int frames)
{
    CarSynthInput input = {
        .rpm = atomic_load(&shared_rpm),
        .throttle = atomic_load(&shared_throttle),
        .speed = atomic_load(&shared_speed),
        .slip = atomic_load(&shared_slip),
        .grass = atomic_load(&shared_grass),
        .stall = atomic_load(&shared_stall),
        .running = atomic_load(&shared_running),
        .cranking = atomic_load(&shared_cranking),
    };
    if (atomic_exchange(&clunk_requests, 0) > 0)
    {
        CarSynthClunk(&synth);
    }
    CarSynthRender(&synth, &input, buffer, (int)frames);
}

// Return the samples of a sound file in the format of the synth, or NULL when the file cannot be loaded
static float *LoadSamples(const char *path, int *count)
{
    *count = 0;
    Wave wave = LoadWave(path);
    if (!IsWaveValid(wave))
    {
        return NULL;
    }
    WaveFormat(&wave, CAR_SYNTH_SAMPLE_RATE, 32, 1);
    float *samples = LoadWaveSamples(wave);
    *count = (int)wave.frameCount;
    UnloadWave(wave);
    return samples;
}

static void Share(const Car *car, Surface surface)
{
    atomic_store(&shared_rpm, car->rpm);
    atomic_store(&shared_throttle, car->throttle);
    atomic_store(&shared_speed, fabsf(car->speed));
    atomic_store(&shared_slip, fabsf(car->slip));
    atomic_store(&shared_grass, surface == SURFACE_GRASS);
    atomic_store(&shared_stall, car->stall);
    atomic_store(&shared_running, !car->engine_off);
    atomic_store(&shared_cranking, car->starting > 0.0f);
}

bool CarAudioInit(CarAudio *audio, const Car *car)
{
    *audio = (CarAudio){0};
    if (!IsAudioDeviceReady())
    {
        return false;
    }
    CarSynthInit(&synth);
    int count = 0;
    screech_samples = LoadSamples(SOUND_SCREECH_PATH, &count);
    CarSynthSetScreech(&synth, screech_samples, count);
    gear_samples = LoadSamples(SOUND_GEAR_PATH, &count);
    CarSynthSetGear(&synth, gear_samples, count);
    Share(car, SURFACE_ROAD);
    atomic_store(&clunk_requests, 0);

    SetAudioStreamBufferSizeDefault(STREAM_BUFFER_FRAMES);
    audio->stream = LoadAudioStream(CAR_SYNTH_SAMPLE_RATE, 32, 1);
    if (!IsAudioStreamValid(audio->stream))
    {
        return false;
    }
    SetAudioStreamCallback(audio->stream, Render);
    audio->crash = LoadSound(SOUND_CRASH_PATH);
    audio->gear = car->gear;
    audio->ready = true;
    return true;
}

void CarAudioUnload(CarAudio *audio)
{
    if (audio->ready)
    {
        StopAudioStream(audio->stream);
        UnloadAudioStream(audio->stream);
    }
    if (IsSoundValid(audio->crash))
    {
        UnloadSound(audio->crash);
    }
    if (screech_samples != NULL)
    {
        UnloadWaveSamples(screech_samples);
        screech_samples = NULL;
    }
    if (gear_samples != NULL)
    {
        UnloadWaveSamples(gear_samples);
        gear_samples = NULL;
    }
    *audio = (CarAudio){0};
}

void CarAudioUpdate(CarAudio *audio, const Car *car, Surface surface)
{
    if (!audio->ready)
    {
        return;
    }
    Share(car, surface);
    if (car->gear != audio->gear)
    {
        atomic_fetch_add(&clunk_requests, 1);
    }
    audio->gear = car->gear;
}

void CarAudioCrash(CarAudio *audio)
{
    if (!audio->ready)
    {
        return;
    }
    CarAudioSetPlaying(audio, false);
    if (IsSoundValid(audio->crash))
    {
        PlaySound(audio->crash);
    }
}

void CarAudioSetPlaying(CarAudio *audio, bool playing)
{
    if (!audio->ready || playing == IsAudioStreamPlaying(audio->stream))
    {
        return;
    }
    if (playing)
    {
        PlayAudioStream(audio->stream);
    }
    else
    {
        StopAudioStream(audio->stream);
    }
}
