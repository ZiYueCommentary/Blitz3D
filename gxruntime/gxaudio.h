#ifndef GXAUDIO_H
#define GXAUDIO_H

#include "gxchannel.h"
#include "../soloud/soloud.h"
#include "../soloud/soloud_wavstream.h"

typedef unsigned int channel_handle;

class gxAudio
{
public:

    struct Sound
    {
        SoLoud::AudioSource* source;
        float volume = 1.0f;
        float pan = 0.0f;
        float pitch = 0.0f;

        ~Sound() { delete source; }
    };

    struct SoundChannel : gxChannel
    {
        const gxAudio& gx_audio;
        channel_handle handle;

        SoundChannel(const gxAudio& gx_audio, const channel_handle handle) : gx_audio(gx_audio), handle(handle) { }

        ~SoundChannel() override;

        void stop() override;

        void setPaused(bool paused) override;
        void setPitch(float pitch) override;
        void setVolume(float volume) override;
        void setPan(float pan) override;
        void setLooping(bool looping) override;
        void set3d(float x, float y, float z, float vx, float vy, float vz) override;

        bool getPaused() override;
        float getPitch() override;
        float getVolume() override;
        float getPan() override;
        bool getLooping() override;

        int getSampleRate() override;

        bool isPlaying() override;
    };

    struct StreamChannel : SoundChannel
    {
        SoLoud::WavStream* stream;

        StreamChannel(const gxAudio& gx_audio, SoLoud::WavStream* stream, const channel_handle handle) : SoundChannel(gx_audio, handle), stream(stream) { }

        ~StreamChannel() override;
    };

    SoLoud::Soloud* soloud = nullptr;

    gxAudio();
    ~gxAudio();

    void set3dListenerConfig(float roll, float dopp, float dist);
    void set3dListener(float x, float y, float z, float kx, float ky, float kz, float jx, float jy, float jz, float vx, float vy, float vz);
    Sound* loadSound(const char* path);
    SoundChannel* playSound(const Sound& sound) const;
    SoundChannel* play3dSound(const Sound& sound, float x, float y, float z, float vx, float vy, float vz) const;
    StreamChannel* playMusic(const char* path, float volume = 1.0f) const;

private:
    float rolloff_factor = 1.0f;
    float doppler_factor = 1.0f;
    float distance_factor = 1.0f;
};

#endif