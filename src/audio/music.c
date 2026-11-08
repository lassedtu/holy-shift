#include "audio/music.h"

#include <math.h>
#include <stdio.h>

#include "core/config.h"

void MusicPlayerLoad(MusicPlayer *player, const char *path, float level)
{
    *player = (MusicPlayer){.level = level};
    snprintf(player->path, sizeof(player->path), "%s", path);
    if (!IsAudioDeviceReady() || !FileExists(path))
    {
        return;
    }
    player->music = LoadMusicStream(path);
    player->loaded = IsMusicValid(player->music);
    if (player->loaded)
    {
        player->music.looping = true;
    }
}

void MusicPlayerUnload(MusicPlayer *player)
{
    if (player->loaded)
    {
        UnloadMusicStream(player->music);
    }
    *player = (MusicPlayer){0};
}

void MusicPlayerUpdate(MusicPlayer *player, bool playing, float gain, float dt)
{
    if (!player->loaded)
    {
        return;
    }
    float step = dt / MUSIC_FADE_SECONDS;
    player->volume = playing ? fminf(player->volume + step, 1.0f) : fmaxf(player->volume - step, 0.0f);
    bool streaming = IsMusicStreamPlaying(player->music);
    if (player->volume > 0.0f && !streaming)
    {
        ResumeMusicStream(player->music);
        if (!IsMusicStreamPlaying(player->music))
        {
            PlayMusicStream(player->music);
        }
    }
    else if (player->volume <= 0.0f && streaming)
    {
        PauseMusicStream(player->music);
    }
    if (IsMusicStreamPlaying(player->music))
    {
        SetMusicVolume(player->music, player->volume * gain * player->level);
        UpdateMusicStream(player->music);
    }
}
