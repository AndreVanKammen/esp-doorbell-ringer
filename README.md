# esp-doorbell-ringer
Code for making a speaker go dingdong by MQTT message

You can also put an array with frequencies in a json message to play a melody.

For example twinkle twinkle:
```
{
    melody:[523,523,784,784,880,880,784,0,698,698,659,659,587,587,523,0]
}
```