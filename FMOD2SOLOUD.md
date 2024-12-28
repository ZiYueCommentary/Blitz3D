# FMod To SoLoud Tutorial

## I. API Change
1. **BREAKING CHANGE!** ```PlayMusic%(File$, Mode% = 0)``` -> ```PlayMusic%(File$, Volume# = 1.0)```
2. ```PlayCDTrack%(Track%, Mode% = 1)``` -> **Removed**
3. ```Load3DSound%(File$)``` <=> ```LoadSound%(File$)```
4. ```SoundLoop(sound_variable)``` -> ```SoundLoop(sound_variable, Loop% = True)```

## II. Optimization Tips
1. **Init** volume by ```SoundVolume``` or ```PlayMusic``` instead of ```ChannelVolume```

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

2. Remember to free the sounds and channels.

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

3. Stop unused channels.
    
   #### Example
   
   ```
   Function AutoReleaseSounds()
      Local Channel%

      For i = CountChannels() - 1 To 0 Step -1
         Channel = GetChannel(i)

         If Not ChannelPlaying(Channel)
            StopChannel(Channel)
         EndIf
      Next
   End Function
   ```
   
## III. New Features
1. ```ChannelLoop(channel_variable, loop% = True)```
2. ```GetChannelPause%(channel_variable)```
3. ```GetChannelPitch#(channel_variable)```
4. ```GetChannelVolume#(channel_variable)```
5. ```GetChannelPan#(channel_variable)```
6. ```GetChannelLoop%(channel_variable)```
7. ```GetChannelSampleRate%(channel_variable)```
8. ```CountSounds%()```
9. ```CountChannels%()```
10. ```GetSound%(index%)```
11. ```GetChannel%(index%)```
12. ```FreeAllSounds()```
13. ```StopAllChannels()```
14. ```VerifySound%(sound_variable)```
15. ```VerifyChannel%(channel_variable)```

## IV. Detailed introduction
### 1. Difference between ```VerifyChannel``` and ```ChannelPlaying```
```VerifyChannel``` will only check if the channel exists in the channel registry, not if the channel is playing.

```ChannelPlaying``` has the same functionality as ```VerifyChannel```, but checks to see if the audio is playing.

Example:
```
Local Channel = PlayMusic("MyMusic.ogg")

;Wait for the music to end.
While(ChannelPlaying(Channel))
   Delay(50)
Wend

Print(VerifyChannel(Channel)) ;Print "1"(True)
Print(ChannelPlaying(Channel)) ;Print "0"(False)
```