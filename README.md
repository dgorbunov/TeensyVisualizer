
# TeensyVisualizer
### 14 Channel Audio Spectrum Analyzer for Teensy 4.0
![Top Board Layout](https://github.com/dgorbunov/TeensyVisualizer/blob/main/Photos/Render%20Top%20V1.0.svg)

Main components:
- Teensy 4.0 MCU
- SmartLED Shield
- HUB75 Compatible LED Matrix
- MSGEQ7 Graphic Equalizer Display Filters
- Adafruit Si5351A Clock Generator

An audio spectrum analyzer that displays frequency bins of line-in or built-in microphone input on an RGB LED Matrix display. 14 peak-detected bands are displayed across a 40Hz - 16kHz range creating a satisfying light show.

This PCB uses all hardware frequency analysis instead of software-based FFT like most Teensy-based audio visualizers. The two MSGEQ7 ICs compute frequency bins and factor in exponential decay, creating a much more realistic representation of audio than traditional FFT approaches. The onboard switch allows use of the built-in microphone or 3.5mm audio input+output, both feature adjustable gain independent from the input volume.

You can adjust the brightness of the display with the onboard rubberized knob, and press the built-in button to change display modes. You can cycle between different types of spectrum analyzers and different color gradients, and your settings are all stored in permanent memory so you don't have to cycle back to them every time the unit powers on. The board also features automatic shut off when no input is detected for convenience.

## Features:
- Retro look, hardware based, clock-adjustable 14 channel audio visualizer
- Automatic scaling to display size
- Supports 3.5mm audio input with direct output, or built-in microphone with input switch
- Dual amplifiers with adjustable gain
- Built-in 5V DC jack with on/off switch
- Onboard button to switch display modes
- Onboard knob to control display brightness
- Non-volatile setting storage
- Automatic shut off when no input detected




## Buy PCB/Kit
PCBs and component kits are available for purchase through [tindie.com](https://www.tindie.com/products/27737/).
Fully-assembled units are available per request.

## Build It Yourself
[BOM](https://github.com/dgorbunov/TeensyVisualizer/tree/main/BOM)
[Schematic](https://github.com/dgorbunov/TeensyVisualizer/blob/main/Schematic/Schematic%20V1.0.svg)
![Schematic Image](https://github.com/dgorbunov/TeensyVisualizer/blob/main/Schematic/Schematic%20V1.0.svg)

## Credits
Originally inspired by Mark Donner's 14 Channel Analyzer based off the ATMega2560 using FastLED. Circuitry redesigned from the ground up around Teensy and 3.3V using SmartMatrix HUB75 panels and programmed from scratch.

The following open-source libraries are used:
- SmartMatrix
- FastLED
- Si5351MCU
- MegunoLink Filter
