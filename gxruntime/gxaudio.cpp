#include "gxaudio.h"
#include "../bbruntime/bbsys.h"
#include "../soloud/soloud_c.h"
#include "../soloud/soloud_wav.h"

gxAudio::SoundChannel::~SoundChannel() {
    if (!SoundChannel::isPlaying()) return;

    SoundChannel::stop();
}

void gxAudio::SoundChannel::stop() {
    if (!SoundChannel::isPlaying()) return;

    gx_audio.soloud->stop(handle);
}

void gxAudio::SoundChannel::setPaused(const bool paused) {
    if (!SoundChannel::isPlaying()) return;

    gx_audio.soloud->setPause(handle, paused);
}

void gxAudio::SoundChannel::setPitch(const float pitch) {
    if (!SoundChannel::isPlaying()) return;

    gx_audio.soloud->setRelativePlaySpeed(handle, pitch);
}

void gxAudio::SoundChannel::setVolume(const float volume) {
    if (!SoundChannel::isPlaying()) return;

    gx_audio.soloud->setVolume(handle, volume);
}

void gxAudio::SoundChannel::setPan(const float pan) {
    if (!SoundChannel::isPlaying()) return;

    gx_audio.soloud->setPan(handle, pan);
}

void gxAudio::SoundChannel::setLooping(const bool looping) {
    if (!SoundChannel::isPlaying()) return;

    gx_audio.soloud->setLooping(handle, looping);
}

void gxAudio::SoundChannel::set3d(const float x, const float y, const float z, const float vx, const float vy, const float vz) {
    if (!SoundChannel::isPlaying()) return;

    gx_audio.soloud->set3dSourceParameters(handle, x * gx_audio.distance_factor, y * gx_audio.distance_factor, z * gx_audio.distance_factor,
                                  vx * gx_audio.distance_factor, vy * gx_audio.distance_factor, vz * gx_audio.distance_factor);
}

bool gxAudio::SoundChannel::getPaused() {
    if (!SoundChannel::isPlaying()) return true;

    return gx_audio.soloud->getPause(handle);
}

float gxAudio::SoundChannel::getPitch() {
    if (!SoundChannel::isPlaying()) return 0.0f;

    return gx_audio.soloud->getRelativePlaySpeed(handle);
}

float gxAudio::SoundChannel::getVolume() {
    if (!SoundChannel::isPlaying()) return 0.0f;

    return gx_audio.soloud->getVolume(handle);
}

float gxAudio::SoundChannel::getPan() {
    if (!SoundChannel::isPlaying()) return 0.0f;

    return gx_audio.soloud->getPan(handle);
}

bool gxAudio::SoundChannel::getLooping() {
    if (!SoundChannel::isPlaying()) return false;

    return gx_audio.soloud->getLooping(handle);
}

int gxAudio::SoundChannel::getSampleRate() {
    if (!SoundChannel::isPlaying()) return 0;

    return static_cast<int>(gx_audio.soloud->getSamplerate(handle));
}

bool gxAudio::SoundChannel::isPlaying() {
    return gx_audio.soloud->isValidVoiceHandle(handle);
}

gxAudio::StreamChannel::~StreamChannel() {
    if (StreamChannel::isPlaying()) {
        StreamChannel::stop();
    }

    delete stream;
}

gxAudio::gxAudio() {
    soloud = new SoLoud::Soloud();

    if (soloud->init(SOLOUD_LEFT_HANDED_3D) != SoLoud::SO_NO_ERROR) {
        delete soloud;
        soloud = nullptr;
    }
}

gxAudio::~gxAudio() {
    if (!soloud) return;

    soloud->deinit();
    delete soloud;
    soloud = nullptr;
}

void gxAudio::set3dListenerConfig(const float roll, const float dopp, const float dist) {
    rolloff_factor = roll;
    doppler_factor = dopp;
    distance_factor = dist;
}

void gxAudio::set3dListener(const float x, const float y, const float z, const float kx, const float ky, const float kz, float jx, const float jy, const float jz,
    const float vx, const float vy, const float vz) {
    if (!soloud) return;

    soloud->set3dListenerParameters(x * distance_factor, y * distance_factor, z * distance_factor, kx, ky, kz, jy, jy, jz, vx * distance_factor, vy * distance_factor, vz * distance_factor);
    soloud->update3dAudio();
}

gxAudio::Sound* gxAudio::loadSound(const char* path) {
    const auto sound = new SoLoud::Wav();

    if (sound->load(path) != SoLoud::SO_NO_ERROR) {
        delete sound;
        return nullptr;
    }

    return new Sound{.source = sound};
}

gxAudio::SoundChannel* gxAudio::playSound(const Sound& sound) const {
    if (!soloud) return nullptr;

    const channel_handle handle = soloud->play(*sound.source, sound.volume, sound.pan, sound.pitch >= EPSILON);
    const auto channel = new SoundChannel(*this, handle);

    if (sound.pitch >= EPSILON) {
        channel->setPitch(sound.pitch);
        channel->setPaused(false);
    }

    return channel;
}

gxAudio::SoundChannel* gxAudio::play3dSound(const Sound& sound, const float x, const float y, const float z, const float vx, const float vy, const float vz) const {
    if (!soloud) return nullptr;

    sound.source->set3dAttenuation(SoLoud::AudioSource::INVERSE_DISTANCE, rolloff_factor);
    sound.source->set3dDopplerFactor(doppler_factor);

    const channel_handle handle = soloud->play3d(*sound.source, x, y, z, vx, vy, vz, sound.volume, false, 0);
    const auto channel = new SoundChannel(*this, handle);

    if (sound.pitch >= EPSILON) {
        channel->setPitch(sound.pitch);
        channel->setPaused(false);
    }

    return channel;
}

gxAudio::StreamChannel* gxAudio::playMusic(const char* path, const float volume) const {
    if (!soloud) return nullptr;

    const auto stream = new SoLoud::WavStream();

    if (stream->load(path) != SoLoud::SO_NO_ERROR) {
        delete stream;
        return nullptr;
    }

    const channel_handle handle = soloud->play(*stream, volume);
    const auto channel = new StreamChannel{*this, stream, handle};

    return channel;
}