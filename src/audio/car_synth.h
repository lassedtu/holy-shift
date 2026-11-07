#ifndef AUDIO_CAR_SYNTH_H
#define AUDIO_CAR_SYNTH_H

#include <stdbool.h>
#include <stdint.h>

#define CAR_SYNTH_SAMPLE_RATE 44100

// The values of the car that the sound follows
typedef struct CarSynthInput
{
    float rpm;
    float throttle; // from 0 to 1
    float speed;    // map pixels each second, always positive
    float slip;     // map pixels each second, always positive
    bool grass;     // false on the road
    float stall;    // from 0 to 1, the engine sputters before it stops
    bool running;   // false when the engine is off, also while the starter motor turns it
    bool cranking;  // true while the starter motor turns the engine
} CarSynthInput;

// The state of the sound of the car: the engine, the tyres on the ground, the slide and the screech
// The sound is made by calculation, only the screech and the gear change are recordings
typedef struct CarSynth
{
    CarSynthInput smooth; // the input after smoothing, so that changes make no clicks
    float fire_phase;     // from 0 to 1 between two firings of the engine
    float fire_time;      // seconds since the last firing
    float push_seconds;   // length of the push of the current firing
    float wander;         // slow random change of the idle speed, from -1 to 1
    float wander_target;
    float crank_phase;    // from 0 to 1 for each turn of the crankshaft
    int cylinder;
    float fire_gain;      // loudness of the current firing
    float shudder_phase;  // from 0 to 1, the shake of an engine that turns too slowly
    float limiter_phase;  // from 0 to 1, the rhythm of the rev limiter
    float limiter_gain;   // loudness that the rev limiter leaves
    float starter_phase;  // from 0 to 1, the turn of the starter motor
    float starter_level;  // loudness of the starter motor
    float resonator[3][4]; // the exhaust: the last two inputs and outputs of each resonant filter
    float engine_low;
    float dc_in;
    float dc_out;
    float ground_low[2];
    float slide_low;
    float slide_high;
    const float *screech_samples; // a loop of the screech of the tyres, mono at CAR_SYNTH_SAMPLE_RATE
    int screech_count;
    float screech_position; // sample position in the loop
    float clunk_time; // seconds since the last gear clunk, negative when there is none
    const float *gear_samples; // the sound of a gear change, mono at CAR_SYNTH_SAMPLE_RATE
    int gear_count;
    int gear_position; // sample position in the sound, gear_count when it does not play
    uint32_t noise;
} CarSynth;

void CarSynthInit(CarSynth *synth);

// Set the loop of the screech of the tyres, the synth does not copy the samples
// Without a loop, NULL, the tyres do not screech
void CarSynthSetScreech(CarSynth *synth, const float *samples, int count);

// Set the sound of a gear change, the synth does not copy the samples
// Without a sound, NULL, a short low thud is made instead
void CarSynthSetGear(CarSynth *synth, const float *samples, int count);

// Play the sound of a gear change
void CarSynthClunk(CarSynth *synth);

// Write a number of mono samples from -1 to 1
void CarSynthRender(CarSynth *synth, const CarSynthInput *input, float *out, int frames);

#endif
