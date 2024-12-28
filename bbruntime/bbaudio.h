#ifndef BBAUDIO_H
#define BBAUDIO_H

#include "bbsys.h"
#include "../gxruntime/gxaudio.h"

extern gxAudio* gx_audio;

int bbVerifySound(gxAudio::Sound* sound);
int bbVerifyChannel(gxChannel* channel);

gxAudio::Sound* bbLoadSound(const BBStr* path);
void bbFreeSound(gxAudio::Sound* sound);
void bbLoopSound(gxAudio::Sound* sound, bool loop);
void bbSoundVolume(gxAudio::Sound* sound, float volume);
void bbSoundPan(gxAudio::Sound* sound, float pan);
void bbSoundPitch(gxAudio::Sound* sound, float pitch);

gxChannel* bbPlaySound(gxAudio::Sound* sound);
gxChannel* bbPlayMusic(const BBStr* path, float volume);

void bbStopChannel(gxChannel* channel);
void bbPauseChannel(gxChannel* channel);
void bbResumeChannel(gxChannel* channel);
void bbChannelVolume(gxChannel* channel, float volume);
void bbChannelPan(gxChannel* channel, float pan);
void bbChannelPitch(gxChannel* channel, float pitch);
void bbChannelLoop(gxChannel* channel, bool loop);
int bbChannelPlaying(gxChannel* channel);

int bbGetChannelPause(gxChannel* channel);
float bbGetChannelPitch(gxChannel* channel);
float bbGetChannelVolume(gxChannel* channel);
float bbGetChannelPan(gxChannel* channel);
int bbGetChannelLoop(gxChannel* channel);

int bbGetChannelSampleRate(gxChannel* channel);

int bbCountSounds();
int bbCountChannels();
gxAudio::Sound* bbGetSound(int index);
gxChannel* bbGetChannel(int index);
void bbFreeAllSounds();
void bbStopAllChannels();
void bbFreeAllStoppedChannels();

// ***** INTERNAL *****
gxChannel* bbPlay3dSound(gxAudio::Sound* sound, float x, float y, float z, float vx, float vy, float vz);
void bbSet3dChannel(gxChannel* channel, float x, float y, float z, float vx, float vy, float vz);
void bbSet3dListenerConfig(float roll, float dopp, float dist);
void bbSet3dListener(float x, float y, float z, float kx, float ky, float kz, float jx, float jy, float jz, float vx, float vy, float vz);

#endif