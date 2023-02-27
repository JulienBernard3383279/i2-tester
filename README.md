# i2-tester - Controller spoofing firmware for input integrity measurements

This is a firmware to upload to a Raspberry Pi Pico in order to create a device to test input integrity.

You should connect a GCC plug to it (check pico-rectangle for longer explanation, I assume here you're familiar with this stuff, and basics of coding) and
then you'll be able to configure the code in `main.cpp` to make the Gamecube controller enter certain states at precise intervals.
This is more of a 'notepad' kind of project at the moment. Alternative useful code is commented or deactivated via macros.

By default, I have it enter Y then A 34 frame lengths apart (the DK test). You should then output the audio of your Switch to your PC, ideally via a HDMI
capture card ($15), though a direct line in works too (but has slight noise).

You'll then need to analyze the output (python file attached).

You'll see in addition to the GCC state changes, there are GPIO changes. This is here to test pro controllers. Testing pro controllers is significantly more difficult.
You'll need to take them apart, wire the data lines to 2 buttons, and connect the ground. You must also not charge the pro con while you're testing or inputs go to shit
(somehow). Not in a timing way, in a voltage way (don't worry about normal usage with it charging).

The EMULATE_PLAYER_BEHAVIOUR controls the 'randomness' of the timing. When it's off the code does the perfect input. But when it's on it allows you to test a more
realistic scenario: what's the success rate if the player accuracy is a normal law centered on the perfect timing so that they succeed X% of the time ?

Note this is done by sweeping over [0 -> 1] (starting at 0.5) as an argument to the inverse normal law cumulative distribution function. So you must only take
into account a block of measures that's a multiple of the number of measures in one sweep to have meaningful results. (By default this is 200)
