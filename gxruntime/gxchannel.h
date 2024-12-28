#ifndef GXCHANNEL_H
#define GXCHANNEL_H

class gxChannel
{
public:
    virtual ~gxChannel();
    virtual void stop() = 0;

    virtual void setPaused(bool paused) = 0;
    virtual void setPitch(float pitch) = 0;
    virtual void setVolume(float volume) = 0;
    virtual void setPan(float pan) = 0;
    virtual void setLooping(bool looping) = 0;
    virtual void set3d(float x, float y, float z, float vx, float vy, float vz) = 0;

    virtual bool getPaused() = 0;
    virtual float getPitch() = 0;
    virtual float getVolume() = 0;
    virtual float getPan() = 0;
    virtual bool getLooping() = 0;

    virtual int getSampleRate() = 0;

    virtual bool isPlaying() = 0;
};

#endif