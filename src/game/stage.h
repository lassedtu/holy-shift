#ifndef GAME_STAGE_H
#define GAME_STAGE_H

#include <stdbool.h>

#include "car/car.h"
#include "level/track.h"

typedef enum StagePhase
{
    STAGE_COUNTDOWN,
    STAGE_RUNNING,
    STAGE_FINISHED,
    STAGE_INTRO // before the countdown of the tutorial: the car waits while the player looks around
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

// With intro, the stage waits in STAGE_INTRO until StageEndIntro, else the countdown starts at once
void StageStart(Stage *stage, const Track *track, bool intro);
void StageEndIntro(Stage *stage);
void StageUpdate(Stage *stage, const Car *car, float dt);

#endif
