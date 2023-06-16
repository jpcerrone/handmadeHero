# handmadeHero2

My second attempt at following https://handmadehero.org/ . A series on implementing a complete game from scratch in C without using any libraries.

The code is in a highly experimental state so expect it to be very ugly and unsctructured.

Currently on day 17.

## Things implemented so far:
* Windows message queue processing.
* Render buffered graphics to the screen using Windows GDI.
* WASAPI audio rendering. Can render a sine wave and change it's pitch in real time. (The original handmade hero uses DirectSound instead of WASAPI).
* Unified keyboard and gamepad input. Using Xinput for gamepads.
* Abstracted platform-specific code into a platform layer.
* Frame time and FPS reporting.
* Basic utility for reading and writing files.
