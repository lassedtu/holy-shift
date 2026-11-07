#ifndef AUDIO_CAR_AUDIO_H
#define AUDIO_CAR_AUDIO_H

#include <stdbool.h>

#include "car/car.h"
#include "level/surface.h"
#include "raylib.h"

// The sound of the car, played on a raylib audio stream
// The audio device must be ready before CarAudioInit
typedef struct CarAudio
{
    AudioStream stream;
    bool ready;
    int gear; // the gear in the last update, a change plays the gear sound
    Sound crash;
} CarAudio;

// Return false when the stream cannot play, then the game continues without sound
bool CarAudioInit(CarAudio *audio, const Car *car);
void CarAudioUnload(CarAudio *audio);

// Give the state of the car to the sound, once each frame
void CarAudioUpdate(CarAudio *audio, const Car *car, Surface surface);

// Play the sound of a crash and stop the sound of the car
// CarAudioSetPlaying starts the sound of the car again
void CarAudioCrash(CarAudio *audio);

// Stop or start the sound, for example in the fly mode
void CarAudioSetPlaying(CarAudio *audio, bool playing);

#endif
