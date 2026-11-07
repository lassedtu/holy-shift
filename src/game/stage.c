#include "game/stage.h"

#define COUNTDOWN_SECONDS 3.0f
#define PROGRESS_WINDOW 120.0f // the car cannot move more than this along the track in one frame

void StageStart(Stage *stage, const Track *track)
{
    *stage = (Stage){0};
    stage->track = track;
    stage->phase = STAGE_COUNTDOWN;
    stage->countdown = COUNTDOWN_SECONDS;
}

static void UpdateProgress(Stage *stage, const Car *car)
{
    const Track *track = stage->track;
    float here = TrackWrap(track, stage->progress);
    float delta = TrackNearest(track, car->position, here, PROGRESS_WINDOW) - here;
    if (track->closed && delta > track->length / 2.0f)
    {
        delta -= track->length;
    }
    else if (track->closed && delta < -track->length / 2.0f)
    {
        delta += track->length;
    }
    stage->progress += delta;
}

void StageUpdate(Stage *stage, const Car *car, float dt)
{
    switch (stage->phase)
    {
    case STAGE_COUNTDOWN:
        stage->countdown -= dt;
        if (stage->countdown <= 0.0f)
        {
            stage->phase = STAGE_RUNNING;
        }
        break;
    case STAGE_RUNNING:
        stage->time += dt;
        UpdateProgress(stage, car);
        if (stage->progress >= stage->track->length - 1.0f)
        {
            stage->phase = STAGE_FINISHED;
        }
        break;
    case STAGE_FINISHED:
        break;
    }
}
