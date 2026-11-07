#ifndef GAME_STAGE_H
#define GAME_STAGE_H

#include "car/car.h"
#include "level/track.h"

typedef enum StagePhase
{
    STAGE_COUNTDOWN,
    STAGE_RUNNING,
    STAGE_FINISHED
} StagePhase;

// One run of a level: a countdown, then the timer runs until the car gets to the finish
// The finish is the end of an open track, or one lap of a closed track
typedef struct Stage
{
    const Track *track;
    StagePhase phase;
    float countdown; // seconds until the start
    float time;      // seconds since the start
    float progress;  // distance that the car drove along the track, a closed track does not wrap
} Stage;

void StageStart(Stage *stage, const Track *track);
void StageUpdate(Stage *stage, const Car *car, float dt);

#endif
