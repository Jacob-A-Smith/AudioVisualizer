// Adafruit NeoPixel - Version: Latest
#include <Adafruit_NeoPixel.h>
#include "configuration.h"

/****************************************************************************************************/
/* VARIABLES                                                                                        */
/****************************************************************************************************/

// MSGEQ7 GRAPHIC EQUALIZER
int leftReadings[NUMBER_OF_BANDS], rightReadings[NUMBER_OF_BANDS];

// NEOPIXEL (WS2812b) / VISUALIZER
Adafruit_NeoPixel visualizer;
uint8_t visualizerMode, colorMode;
uint32_t* colorScheme;

/****************************************************************************************************/
/* MAIN - SETUP / LOOP                                                                              */
/****************************************************************************************************/
void setup() {
  // CONTROLS
  pinMode(VISUALIZER_POWER_SWITCH, INPUT);
  pinMode(LIGHTING_MODE_PB, INPUT);
  pinMode(COLOR_MODE_PB, INPUT);

  // MSGEQ7 GRAPHIC EQUALIZER
  pinMode(RESET_PIN, OUTPUT);
  pinMode(STROBE_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
  digitalWrite(STROBE_PIN, HIGH);

  // NEOPIXEL (WS2812b) / VISUALIZER
  visualizer = Adafruit_NeoPixel(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
  visualizer.setBrightness(DEFAULT_BRIGHTNESS);
  visualizer.begin();
  colorVisualizer(BLACK);
  visualizerMode = colorMode = 0;
  applyColorScheme(colorMode);
  
  // TEST
  if (digitalRead(LIGHTING_MODE_PB) || digitalRead(COLOR_MODE_PB)) { systemTest(); }
}

void loop() {
  if (visualizerPowerOn()) {
    readControlInputs();
    readMSGEQ7();
    writeVisualizer(visualizerMode);
    delay(REDRAW_DELAY);
  } else {
    colorVisualizer(BLACK);
    // sleep mode, conserve power
    while(!visualizerPowerOn()) { delay(REST_DELAY); }
  }
}

/****************************************************************************************************/
/* FUNCTIONS                                                                                        */
/****************************************************************************************************/

// CONTROLS

bool visualizerPowerOn() {
  return (digitalRead(VISUALIZER_POWER_SWITCH));
}

void readControlInputs() {
  // setup
  static bool lightingModePb = 0, lastLightingModePb = 0, colorModePb = 0, lastColorModePb = 0;
  static unsigned long lightingModePbDebounce = -1, colorModePbDebounce = -1;
  // lighting mode
  bool pbReading = digitalRead(LIGHTING_MODE_PB);
  if (pbReading != lastLightingModePb) {
    lightingModePbDebounce = millis();
  }
  if ((millis() - lightingModePbDebounce >= DEBOUNCE_DELAY) && (lightingModePbDebounce != -1)) {
    lightingModePbDebounce = -1;
    if (pbReading != lightingModePb) {
      lightingModePb = pbReading;
      if (lightingModePb) {
        if (++visualizerMode >= NUM_VISUALIZER_MODES) visualizerMode = 0;
      }
    }
  }
  lastLightingModePb = pbReading;
  // color mode
  pbReading = digitalRead(COLOR_MODE_PB);
  if (pbReading != lastColorModePb) {
    colorModePbDebounce = millis(); 
  }
  if ((millis() - colorModePbDebounce >= DEBOUNCE_DELAY) && (colorModePbDebounce != -1)) {
    colorModePbDebounce = -1;
    if (pbReading != colorModePb) {
      colorModePb = pbReading;
      if (colorModePb) {
        if (++colorMode >= NUM_COLOR_MODES) colorMode = 0;
        if (colorMode < NUM_COLOR_SCHEMES) applyColorScheme(colorMode);
      }
    }
  }
  lastColorModePb = pbReading;
}

// MSGEQ7 GRAPHIC EQUALIZER

void readMSGEQ7() {
  digitalWrite(RESET_PIN, HIGH);
  digitalWrite(RESET_PIN, LOW);
  for (uint8_t band = 0; band < NUMBER_OF_BANDS; ++band) {
    digitalWrite(STROBE_PIN, LOW);
    delayMicroseconds(MSGEQ7_READ_DELAY);
    leftReadings[band] = analogRead(LEFT_AUDIO_IN_PIN);
    rightReadings[band] = analogRead(RIGHT_AUDIO_IN_PIN);
    digitalWrite(STROBE_PIN, HIGH);
  }
}

// NEOPIXEL (WS2812b) / VISUALIZER

void writeVisualizer(uint8_t _visualizerMode) {
  switch(_visualizerMode) {
    case 1:
      doubleBarGraph(false);
      break;
    case 2:
      barGraph(true); // inverse bar graph
      break;
    case 3:
      doubleBarGraph(true); // inverse double
      break;
    case 4:
      outsideInGraph(false);
      break;
    case 5:
      outsideInGraph(true);
      break;
    case 6:
      static uint8_t cycleMode = 0;
      static unsigned long startTime = millis();
      static unsigned long deltaTime;
      deltaTime = millis() - startTime;
      if (deltaTime >= MODE_CYCLE_DELAY) {
        if (++cycleMode >= NUM_VISUALIZER_MODES - 1) cycleMode = 0;
        startTime = millis();
      }
      return writeVisualizer(cycleMode);
    default:
      barGraph(false);
      break;
  }
  visualizer.show();
}

void barGraph(bool inverse) {
  for (uint8_t i = 0; i < NUM_COLUMNS; ++i) {
    int band = ((leftReadings[i] + rightReadings[i]) / 2) - AUDIO_NOISE;
    for (uint8_t j = 0; j < NUM_ROWS; ++j) {
      uint8_t pixel = (i * NUM_ROWS) + j;
      uint32_t color = (band > 0 ? (inverse ? BLACK : calculateColor(pixel)) : (inverse ? calculateColor(pixel) : BLACK));
      band -= BAND_MAX_READING / NUM_ROWS;
      visualizer.setPixelColor(pixel, color);
    }
  }
}

void doubleBarGraph(bool inverse) {
  for (uint8_t i = 0; i < NUM_COLUMNS; ++i) {
    int lBand = leftReadings[i] - AUDIO_NOISE;
    int rBand = rightReadings[i] - AUDIO_NOISE;
    for (uint8_t j = 0; j < NUM_ROWS; ++j) {
      uint8_t pixel = (i * NUM_ROWS) + j;
      uint32_t color = (inverse ? calculateColor(pixel) : BLACK);
      if (j < NUM_ROWS / 2) {
        if (lBand > 0) color = (inverse ? BLACK : calculateColor(pixel));
        lBand -= BAND_MAX_READING / (NUM_ROWS * 2);
      } else {
        if (rBand > 0) color = (inverse ? BLACK : calculateColor(pixel));
        rBand -= BAND_MAX_READING / (NUM_ROWS * 2);
      }
      visualizer.setPixelColor(pixel, color);
    }
  }
}

void outsideInGraph(bool inverse) {
  for (uint8_t i = 0; i < NUM_COLUMNS; ++i) {
    int band = leftReadings[i] - AUDIO_NOISE;
    for (uint8_t j = 0; j < NUM_ROWS / 2; ++j) {
      uint8_t pixel = (i * NUM_ROWS) + j;
      uint32_t color = (inverse ? calculateColor(pixel) : BLACK);
      if (band > 0) color = (inverse ? BLACK : calculateColor(pixel));
      band -= BAND_MAX_READING / (NUM_ROWS * 2);
      visualizer.setPixelColor(pixel, color);
    }
    band = rightReadings[i] - AUDIO_NOISE;
    for (uint8_t j = NUM_ROWS - 1; j >= NUM_ROWS / 2; --j) {
      uint8_t pixel = (i * NUM_ROWS) + j;
      uint32_t color = (inverse ? calculateColor(pixel) : BLACK);
      if (band > 0) color = (inverse ? BLACK : calculateColor(pixel));
      band -= BAND_MAX_READING / (NUM_ROWS * 2);
      visualizer.setPixelColor(pixel, color);
    }
  }
}

uint32_t calculateColor(int pixel) {
  switch (colorMode) {
    case 4: // should start at 0?
      return spectrumShift();
    case 5:
      return rainbow();
    default:
      return colorScheme[(int)(pixel / (NUM_PIXELS / NUMBER_OF_BANDS))];
  }
}

uint32_t spectrumShift() {
  // TODO: update colors based off of a redraw delay modulus
  static unsigned long updateTimer = millis();
  static unsigned long updateDelay;
  updateDelay = millis() - updateTimer;
  static uint8_t _r = 0, _g = 0, _b = 0xFF;
  static uint8_t _state = 0;
  uint8_t step = 3;
  if (updateDelay > COLOR_CYCLE_DELAY) {
    switch (_state) {
      case 0:
        _g += step;
        if (_g >= 255) ++_state;
        break;
      case 1:
        _b -= step;
        if (_b <= 0) ++_state;
        break;
      case 2:
        _r += step;
        if (_r >= 255) ++_state;
        break;
      case 3:
        _g -= step;
        if (_g <= 0) ++_state;
        break;
      case 4:
        _b += step;
        if (_b >= 255) ++_state;
        break;
      case 5:
        _r -= step;
        if (_r <= 0) _state = 0;
        break;
      default:
        _r = 0, _g = 0, _b = 0xFF;
        _state = 0;
        break;
    }
    updateTimer = millis();
  }
  return (((uint32_t)_r << 16) | ((uint32_t)_g << 8) | (uint32_t)_b);
}

uint32_t rainbow() {
  static int _i = 0, _j = 0, _k = 0;
  static int rainbowScale = 192 / NUM_PIXELS;
  if (++_k > 10) {
    _k = 0;
    if (++_j > NUM_PIXELS) {
      _j = 0;
      if (++_i > NUM_PIXELS * 10) _i = 0;
    }
  }
  return (rainbowOrder((rainbowScale * (_j + _i)) % 191));
}

uint32_t rainbowOrder(byte position) {
  // 6 total zones of color change:
  if (position < 31) { // Red -> Yellow (Red = FF, blue = 0, green goes 00-FF)
    return visualizer.Color(0xFF, position * 8, 0);
  } else if (position < 63) {// Yellow -> Green (Green = FF, blue = 0, red goes FF->00)
    position -= 31;
    return visualizer.Color(0xFF - position * 8, 0xFF, 0);
  } else if (position < 95) { // Green->Aqua (Green = FF, red = 0, blue goes 00->FF)
    position -= 63;
    return visualizer.Color(0, 0xFF, position * 8);
  } else if (position < 127) { // Aqua->Blue (Blue = FF, red = 0, green goes FF->00)
    position -= 95;
    return visualizer.Color(0, 0xFF - position * 8, 0xFF);
  } else if (position < 159) { // Blue->Fuchsia (Blue = FF, green = 0, red goes 00->FF)
    position -= 127;
    return visualizer.Color(position * 8, 0, 0xFF);
  } else { //160 <position< 191   Fuchsia->Red (Red = FF, green = 0, blue goes FF->00)
    position -= 159;
    return visualizer.Color(0xFF, 0x00, 0xFF - position * 8);
  }
}

void colorVisualizer(uint32_t color) {
  for (uint8_t i = 0; i < NUM_PIXELS; ++i) {
    visualizer.setPixelColor(i, color);
  }
  visualizer.show();
}

// HELPERS
void applyColorScheme(uint8_t scheme) {
  if (scheme < NUM_COLOR_SCHEMES) {
    colorScheme = COLOR_SCHEMES[scheme];
  } else {
    colorScheme = COLOR_SCHEMES[0];
  }
}

// TEST 
void systemTest() {
  uint32_t __v[] = {RED, LIME, BLUE, WHITE, BLACK};
  for (uint8_t i = 0; i < 5; ++i) {
    colorVisualizer(__v[i]);
    delay(1000);
  }
  colorVisualizer(BLACK);
}




