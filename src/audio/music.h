#ifndef AUDIO_MUSIC_H
#define AUDIO_MUSIC_H

#include <stdbool.h>

#include "raylib.h"

// One music track that fades in and out, for example the theme of the menus
// The track pauses when it is silent, so it goes on from the same place the next time
typedef struct MusicPlayer
{
    Music music;
    bool loaded;  // false when the file cannot be loaded or there is no audio device, then nothing plays
    float level;  // the volume of the track at full volume
    float volume; // from 0 to 1, the fade
    char path[256]; // the file of the track, "" when there is none
} MusicPlayer;

// Load a track that loops, the game goes on without music when it fails
// The level is the volume of the track at full volume, from 0 to 1
void MusicPlayerLoad(MusicPlayer *player, const char *path, float level);
void MusicPlayerUnload(MusicPlayer *player);

// Call once each frame, the track fades in while playing is true and fades out while it is false
// The gain (from 0 to 1) is the music volume of the settings
void MusicPlayerUpdate(MusicPlayer *player, bool playing, float gain, float dt);

#endif
