#include "std.h"
#include "bbaudio.h"

#include "../MultiLang/MultiLang.h"

gxAudio* gx_audio;

set<gxAudio::Sound*> sounds;
set<gxChannel*> channels;

int bbVerifySound(gxAudio::Sound* sound) {
    return sounds.contains(sound);
}

int bbVerifyChannel(gxChannel* channel) {
    return channels.contains(channel);
}

static void debugSound(gxAudio::Sound* s, const char* function) {
    if (!sounds.contains(s)) ErrorLog(function, MultiLang::sound_not_exist)
}

static void debugChannel(gxChannel* channel, const char* function) {
    if (!channels.contains(channel)) ErrorLog(function, MultiLang::sound_not_exist)
}

static void debugSoundIndex(const int index, const char* function) {
    if (index < 0 || index >= static_cast<int>(sounds.size())) ErrorLog(function, MultiLang::illegal_sound_index)
}

static void debugChannelIndex(const int index, const char* function) {
    if (index < 0 || index >= static_cast<int>(channels.size())) ErrorLog(function, MultiLang::illegal_channel_index)
}

gxAudio::Sound* bbLoadSound(const BBStr* path) {
    gxAudio::Sound* sound = gx_audio->loadSound(path->c_str());

    if (sound != nullptr) {
        sounds.insert(sound);
    }

    delete path;
    return sound;
}

void bbFreeSound(gxAudio::Sound* sound) {
    if (!bbVerifySound(sound)) return;

    sounds.erase(sound);

    delete sound;
}

void bbLoopSound(gxAudio::Sound* sound, const bool loop) {
    if (!sound) return;

    debugSound(sound, "LoopSound");

    sound->source->setLooping(loop);
}

void bbSoundVolume(gxAudio::Sound* sound, float volume) {
    if (!sound) return;

    debugSound(sound, "SoundVolume");

    volume = std::max(volume, 0.0f);
    sound->volume = volume;
}

void bbSoundPan(gxAudio::Sound* sound, const float pan) {
    if (!sound) return;

    debugSound(sound, "SoundPan");

    sound->pan = pan;
}

void bbSoundPitch(gxAudio::Sound* sound, const float pitch) {
    if (!sound) return;

    debugSound(sound, "SoundPitch");

    sound->pitch = pitch;
}

gxChannel* bbPlaySound(gxAudio::Sound* sound) {
    if (!sound) return nullptr;

    debugSound(sound, "PlaySound");

    gxChannel* channel = gx_audio->playSound(*sound);

    if (channel != nullptr) {
        channels.insert(channel);
    }

    return channel;
}

gxChannel* bbPlayMusic(const BBStr* path, float volume) {
    volume = std::max(volume, 0.0f);
    gxChannel* channel = gx_audio->playMusic(path->c_str(), volume);

    if (channel != nullptr) {
        channels.insert(channel);
    }

    delete path;
    return channel;
}

void bbStopChannel(gxChannel* channel) {
    if (!channel || !bbVerifyChannel(channel)) return;

    channel->stop();
    channels.erase(channel);

    delete channel;
}

void bbPauseChannel(gxChannel* channel) {
    if (!channel) return;

    debugChannel(channel, "PauseChannel");

    channel->setPaused(true);
}

void bbResumeChannel(gxChannel* channel) {
    if (!channel) return;

    debugChannel(channel, "ResumeChannel");

    channel->setPaused(false);
}

void bbChannelVolume(gxChannel* channel, float volume) {
    if (!channel) return;

    debugChannel(channel, "ChannelVolume");

    volume = std::max(volume, 0.0f);
    channel->setVolume(volume);
}

void bbChannelPan(gxChannel* channel, const float pan) {
    if (!channel) return;

    debugChannel(channel, "ChannelPan");

    channel->setPan(pan);
}

void bbChannelPitch(gxChannel* channel, const float pitch) {
    if (!channel) return;

    debugChannel(channel, "ChannelPitch");

    channel->setPitch(pitch);
}

void bbChannelLoop(gxChannel* channel, const bool loop) {
    if (!channel) return;

    debugChannel(channel, "ChannelLoop");

    channel->setLooping(loop);
}

int bbChannelPlaying(gxChannel* channel) {
    return bbVerifyChannel(channel) && channel->isPlaying();
}

int bbGetChannelPause(gxChannel* channel) {
    debugChannel(channel, "GetChannelPause");

    return channel->getPaused();
}

float bbGetChannelPitch(gxChannel* channel) {
    debugChannel(channel, "GetChannelPitch");

    return channel->getPitch();
}

float bbGetChannelVolume(gxChannel* channel) {
    debugChannel(channel, "GetChannelVolume");

    return channel->getVolume();
}

float bbGetChannelPan(gxChannel* channel) {
    debugChannel(channel, "GetChannelPan");

    return channel->getPan();
}

int bbGetChannelLoop(gxChannel* channel) {
    debugChannel(channel, "GetChannelLoop");

    return channel->getLooping();
}

int bbGetChannelSampleRate(gxChannel* channel) {
    debugChannel(channel, "GetChannelSampleRate");

    return channel->getSampleRate();
}

int bbCountSounds() {
    return static_cast<int>(sounds.size());
}

int bbCountChannels() {
    return static_cast<int>(channels.size());
}

gxAudio::Sound* bbGetSound(const int index) {
    debugSoundIndex(index, "GetSound");

    auto sound = sounds.begin();
    std::advance(sound, index);

    return *sound;
}

gxChannel* bbGetChannel(const int index) {
    debugChannelIndex(index, "GetChannel");

    auto channel = channels.begin();
    std::advance(channel, index);

    return *channel;
}

void bbFreeAllSounds() {
    for (const auto sound : sounds) {
        delete sound;
    }

    sounds.clear();
}

void bbStopAllChannels() {
    for (const auto channel : channels) {
        delete channel;
    }

    channels.clear();
}

gxChannel* bbPlay3dSound(gxAudio::Sound* sound, const float x, const float y, const float z, const float vx, const float vy, const float vz) {
    if (!sound) return nullptr;

    debugSound(sound, "Play3dSound");

    gxChannel* channel = gx_audio->play3dSound(*sound, x, y, z, vx, vy, vz);

    if (channel != nullptr) {
        channels.insert(channel);
    }

    return channel;
}

void bbSet3dChannel(gxChannel* channel, const float x, const float y, const float z, const float vx, const float vy, const float vz) {
    if (!channel) return;

    debugChannel(channel, "Set3dChannel");

    channel->set3d(x, y, z, vx, vy, vz);
}

void bbSet3dListenerConfig(const float roll, const float dopp, const float dist) {
    gx_audio->set3dListenerConfig(roll, dopp, dist);
}

void bbSet3dListener(const float x, const float y, const float z, const float kx, const float ky, const float kz, const float jx, const float jy, const float jz, const float vx,
    const float vy, const float vz) {
    gx_audio->set3dListener(x, y, z, kx, ky, kz, jx, jy, jz, vx, vy, vz);
}

bool audio_create() {
    gx_audio = new gxAudio();

    return true;
}

bool audio_destroy() {
    if (!gx_audio) return true;

    for (auto sound: sounds) {
        delete sound;
    }

    sounds.clear();

    for (auto channel: channels) {
        delete channel;
    }

    channels.clear();

    delete gx_audio;
    gx_audio = nullptr;

    return true;
}

void audio_link(void(*rtSym)(const char*, void*)) {
    //Sound
    rtSym("%VerifySound%sound", bbVerifySound);
    rtSym("%LoadSound$filename", bbLoadSound);
    rtSym("FreeSound%sound", bbFreeSound);
    rtSym("LoopSound%sound%loop=1", bbLoopSound);
    rtSym("SoundPitch%sound%pitch", bbSoundPitch);
    rtSym("SoundVolume%sound#volume", bbSoundVolume);
    rtSym("SoundPan%sound#pan", bbSoundPan);

    //Playing
    rtSym("%PlaySound%sound", bbPlaySound);
    rtSym("%PlayMusic$filepath#volume=1", bbPlayMusic);

    //Channel Params
    rtSym("%VerifyChannel%sound", bbVerifyChannel);
    rtSym("StopChannel%channel", bbStopChannel);
    rtSym("PauseChannel%channel", bbPauseChannel);
    rtSym("ResumeChannel%channel", bbResumeChannel);
    rtSym("ChannelPitch%channel#pitch", bbChannelPitch);
    rtSym("ChannelVolume%channel#volume", bbChannelVolume);
    rtSym("ChannelPan%channel#pan", bbChannelPan);
    rtSym("ChannelLoop%channel%loop=1", bbChannelLoop);
    rtSym("%ChannelPlaying%channel", bbChannelPlaying);
    rtSym("%GetChannelPause%channel", bbGetChannelPause);
    rtSym("#GetChannelPitch%channel", bbGetChannelPitch);
    rtSym("#GetChannelVolume%channel", bbGetChannelVolume);
    rtSym("#GetChannelPan%channel", bbGetChannelPan);
    rtSym("%GetChannelLoop%channel", bbGetChannelLoop);
    rtSym("%GetChannelSampleRate%channel", bbGetChannelSampleRate);
    rtSym("%Load3DSound$filename", bbLoadSound);

    //Misc
    rtSym("%CountSounds", bbCountSounds);
    rtSym("%CountChannels", bbCountChannels);
    rtSym("%GetSound%index", bbGetSound);
    rtSym("%GetChannel%index", bbGetChannel);
    rtSym("FreeAllSounds", bbFreeAllSounds);
    rtSym("StopAllChannels", bbStopAllChannels);
}