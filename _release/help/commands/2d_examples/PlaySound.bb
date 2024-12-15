; Assign a global variable for the sound
Global sound

; Load the sound file into memory

sound = LoadSound("sounds/die.wav")

; If the sound is valid, play the sound, otherwise tell the player that the sound file is invalid.

If VerifySound(sound)
    PlaySound(sound)
Else
    Print("Oops, the sound file is invalid.")
EndIf