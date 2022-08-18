#include <MatrixHardware_Teensy4_ShieldV5.h>  // Choose based on your Teensy and shield version
#include <EEPROM.h>
#include <SmartMatrix.h>
#include <FastLED.h>
#include <si5351mcu.h>
#include <MegunoLink.h>
#include <Filter.h>

#define COLOR_DEPTH 24                  // Choose the color depth used for storing pixels in the layers: 24 or 48 (24 is good for most sketches - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24)
const uint16_t kMatrixWidth = 64;       // Set to the width of your display, must be a multiple of 8
const uint16_t kMatrixHeight = 32;      // Set to the height of your display
const uint8_t kRefreshDepth = 48;       // Tradeoff of color quality vs refresh rate, max brightness, and RAM usage.  36 is typically good, drop down to 24 if you need to.  On Teensy, multiples of 3, up to 48: 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48.  On ESP32: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save RAM, more to keep from dropping frames and automatically lowering refresh rate.  (This isn't used on ESP32, leave as default)
const uint8_t kPanelType = SM_PANELTYPE_HUB75_32ROW_MOD16SCAN;   // Choose the configuration that matches your panels.  See more details in MatrixCommonHub75.h and the docs: https://github.com/pixelmatix/SmartMatrix/wiki
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_NONE);        // see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);

#define NOISE 65            // 0-1023, output noisefloor when not playing audio
#define NOISE_TOLERANCE 2 // 1-4, tolerance for auto shut off function
#define SMOOTHING 78        // 100 = no smoothing, 0 = full smoothing
#define TARGET_FPS 30       // 0-60, Frames per second - higher can be more jittery

#define BRIGHTNESS_PIN 16  // Brightness potentiometer
#define MODE_PIN 17        // Mode pushbutton
#define STROBE_PIN 22      // MSGEQ7 combined strobe
#define RESET_PIN 23       // MSGEQ7 combined reset
#define OUT_PIN_0 14       // MSGEQ7_0 output (this is A0/14 on Teensy 4.0),
#define OUT_PIN_1 15       // MSGEQ7_1 output (this is A1/15 on Teensy 4.0)

unsigned long long CLK_FREQ_0 = 104570; // MSGEQ7 clock freq. 0
unsigned long long CLK_FREQ_1 = 166280; // MSGEQ7 clock freq. 1 

int levelBin0[7];          // Holds 7 frequency bands from MSGEQ7_0
int levelBin1[7];          // Holds 7 frequency bands from MSGEQ7_1
int levelBinPeaks[14] = {0};  // Holds maximum peak values for all bins
int binWidth;              // Will calculate bin width based on display size
int binPadding;            // Will calculate padding to center graphics
int quietTime;
bool isQuiet = false;
int currentFPS = 0;
int currentBrightness = 0;
int startBrightness = 0;

Si5351mcu si;
ExponentialFilter<int> * BinFilters[14];

bool buttonPressed = false;      // Button momentary state
int mode = 0;                    // Current display mode
String modeString = "";          // Current display mode, string
const int modeAddress = 0;       // EEPROM memory address for mode, set to default 0
const int numModes = 3;          // Number of modes to choose from
const String modeList[numModes] = {
  "Rainbow",
  "Heat",
  "White"
};

void setup() {
  Serial.begin(115200);
  
  if (EEPROM.read(modeAddress) == 0xFF) { // if EEPROM is blank
    EEPROM.write(modeAddress, mode);
  } else {
    mode = EEPROM.read(modeAddress);
  }
  
  modeString = modeList[mode];
 
  // Initialize Matrix
  matrix.addLayer(&backgroundLayer); 
  matrix.begin();
  backgroundLayer.enableColorCorrection(true);

  backgroundLayer.fillScreen({0,0,0});
  backgroundLayer.swapBuffers();
  backgroundLayer.setFont(font6x10);
  backgroundLayer.drawString(14, 1, {212,66,245}, "Teensy");
  backgroundLayer.drawString(2, 11, {212,66,245}, "Visualizer");
  backgroundLayer.setFont(font3x5);
  backgroundLayer.drawString(14, 24, {255,255,255}, "14 Channel");
  backgroundLayer.swapBuffers();

  binWidth = (kMatrixWidth / 14);
  binPadding = (kMatrixWidth - (binWidth * 14)) / 2;
  
  for (int i = 0; i < 14; i++) {
    BinFilters[i] =  new ExponentialFilter<int>(SMOOTHING, 0);
    }

  pinMode(MODE_PIN, INPUT);
  pinMode(BRIGHTNESS_PIN, INPUT);
  pinMode(STROBE_PIN, OUTPUT);
  pinMode(RESET_PIN, OUTPUT);
  pinMode(OUT_PIN_0, INPUT);
  pinMode(OUT_PIN_1, INPUT);

  si.init(25000000L);
  si.setFreq(0, CLK_FREQ_0);
  si.setFreq(1, CLK_FREQ_1);
  si.setPower(0, SIOUT_8mA);
  si.setPower(1, SIOUT_8mA);
  si.enable(0);
  si.enable(1);

  // Create an initial state for MSGEQ7 pins
  digitalWrite(RESET_PIN,  LOW);
  digitalWrite(STROBE_PIN, LOW);

  delay(500);
  
  for (int i = 0; i < kMatrixWidth; i++) {
    backgroundLayer.fillRectangle(i, kMatrixHeight - 1, i, kMatrixHeight - 2, {255,255,255});
    backgroundLayer.swapBuffers();
    delay(30);
  }
}

void loop() {
  int startTime = millis();
  getBins();
  autoShutOff(startTime);

  if (digitalRead(MODE_PIN) == HIGH && buttonPressed == false) {
    buttonPressed = true;
    if (mode == numModes - 1) {
      mode = 0;
    } else {
      mode += 1;
    }
    
    modeString = modeList[mode];
    EEPROM.write(modeAddress, mode);
    
  } else if (digitalRead(MODE_PIN) == LOW && buttonPressed == true) {
    buttonPressed = false;
  }

  switch (mode) {
    case 0:
      rainbow();
      break;
      
    case 1:
      break;
      
    case 2:
      break;
    
  }
  
    int x = binPadding;
//    double hue = 0;
    for (int i = 0; i < 14; i++) {
    
      int val = ((max((BinFilters[i]->Current() - NOISE), 0) / 1023.0) * kMatrixHeight); //float calculations
//      int val_scaled = 255.0 * val / kMatrixHeight;

      SM_RGB color = CRGB(CHSV(i * 18.5, 255, 255)); // color hue based on band
      if (modeString == "Heat") color = CRGB(CHSV(100 - val*3.2, 255, 255)); // color hue based on peak height
      if (modeString == "White") color = CRGB(255,255,255); // color hue based on peak height

      if (i != 13) {
        int valNext = (((BinFilters[i+1]->Current() - NOISE) / 1023.0) * kMatrixHeight); //float calculations
        backgroundLayer.drawLine(x, kMatrixHeight - val - 1, x+4, kMatrixHeight - valNext - 1, color); //to do falling white stars/rain under curve
//        backgroundLayer.drawLine(x, kMatrixHeight - 1, x, kMatrixHeight - val, CRGB(CHSV(255,150,200))); //under line
      }
    
      for (int i = 0; i < binWidth; i++) {
  //       backgroundLayer.fillRectangle(x, kMatrixHeight - 1, x, kMatrixHeight - 1 - val, color);
  //       hue += 279/56;
        x++;
      
    }
  }
  
  backgroundLayer.swapBuffers(false);

  int loopFPS = 1.0/((millis() - startTime)/1000.0);
  if (loopFPS > TARGET_FPS) {
    delay((1.0/TARGET_FPS - 1.0/loopFPS)*1000.0);
  }
  
  currentFPS = 1.0/((millis() - startTime)/1000.0);
//  Serial.println("FPS: " + (String)currentFPS);
}

void autoShutOff(int startTime) {
  int avg = 0;
  int fadeTime = 2000;
  int waitTime = 3000;
  
  for (int i = 0; i < 13; i++) {
    avg += BinFilters[i]->Current();
  }

  avg /= 13;

  if (!isQuiet && avg < NOISE * NOISE_TOLERANCE) {
    quietTime = millis();
    isQuiet = true;
    startBrightness = analogRead(BRIGHTNESS_PIN) / 1023.0 * 255;
    
  } else if (avg > NOISE * NOISE_TOLERANCE) {
    isQuiet = false;
    
  }

  backgroundLayer.fillScreen({0,0,0});
  
  if (isQuiet && startTime - quietTime > waitTime) {
    if (currentBrightness > 0) currentBrightness -= startBrightness/currentFPS/fadeTime/1000;
    currentBrightness = max(currentBrightness, 0);
    Serial.println(currentBrightness);
    
  } else {
    currentBrightness = analogRead(BRIGHTNESS_PIN) / 1023.0 * 255;
    
  }

  matrix.setBrightness(currentBrightness); //limit brightness to positives only
}

void rainbow() {
  
}

void getBins() {
  
    // Create an initial state for our pins
  digitalWrite(STROBE_PIN,  HIGH);
  digitalWrite(RESET_PIN,  HIGH);
  digitalWrite(RESET_PIN, LOW);
  delayMicroseconds(75);

  // Cycle through each frequency band by pulsing the strobe.
  for (int i = 0; i < 7; i++) {
    digitalWrite(STROBE_PIN, LOW);
    delayMicroseconds(40);
    
    levelBin0[i] = analogRead(OUT_PIN_0);
    levelBin1[i] = analogRead(OUT_PIN_1);
    
    digitalWrite(STROBE_PIN, HIGH);
    delayMicroseconds(40);
  }
  
  int n = 0;
  for (int i = 0; i < 14; i++) {
    if (i % 2 == 0) {
      BinFilters[i]->Filter(levelBin0[n]);
    } else {
      BinFilters[i]->Filter(levelBin1[n]);
      n++;
    }
  }
  
}
