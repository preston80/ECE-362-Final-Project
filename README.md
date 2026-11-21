# ECE-362-Final-Project

Sound-tracking phone stand firmware for the Purdue Proton (RP2350) using the
Pico SDK and PlatformIO.

## Tracking behavior

- The stereo I2S microphones on GPIO 6 (BCLK), GPIO 7 (WS), and GPIO 8 (DATA)
  estimate the direction of a sound.
- The ultrasonic sensor uses GPIO 10 (TRIG) and GPIO 11 (ECHO). The servo
  distance is reported periodically and does not block audio tracking.
- The servo signal is on GPIO 18. The negated detected sound angle is added to
  the 90-degree center position with a 3x response gain. Weak audio correlation is
  rejected; accepted angles use a rolling median, exponential smoothing, and a
  stable-direction lock. A new side direction must persist across four detections
  (16 to return to center) to
  change the lock. Servo movement is limited to 30 degrees per update and holds
  between accepted detections.
- Direction is relative to the microphone pair. If servo motion goes the wrong
  way for your mechanical setup, reverse the angle mapping in `src/main.c`.

## Wiring notes

- Connect all grounds together. Power the servo from a suitable external supply
  rather than the Proton board, and connect the supply ground to board ground.
- HC-SR04-style sensors commonly drive ECHO at 5 V. Use a level shifter or
  resistor divider to bring ECHO down to a safe 3.3 V before connecting GPIO 11.
- Verify the microphone channel order and the servo's safe pulse-width range
  before mounting the phone.
