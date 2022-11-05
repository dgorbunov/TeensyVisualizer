# TeensyVisualizer
### 14 Channel Music Visualizer for Teensy 4.0
*IC-based hardware spectrum analyzer with line, microphone input, and configurable display modes.*

![TeensyVisualizer in Rainbow Mode](https://github.com/dgorbunov/TeensyVisualizer/blob/main/Photos/rainbow_animated.gif)
![Top Board Layout](https://github.com/dgorbunov/TeensyVisualizer/blob/main/Photos/Render%20Top%20V1.0.svg)

An entirely on-chip hardware spectrum analyzer that displays frequency bins from either line-in or microphone input on an RGB LED Matrix display. Designed to work with HUB75 RGB LED Matrices, and scalable to any size. 14 bands across 40Hz - 16kHz are displayed in a variety of user changeable modes. Modes can be changed with the built-in button on the back of TeensyVisualizer and brightness can be adjusted with the onboard potentiometer. There are several display modes and color profiles to chose from, these settings are all non-volatile and stored into the Teensy's onboard EEPROM memory.

This PCB uses all hardware frequency analysis instead of software-based FFT like most Teensy-based audio visualizers. The two MSI MSGEQ7 Graphic Equalizer Display Filters compute frequency bins and decay at a logarathmic rate per sample, creating a much more realistic representation of audio than FFT. The onboard switch allows use of the built-in microphone or passthrough 3.5mm line input, both inputs feature adjustable gain through a dual op amp. A trick is used to offset the center frequencies of the 2 MSGEQ7 ICs by passing in two external clocks via the onboard SI5351A Clock Generator - this allows 14 independent frequencies to be sampled.

## Main components:
- Teensy 4.0 ARM MCU
- SmartMartix SmartLED Shield
- HUB75 64x32 P5 LED Matrix (scalable to any matrix size/pitch)
- 2x MSI MSGEQ7 Graphic Equalizer ICs
- Adafruit SI5351A Clock Generator
- LM358 Dual Channel Op Amp

## Features:
- 14 Channel IC-Based Hardware Spectrum Analyzer
- Automatic scaling to any matrix size, supports daisy chaining
- Switch between 3.5mm line-in passthrough or built-in microphone
- Dual channel amplifier with adjustable gain
- Built-in 5V DC jack with on/off switch
- Onboard button to switch display modes
- Onboard knob to control display brightness
- Non-volatile setting storage
- Automatic dimming when no audio detected

## PCB/Kits
PCBs can be purchased on Tindie [here](https://www.tindie.com/products/27737/). Components can all be sourced on popular suppliers like Digi-Key or Mouser by following the Bill of Materials (BOM). Gerber files are also included if you want to fabricate the PCB yourself.

Full Kits include everything you need to build your own TeensyVisualizer, they are available for purchase through Tindie [here](https://www.tindie.com/products/27748/).

## Build It Yourself
The Bill of Materials can be found here: [BOM](https://github.com/dgorbunov/TeensyVisualizer/tree/main/BOM).
The Schematic can be found here: [Schematic](https://github.com/dgorbunov/TeensyVisualizer/blob/main/Schematic/Schematic%20V1.1.svg).
![Schematic Image](https://github.com/dgorbunov/TeensyVisualizer/blob/main/Schematic/Schematic%20V1.1.svg)

## 3D Printed Case
![TeensyVisaulizer Case](https://github.com/dgorbunov/TeensyVisualizer/blob/main/Photos/Case%20V1.0.png)
The 3D Printed Case is included with the Kit, or you can print it yourself. This case is designed for the standard 320mm x 160mm 64x32 P5 LED Matrix.
[STL Download](https://github.com/dgorbunov/TeensyVisualizer/raw/main/Case/TeensyVisualizer%20Case%20V1.0.stl).

## Credits
Originally inspired by Mark Donner's 14 Channel Analyzer based off the ATMega2560 using FastLED. Circuit redesigned from the ground up around Teensy and 3.3V using SmartMatrix HUB75 panels and programmed from scratch.

The following open-source libraries are used:
- SmartMatrix
- SmartMatrix GFX
- FastLED
- MegunoLink
- SI5351MCU
