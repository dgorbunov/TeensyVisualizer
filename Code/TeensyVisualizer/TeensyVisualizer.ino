/* 
 *  TeensyVisualizer, 14 Channel Audio Visualizer for Teensy 4.0 using HUB75 LED Matrices.
 *  Author: Daniel Gorbunov. Instructions, PCB, Schematics, and Code available on GitHub.
 *  Code v2.01, November 2022. MIT License. github.com/dgorbunov/TeensyVisualizer
 *  
 *  The following libraries must be installed to use this software: 
 *  SmartMatrix, SmartMatrix GFX, FastLED, MegunoLink, SI5351MCU
 */

#include <MatrixHardware_Teensy4_ShieldV5.h>  // Choose based on your Teensy and shield version
#include <EEPROM.h>
#include <SmartMatrix.h>
#include <FastLED.h>
#include <si5351mcu.h>
#include <MegunoLink.h>
#include <Filter.h>
#include <map>

#define COLOR_DEPTH 24                  // Choose the color depth used for storing pixels in the layers: 24 or 48 (24 is good for most sketches)
const uint16_t kMatrixWidth = 64;       // Set to the width of your display, must be a multiple of 8
const uint16_t kMatrixHeight = 32;      // Set to the height of your display
const uint8_t kRefreshDepth = 48;       // Tradeoff of color quality vs refresh rate, max brightness, and RAM usage.  36 is typically good, drop down to 24 if you need to.  On Teensy, multiples of 3, up to 48: 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48.
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save RAM, more to keep from dropping frames and automatically lowering refresh rate.  (This isn't used on ESP32, leave as default)
const uint8_t kPanelType = SM_PANELTYPE_HUB75_32ROW_MOD16SCAN;  // Choose the configuration that matches your panels.  See more details in MatrixCommonHub75.h and the docs: https://github.com/pixelmatix/SmartMatrix/wiki
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_NONE);        // see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);

#define NOISE 70             // 0-1023, noise subtracted from signaal
#define NOISE_TOLERANCE 1.2  // NOISE_TOLERANCE * NOISE = minimum noise to go into idle
#define SMOOTHING 72         // 100 = no smoothing, 0 = full smoothing
#define TARGET_FPS 28        // 0-60, frames per second. Higher FPS = faster decay due to more sampling
#define ADDRESS_MODE 0       // Memory address for currentMode, default 0
#define ADDRESS_COLOR 1      // Memory address for color, default 1

// Required values for TeensyVisualizer PCB
#define BRIGHTNESS_PIN 16  // Brightness potentiometer
#define MODE_PIN 17        // currentMode pushbutton
#define STROBE_PIN 22      // MSGEQ7 combined strobe
#define RESET_PIN 23       // MSGEQ7 combined reset
#define OUT_PIN_0 14       // MSGEQ7_0 output (this is A0/14 on Teensy 4.0)
#define OUT_PIN_1 15       // MSGEQ7_1 output (this is A1/15 on Teensy 4.0)

// SI5351A Frequencies - default for 14 channel sweep
uint32_t CLK_FREQ_0 = 104570; // MSGEQ7 clock freq. 0
uint32_t CLK_FREQ_1 = 166280; // MSGEQ7 clock freq. 1 

int binWidth = 14;                // Calculates width based on display size
int binPadding = 4;               // Calculates padding to center graphics
unsigned long timeQuiet = 0;
bool isQuiet = false;
int correctedFPS = TARGET_FPS;
float currentBrightness = 0;
float startBrightness = 0;

bool buttonPressed = false;           // Stores whether button has been pressed
bool modeChanged = false;             // Stores whether mode has been changed
unsigned long timeButtonPressed = 0;  // Initial time that the button was pressed
unsigned long modeSwitchTime = 650;   // Time required holding button to change currentMode

unsigned long waveCycleTime = 4000; // Gradient wave cycle time
unsigned long waveStartTime = 0;    // Gradient wave start timer

// TODO: Bezier curves
std::map<int, String> modes = 
{
  {0, "Lines"}, 
  {1, "Blocks"},
  {2, "Levels"},
  {3, "Bounce"},
};

// TODO: Rainbow hue based on peak height
std::map<int, String> colors = 
{
  {0, "Rainbow"},
  {1, "Wave"},
  {2, "Heat"}
};

unsigned int currentMode = 0;
unsigned int currentColor = 0;

Si5351mcu si;
ExponentialFilter<int> * BinFilters[14];

void setup() {
  Serial.begin(115200);

  // Initialize matrix
  matrix.addLayer(&backgroundLayer); 
  matrix.begin();
  backgroundLayer.enableColorCorrection(true);

  // Draw startup graphics
  backgroundLayer.fillScreen({0,0,0});
  backgroundLayer.swapBuffers();
  backgroundLayer.setFont(font6x10);
  backgroundLayer.drawString(14, 1, {212,66,245}, "Teensy");
  backgroundLayer.drawString(2, 11, {212,66,245}, "Visualizer");
  backgroundLayer.setFont(font3x5);
  backgroundLayer.drawString(14, 24, {255,255,255}, "14 Channel");
  backgroundLayer.swapBuffers();

  // Set binWidth and padding based on matrix size
  binWidth = (kMatrixWidth / 14);
  binPadding = (kMatrixWidth - (binWidth * 14)) / 2;

  // Create 14 ExponentialFilters for frequency bins
  for (int i = 0; i < 14; i++) {
    BinFilters[i] =  new ExponentialFilter<int>(SMOOTHING, 0);
  }

  // Write to EEPROM if blank, otherwise read mode and color data
  if (EEPROM.read(ADDRESS_MODE) == 0xFF) {
    EEPROM.write(ADDRESS_MODE, currentMode);
  } else {
    currentMode = EEPROM.read(ADDRESS_MODE);
  }
  
  if (EEPROM.read(ADDRESS_COLOR) == 0xFF) {
    EEPROM.write(ADDRESS_COLOR, currentColor);
  } else {
    currentColor = EEPROM.read(ADDRESS_COLOR);
  }

  // Setup pins
  pinMode(MODE_PIN, INPUT);
  pinMode(BRIGHTNESS_PIN, INPUT);
  pinMode(STROBE_PIN, OUTPUT);
  pinMode(RESET_PIN, OUTPUT);
  pinMode(OUT_PIN_0, INPUT);
  pinMode(OUT_PIN_1, INPUT);

  // MSGEQ7 initial state
  digitalWrite(RESET_PIN,  LOW);
  digitalWrite(STROBE_PIN, LOW);

  // Setup SI5351
  si.init(25000000L);
  si.setFreq(0, CLK_FREQ_0);
  si.setFreq(1, CLK_FREQ_1);
  si.setPower(0, SIOUT_8mA);
  si.setPower(1, SIOUT_8mA);
  si.enable(0);
  si.enable(1);

  // Loading Animation
  for (int i = 0; i < kMatrixWidth; i++) {
    delay(35);
    backgroundLayer.fillRectangle(i, kMatrixHeight - 1, i, kMatrixHeight - 2, {255,255,255});
    backgroundLayer.swapBuffers();
  }
  
  backgroundLayer.fillScreen({0,0,0});
  backgroundLayer.swapBuffers(false);
}

void loop() {
  unsigned long loopStartTime = millis();
  
  readFrequencies();
  autoDim();

  // If FPS below target, draw white pixel in upper right
  if (correctedFPS < TARGET_FPS - 2) {
    backgroundLayer.drawPixel(kMatrixWidth - 1, 0, {255,255,255});
    Serial.println("Low FPS");
  }
  
  int buttonState = digitalRead(MODE_PIN);
  if (buttonState && !buttonPressed) {
    buttonPressed = true;
    modeChanged = false;
    timeButtonPressed = millis();
  } 
  // button depress
  else if (!buttonState && buttonPressed && loopStartTime - timeButtonPressed <= modeSwitchTime) {
    buttonPressed = false;

    // color change case
    if (!modeChanged) {
      currentColor = (currentColor == colors.size() - 1) ? 0 : currentColor + 1;
      EEPROM.write(ADDRESS_COLOR, currentColor);
    } else {
      // if mode was changed, don't also change color on button depress
      modeChanged = false;
    }
  } 

  // mode change case
  else if (buttonPressed && loopStartTime - timeButtonPressed > modeSwitchTime) {
    timeButtonPressed = millis();
    modeChanged = true;
    
    currentMode = (currentMode == modes.size() - 1) ? 0 : currentMode + 1;
    EEPROM.write(ADDRESS_MODE, currentMode);
  }

  String color = colors.at(currentColor);
  Serial.println("Mode: " + modes.at(currentMode));
  Serial.println("Color: " + color);

  switch(currentMode) {
    case 0:
      lines(false, color);
      break;
    case 1:
      blocks(true, color);
      break;
    case 2:
      blocks(false, color);
      break;
    case 3:
      bounce(false, color);
      break;
    default:
      lines(false, color);
      break;
  }
  
  // Swap layer to render buffer
  backgroundLayer.swapBuffers(false);

  // Add delay to correct FPS to target
  int uncorrectedFPS = 1.0 / ((millis() - loopStartTime) / 1000.0);
  if (uncorrectedFPS > TARGET_FPS) {
    delay((1.0 / TARGET_FPS - 1.0 / uncorrectedFPS) * 1000.0);
  }
  
  correctedFPS = 1.0 / ((millis() - loopStartTime) / 1000.0);
  Serial.println("FPS: " + (String) correctedFPS);
}

// Lines drawn between the peaks of each channel
void lines(boolean filled, String color){
  SM_RGB currentColor = CRGB(0,0,0);
  
  for (int i = 0, x = binPadding / 2; i < 15; i++, x += binWidth) {
    int peak = getPeakValue(i-1);
    int nextPeak = getPeakValue(i);
    
    if (color == "Heat") {
      if (i == 0) currentColor = CRGB(CHSV(100 - nextPeak * 3.2, 255, 255));
      else currentColor = CRGB(CHSV(100 - peak * 3.2, 255, 255));
    } else if (color == "Wave") {
      currentColor = CRGB(CHSV((i) * 16 + getWaveModifier(), 255, 255));
    } else {
      currentColor = CRGB(CHSV((i) * 16, 255, 255));
    }

    backgroundLayer.drawLine(x, (kMatrixHeight - 1) - peak, x + (binWidth - 1), (kMatrixHeight - 1) - nextPeak, currentColor);
  }
}

// Version of lines that expands outward in mirror effect from center
void bounce(boolean filled, String color) {
  SM_RGB currentColor = CRGB(0,0,0);

  for (int i = 0, x = binPadding / 2; i < 15; i++, x += binWidth) {
    int peak = getPeakValue(i-1);
    int nextPeak = getPeakValue(i);
    
    if (color == "Heat") {
      if (i == 0) currentColor = CRGB(CHSV(100 - nextPeak * 3.2, 255, 255));
      else currentColor = CRGB(CHSV(100 - peak * 3.2, 255, 255));
    } else if (color == "Wave") {
      currentColor = CRGB(CHSV((i) * 16 + getWaveModifier(), 255, 255));
//      SM_RGB currentColorPhased = CRGB(CHSV((i) * 16 + getWaveModifier() + 127, 255, 255));
    } else {
      currentColor = CRGB(CHSV((i) * 16, 255, 255));
    }

    backgroundLayer.drawLine(x, (kMatrixHeight / 2) - 1 - (peak / 2), x + (binWidth - 1), (kMatrixHeight / 2) - 1 - (nextPeak / 2), currentColor); // draw upwards facing lines
    backgroundLayer.drawLine(x, (kMatrixHeight / 2) + (peak / 2), x + (binWidth - 1), (kMatrixHeight / 2) + (nextPeak / 2), currentColor); // draw downwards facing lines
  }
}

// Blocks of binWidth drawn up to the peaks of each channel
// TODO: Peak detection/falling peaks
void blocks(boolean filled, String color) {
  SM_RGB currentColor = CRGB(0,0,0);
  
  for (int i = 0, x = binPadding; i < 14; i++, x += binWidth) {
    int peak = getPeakValue(i);
    
    if (color == "Heat") {
      currentColor = CRGB(CHSV(100 - peak * 3.2, 255, 255));
    } else if (color == "Wave") {
      currentColor = CRGB(CHSV((i) * 17 + getWaveModifier(), 255, 255));
    } else {
      currentColor = CRGB(CHSV((i) * 17, 255, 255));
    }    
    
    if (filled) backgroundLayer.fillRectangle(x, kMatrixHeight - 1, x + binWidth - 1, kMatrixHeight - 1 - peak, currentColor);
    else backgroundLayer.drawLine(x, kMatrixHeight - 1 - peak, x + binWidth - 1, kMatrixHeight - 1 - peak, currentColor);
  }
}

// algorithm from http://www.netgraphics.sk/bresenham-algorithm-for-a-line
void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, const rgb24 color) {
    // if point x1, y1 is on the right side of point x2, y2, change them
    if ((x1 - x2) > 0) {
        drawLine(x2, y2, x1, y1, color);
        return;
    }
    // test inclination of line
    // function Math.abs(y) defines absolute value y
    if (abs(y2 - y1) > abs(x2 - x1)) {
        // line and y axis angle is less then 45 degrees
        // thats why go on the next procedure
        bresteepline(y1, x1, y2, x2, color); return;
    }
    // line and x axis angle is less then 45 degrees, so x is guiding
    // auxiliary variables
    int x = x1, y = y1, sum = x2 - x1, Dx = 2 * (x2 - x1), Dy = abs(2 * (y2 - y1));
    int prirastokDy = ((y2 - y1) > 0) ? 1 : -1;
    // draw line
    for (int i = 0; i <= x2 - x1; i++) {
        backgroundLayer.drawPixel(x, y, color);
        x++;
        sum -= Dy;
        if (sum < 0) {
            y = y + prirastokDy;
            sum += Dx;
        }
    }
}

void bresteepline(int16_t x3, int16_t y3, int16_t x4, int16_t y4, const rgb24 color) {
    // if point x3, y3 is on the right side of point x4, y4, change them
    if ((x3 - x4) > 0) {
        bresteepline(x4, y4, x3, y3, color);
        return;
    }

    int x = x3, y = y3, sum = x4 - x3,  Dx = 2 * (x4 - x3), Dy = abs(2 * (y4 - y3));
    int prirastokDy = ((y4 - y3) > 0) ? 1 : -1;

    for (int i = 0; i <= x4 - x3; i++) {
        backgroundLayer.drawPixel(y, x, color);
        x++;
        sum -= Dy;
        if (sum < 0) {
            y = y + prirastokDy;
            sum += Dx;
        }
    }
}

int getPeakValue(int index) {
  if (index < 0 || index >= 14) return 0;
  return ((max((BinFilters[index]->Current() - NOISE), 0) / 1023.0) * kMatrixHeight);
}

float getWaveModifier() {
  unsigned long elapsedTime = millis() - waveStartTime;
  
  if (waveStartTime == 0 || elapsedTime >= waveCycleTime) {
    waveStartTime = millis();
    return 0;
  } else {
    return elapsedTime * (255.0 / waveCycleTime);
  }
}

// Detect the noisefloor and go dark if no sound is being played
void autoDim() {
  float avgLevel = 0;
  unsigned long fadeTime = 2000;
  unsigned long waitToFadeTime = 8000;
  
  for (int i = 0; i < 14; i++) {
    avgLevel += BinFilters[i]->Current();
  }

  avgLevel /= 14.0;

  if (!isQuiet && avgLevel < NOISE * NOISE_TOLERANCE) {
    timeQuiet = millis();
    isQuiet = true;
    
  } else if (isQuiet && avgLevel >= NOISE * NOISE_TOLERANCE) {
    isQuiet = false;
  }
  
  backgroundLayer.fillScreen({0,0,0});
  int userBrightness = ceil(analogRead(BRIGHTNESS_PIN) / 1023.0 * 255);
  
  if (isQuiet && millis() - timeQuiet > waitToFadeTime) {
    if (millis() - timeQuiet > waitToFadeTime + fadeTime) {
      currentBrightness = 0;
    } else if (currentBrightness > 0) {
      currentBrightness -= userBrightness / (fadeTime / (1000.0/correctedFPS)); // brightness / (fadeTime / (ms per frame))
    }
    
  } else {
    currentBrightness = (userBrightness < 2) ? 0 : userBrightness;
  }

  matrix.setBrightness(max(currentBrightness, 0)); //limit brightness to positives only
}

// Read and bin frequencies from MSGEQ7 chips
void readFrequencies() {
  int levelBin[2][7];
  
  // Initial state for MSGEQ7
  digitalWrite(STROBE_PIN,  HIGH);
  digitalWrite(RESET_PIN,  HIGH);
  digitalWrite(RESET_PIN, LOW);
  delayMicroseconds(75);

  // Pulse the strobe to cycle through frequency bands
  for (int i = 0; i < 7; i++) {
    digitalWrite(STROBE_PIN, LOW);
    delayMicroseconds(40);
    
    levelBin[0][i] = analogRead(OUT_PIN_0);
    levelBin[1][i] = analogRead(OUT_PIN_1);
    
    digitalWrite(STROBE_PIN, HIGH);
    delayMicroseconds(40);
  }
  
  // Assemble 14 bands of filtered amplitudes
  for (int i = 0, n = 0; i < 14; i++) {
    if (i % 2 == 0) {
      BinFilters[i]->Filter(levelBin[0][n]);
    } else {
      BinFilters[i]->Filter(levelBin[1][n++]);
    }
  }
}
