# FMod To SoLoud Tutorial

## I. API Change
1. ```PlayMusic%(File$, Mode% = 0)``` -> ```PlayMusic%(File$, Volume# = 1.0)```
2. ```PlayCDTrack%(Track%, Mode% = 1)``` -> **Removed**
3. ```Load3DSound%(File$)``` <=> ```LoadSound%(File$)```

## II. Optimization Tips
1. Init volume by SoundVolume or PlayMusic instead of ChannelVolume

   #### Example

   - Bad

   ```
   Local Sound = LoadSound("MySound.ogg")
   Local Channel = PlaySound(Sound)
   ChannelVolume(Channel, 0.5)
   ```

   ```
   Local Channel = PlayMusic("MySound.ogg")
   ChannelVolume(Channel, 0.5)
   ```
    
   - Optimized
    
   ```
   Local Sound = LoadSound("MySound.ogg")
   SoundVolume(Sound, 0.5)
   Local Channel = PlaySound(Sound)
   ```
    
   ```
   Local Channel = PlayMusic("MySound.ogg", 0.5)
   ```

2. Use FreeAllSounds, StopAllChannels and FreeAllStoppedChannels more often to avoid memory leaks.

   #### Example

   ```
   LoadMySounds() ; User defined function
   
   Repeat
       If KeyDown(1)
           FreeAllSounds()
           StopAllChannels()
           End()
       EndIf
   Forever
   ```
   
## III. New Features
1. ```GetChannelPause%(channel_variable)```
2. ```GetChannelPitch#(channel_variable)```
3. ```GetChannelVolume#(channel_variable)```
4. ```GetChannelPan#(channel_variable)```
5. ```GetChannelLoop%(channel_variable)```
6. ```GetChannelSampleRate%(channel_variable)```
7. ```CountSounds%()```
8. ```CountChannels%()```
9. ```GetSound%(index%)```
10. ```GetChannel%(index%)```
11. ```FreeAllSounds()```
12. ```StopAllChannels()```
13. ```FreeAllStoppedChannels()```
14. ```VerifySound%(sound_variable)```
15. ```VerifyChannel%(channel_variable)```