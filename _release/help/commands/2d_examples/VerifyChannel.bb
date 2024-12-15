;First, load and play the music.

Local channel = PlayMusic("music.ogg")

;Check the music channel whether is valid or not.

If VerifyChannel(channel)
    Print("Good job! The music is playing successfully.")
    Delay(5000)

    ;Now, stop the channel.
    StopChannel(channel)

    ;Print the channel whether is valid or not.
    ;If everything is OK. It should print "0"(False) because channel was stopped.
    Print(VerifyChannel(channel))
Else
    Print("The music is invalid.")
    ;You don't need to call StopChannel because it will automatically release if the music doesn't play successfully.
EndIf