#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Encoder.h>
#include <EEPROM.h>

#define OLED_ADDR 0x3C

#define EEPROM_MAGIC 42

#define ENC_A 4
#define ENC_B 5
#define ENC_SW 6
#define APP_SCROLL_STEP 8
#define VOL_SCROLL_STEP 4

#define IDLE_TIMEOUT 30000UL

Adafruit_SH1106G display(128, 64, &Wire, -1);
Encoder encoder(ENC_A, ENC_B);

enum ScreenMode {
  APP_SELECT,
  VOLUME_EDIT
};

ScreenMode currentMode = APP_SELECT;

const char* apps[] = {
  "Spotify",
  "Brave",
  "Games",
  "Unmapped",
  "Discord",
  "Steam",
  "VLC",
  "Master"
};
const int DUST_COUNT = 24;

int dustX[DUST_COUNT];
int dustY[DUST_COUNT];
const int APP_COUNT =
  sizeof(apps) / sizeof(apps[0]);

int appVolume[APP_COUNT] = { 0 };

bool appMuted[APP_COUNT];
int previousVolume[APP_COUNT];

int selectedApp = 0;

bool lastButtonState = HIGH;

long lastEncoderPos = 0;

bool screenDirty = true;

unsigned long lastActivity = 0;
bool idleMode = false;
int peakLevel[APP_COUNT] = { 0 };


unsigned long buttonPressStart = 0;
bool longPressHandled = false;

const unsigned long LONG_PRESS_TIME = 1000;
void centerText(const char* text, int y, int size);

void drawScreen();
void drawAppScreen();
void drawVolumeScreen();
void drawIdleScreen();

void splashScreen();
void handleEncoder();
void handleButton();
void checkIdle();
void wakeDisplay();

void drawMusicIcon();
void drawChatIcon();
void drawShieldIcon();
void drawGamepadIcon();
void drawStarIcon();
void drawConeIcon();
void drawQuestionIcon();
void drawMuteIcon();
void drawAppIcon(int app);

void loadSettings();
void saveSettings();



// =======================================
// SETUP
// =======================================

void setup() {

  pinMode(ENC_SW, INPUT_PULLUP);

  Serial.begin(9600);

  display.begin(OLED_ADDR, true);

  loadSettings();

  for (int i = 0; i < DUST_COUNT; i++) {
    dustX[i] = random(128);
    dustY[i] = random(64);
  }


  splashScreen();

  encoder.write(0);
  lastEncoderPos = 0;

  lastActivity = millis();

  screenDirty = true;
}
// =======================================
//DEEJ
// =======================================
void sendDeejValues() {
  String builtString = "";

  for (int i = 0; i < APP_COUNT; i++) {
    int deejValue;

    if (appMuted[i]) {
      deejValue = 0;
    } else {
      deejValue =
        map(
          appVolume[i],
          0,
          100,
          0,
          1023);
    }

    builtString += String(deejValue);

    if (i < APP_COUNT - 1) {
      builtString += "|";
    }
  }

  Serial.println(builtString);
}

// =======================================
// MAIN LOOP
// =======================================
void loop() {

  checkIdle();

  handleEncoder();
  handleButton();

  static unsigned long lastSend = 0;

  if (millis() - lastSend > 50) {
    sendDeejValues();
    lastSend = millis();
  }

  if (idleMode) {
    screenDirty = true;
  }

  if (screenDirty) {
    drawScreen();
    screenDirty = false;
  }
}

// =======================================
//Save
// =======================================
void saveSettings() {
  int addr = 0;

  EEPROM.update(addr++, EEPROM_MAGIC);

  for (int i = 0; i < APP_COUNT; i++) {
    EEPROM.update(addr++, appVolume[i]);
  }

  for (int i = 0; i < APP_COUNT; i++) {
    EEPROM.update(addr++, appMuted[i]);
  }

  EEPROM.update(addr++, selectedApp);
}


// =======================================
// Load
// =======================================

void loadSettings() {
  int addr = 0;

  if (EEPROM.read(addr++) != EEPROM_MAGIC) {
    for (int i = 0; i < APP_COUNT; i++) {
      appVolume[i] = 75;
      previousVolume[i] = 75;
      appMuted[i] = false;
    }

    selectedApp = 0;

    saveSettings();
    return;
  }

  for (int i = 0; i < APP_COUNT; i++) {
    appVolume[i] = EEPROM.read(addr++);

    if (appVolume[i] > 100) {
      appVolume[i] = 75;
    }
  }

  for (int i = 0; i < APP_COUNT; i++) {
    appMuted[i] = (EEPROM.read(addr++) == 1);
  }

  for (int i = 0; i < APP_COUNT; i++) {
    previousVolume[i] = appVolume[i];
  }

  selectedApp = EEPROM.read(addr++);

  if (selectedApp >= APP_COUNT) {
    selectedApp = 0;
  }
}
// =======================================
//StartUp
// =======================================
void splashScreen() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  // ============================
  // Audio Level Animation
  // ============================

  for (int level = 0; level < 5; level++) {
    display.clearDisplay();

    centerText("DEEJ", 0, 1);

    int x = 24;

    for (int i = 0; i < 5; i++) {
      int h = 8 + (i * 6);

      if (i <= level) {
        display.fillRect(
          x + (i * 16),
          54 - h,
          10,
          h,
          SH110X_WHITE);
      } else {
        display.drawRect(
          x + (i * 16),
          54 - h,
          10,
          h,
          SH110X_WHITE);
      }
    }

    display.display();
    delay(250);
  }

  // ============================
  // Peak Effect
  // ============================

  for (int flash = 0; flash < 2; flash++) {
    display.clearDisplay();

    centerText("DEEJ", 0, 1);

    for (int i = 0; i < 5; i++) {
      int h = 8 + (i * 6);

      display.fillRect(
        24 + (i * 16),
        54 - h,
        10,
        h,
        SH110X_WHITE);
    }

    display.display();
    delay(70);

    display.clearDisplay();
    display.display();
    delay(70);
  }

  // ============================
  // Rotary Encoder Animation
  // ============================

  const char* frames[] = {
    "O",
    "/",
    "-",
    "\\",
    "O"
  };

  for (int i = 0; i < 5; i++) {
    display.clearDisplay();

    display.setTextSize(3);

    int16_t x1, y1;
    uint16_t w, h;

    display.getTextBounds(
      frames[i],
      0,
      0,
      &x1,
      &y1,
      &w,
      &h);

    display.setCursor(
      (128 - w) / 2,
      8);

    display.print(frames[i]);

    centerText("DEEJ", 44, 1);

    display.display();

    delay(180);
  }

  // ============================
  // Final Startup Screen
  // ============================

  display.clearDisplay();

  centerText("DEEJ", 10, 2);

  centerText(
    "Audio Controller",
    38,
    1);

  centerText(
    "V2.1",
    52,
    1);

  display.display();

  delay(1800);
}

// =======================================

void wakeDisplay() {
  idleMode = false;
  lastActivity = millis();
  screenDirty = true;
}


// =======================================

void checkIdle() {
  if (!idleMode) {
    if (millis() - lastActivity > IDLE_TIMEOUT) {
      currentMode = APP_SELECT;
      idleMode = true;
      screenDirty = true;
    }
  }
}

// =======================================
// INPUT
// =======================================
void handleEncoder() {

  long pos = encoder.read();

  if (idleMode) {
    if (pos != lastEncoderPos) {
      wakeDisplay();
      lastEncoderPos = pos;
    }

    return;
  }

  long delta = pos - lastEncoderPos;

  // CLOCKWISE
  if (currentMode == APP_SELECT) {
    if (delta >= APP_SCROLL_STEP) {
      selectedApp++;

      if (selectedApp >= APP_COUNT)
        selectedApp = 0;

      lastEncoderPos = pos;
      lastActivity = millis();
      screenDirty = true;
    }
  } else {
    if (delta >= VOL_SCROLL_STEP) {
      if (appMuted[selectedApp]) {
        appMuted[selectedApp] = false;
        appVolume[selectedApp] = 0;
      }

      appVolume[selectedApp] += 4;

      if (appVolume[selectedApp] > 100) {
        appVolume[selectedApp] = 100;
      }

      saveSettings();

      lastEncoderPos = pos;
      lastActivity = millis();
      screenDirty = true;
    }
  }

  // COUNTER-CLOCKWISE
  if (currentMode == APP_SELECT) {
    if (delta <= -APP_SCROLL_STEP) {
      selectedApp--;

      if (selectedApp < 0) {
        selectedApp = APP_COUNT - 1;
      }

      lastEncoderPos = pos;
      lastActivity = millis();
      screenDirty = true;
    }
  } else {
    if (delta <= -VOL_SCROLL_STEP) {
      if (!appMuted[selectedApp]) {
        appVolume[selectedApp] -= 4;

        if (appVolume[selectedApp] <= 0) {
          appVolume[selectedApp] = 0;
          appMuted[selectedApp] = true;
        }

        saveSettings();
      }

      lastEncoderPos = pos;
      lastActivity = millis();
      screenDirty = true;
    }
  }
}

// =======================================
void handleButton() {
  bool currentButton = digitalRead(ENC_SW);

  // Wake from idle
  if (idleMode) {
    if (lastButtonState == HIGH && currentButton == LOW) {
      wakeDisplay();

      // Prevent wake press from being interpreted
      // as a mute/menu press.
      buttonPressStart = millis();
      lastButtonState = currentButton;
      return;
    }

    lastButtonState = currentButton;
    return;
  }

  // Button pressed
  if (lastButtonState == HIGH && currentButton == LOW) {
    buttonPressStart = millis();
    longPressHandled = false;
  }

  // Long press
  if (currentButton == LOW && !longPressHandled) {
    if (millis() - buttonPressStart >= LONG_PRESS_TIME) {
      if (!appMuted[selectedApp]) {
        previousVolume[selectedApp] =
          appVolume[selectedApp];

        appMuted[selectedApp] = true;
      } else {
        appMuted[selectedApp] = false;

        appVolume[selectedApp] =
          previousVolume[selectedApp];
      }

      saveSettings();

      longPressHandled = true;

      lastActivity = millis();
      screenDirty = true;
    }
  }

  // Short press
  if (lastButtonState == LOW && currentButton == HIGH) {
    unsigned long pressTime =
      millis() - buttonPressStart;

    if (pressTime < LONG_PRESS_TIME) {
      if (currentMode == APP_SELECT) {
        currentMode = VOLUME_EDIT;
      } else {
        currentMode = APP_SELECT;
      }

      lastEncoderPos = encoder.read();

      lastActivity = millis();
      screenDirty = true;
    }

    longPressHandled = false;
  }

  lastButtonState = currentButton;
}

// =======================================
// DISPLAY
// =======================================

void drawScreen() {
  display.clearDisplay();

  if (idleMode) {
    drawIdleScreen();
  } else if (currentMode == APP_SELECT) {
    drawAppScreen();
  } else {
    drawVolumeScreen();
  }

  display.display();
}

void drawIdleScreen() {
  static unsigned long lastPeakDrop = 0;

  if (millis() - lastPeakDrop > 100) {
    for (int i = 0; i < APP_COUNT; i++) {
      if (peakLevel[i] > 0) {
        peakLevel[i]--;
      }
    }

    lastPeakDrop = millis();
  }

  centerText(
    apps[selectedApp],
    4,
    1);

  // Draw volume bars
  for (int i = 0; i < APP_COUNT; i++) {
    int barHeight =
      map(
        appVolume[i],
        0,
        100,
        0,
        30);

    int x = 4 + (i * 15);

    display.drawRect(
      x,
      20,
      10,
      30,
      SH110X_WHITE);

    if (!appMuted[i]) {
      display.fillRect(
        x + 1,
        50 - barHeight,
        8,
        barHeight,
        SH110X_WHITE);

      if (barHeight > peakLevel[i]) {
        peakLevel[i] = barHeight;
      }
    }

    if (peakLevel[i] > 0) {
      display.drawLine(
        x + 1,
        50 - peakLevel[i],
        x + 8,
        50 - peakLevel[i],
        SH110X_WHITE);
    }

    // Selected app blink
    if (i == selectedApp) {
      if ((millis() / 300) % 2) {
        display.drawRect(
          x - 1,
          19,
          12,
          32,
          SH110X_WHITE);
      }
    }
  }

  // Dust particles
  for (int i = 0; i < DUST_COUNT; i++) {
    display.drawPixel(
      dustX[i],
      dustY[i],
      SH110X_WHITE);

    dustY[i]++;

    if (random(10) > 7) {
      dustY[i]++;
    }

    if (dustY[i] > 63) {
      dustY[i] = 0;
      dustX[i] = random(128);
    }
  }
}

//centerText(
//  "Rotate To Wake",
//  54,
//  1);


// =======================================
//Volume Screen
// =======================================
void drawVolumeScreen() {
  centerText(
    apps[selectedApp],
    0,
    1);

  display.drawLine(
    0,
    12,
    127,
    12,
    SH110X_WHITE);

  char text[16];

  if (appMuted[selectedApp]) {
    strcpy(text, "MUTED");
  } else {
    sprintf(
      text,
      "%d%%",
      appVolume[selectedApp]);
  }

  centerText(
    text,
    18,
    2);

  display.drawRect(
    10,
    44,
    108,
    12,
    SH110X_WHITE);

  if (!appMuted[selectedApp]) {
    int width =
      map(
        appVolume[selectedApp],
        0,
        100,
        0,
        106);

    display.fillRect(
      11,
      45,
      width,
      10,
      SH110X_WHITE);
  }

  display.setTextSize(1);
  display.setCursor(30, 56);
  display.print("Press=Apps");
}

// =======================================
void drawMusicIcon() {
  display.drawLine(
    60, 4,
    60, 18,
    SH110X_WHITE);

  display.drawLine(
    60, 4,
    70, 6,
    SH110X_WHITE);

  display.fillCircle(
    58, 20,
    3,
    SH110X_WHITE);

  display.fillCircle(
    68, 22,
    3,
    SH110X_WHITE);
}

// =======================================
//Icons
// =======================================
void drawChatIcon() {
  display.drawRoundRect(54, 6, 20, 12, 3, SH110X_WHITE);
  display.drawLine(60, 18, 58, 22, SH110X_WHITE);
}

void drawShieldIcon() {
  display.drawLine(64, 4, 72, 8, SH110X_WHITE);
  display.drawLine(72, 8, 70, 18, SH110X_WHITE);
  display.drawLine(70, 18, 64, 22, SH110X_WHITE);
  display.drawLine(64, 22, 58, 18, SH110X_WHITE);
  display.drawLine(58, 18, 56, 8, SH110X_WHITE);
  display.drawLine(56, 8, 64, 4, SH110X_WHITE);
}

void drawGamepadIcon() {
  display.drawRoundRect(54, 8, 20, 10, 4, SH110X_WHITE);

  display.drawLine(58, 13, 62, 13, SH110X_WHITE);
  display.drawLine(60, 11, 60, 15, SH110X_WHITE);

  display.drawPixel(68, 12, SH110X_WHITE);
  display.drawPixel(71, 14, SH110X_WHITE);
}

void drawStarIcon() {
  display.drawLine(64, 4, 66, 10, SH110X_WHITE);
  display.drawLine(66, 10, 72, 10, SH110X_WHITE);
  display.drawLine(72, 10, 67, 14, SH110X_WHITE);
  display.drawLine(67, 14, 69, 20, SH110X_WHITE);
  display.drawLine(69, 20, 64, 16, SH110X_WHITE);
  display.drawLine(64, 16, 59, 20, SH110X_WHITE);
  display.drawLine(59, 20, 61, 14, SH110X_WHITE);
  display.drawLine(61, 14, 56, 10, SH110X_WHITE);
  display.drawLine(56, 10, 62, 10, SH110X_WHITE);
  display.drawLine(62, 10, 64, 4, SH110X_WHITE);
}

void drawConeIcon() {
  display.drawTriangle(
    64, 4,
    56, 20,
    72, 20,
    SH110X_WHITE);

  display.drawLine(59, 10, 69, 10, SH110X_WHITE);
  display.drawLine(58, 15, 70, 15, SH110X_WHITE);
}

void drawQuestionIcon() {
  display.drawCircle(64, 10, 6, SH110X_WHITE);
  display.drawPixel(64, 20, SH110X_WHITE);
}

void drawSpeakerIcon() {
  display.fillTriangle(
    58, 12,
    64, 8,
    64, 16,
    SH110X_WHITE);

  display.drawCircle(68, 12, 3, SH110X_WHITE);
  display.drawCircle(68, 12, 6, SH110X_WHITE);
}

// =======================================
void drawMuteIcon() {
  display.fillTriangle(
    58, 12,
    64, 8,
    64, 16,
    SH110X_WHITE);

  display.drawLine(
    68, 6,
    76, 18,
    SH110X_WHITE);

  display.drawLine(
    76, 6,
    68, 18,
    SH110X_WHITE);
}

void drawAppIcon(int app) {
  switch (app) {
    case 0:  // Spotify
      drawMusicIcon();
      break;

    case 1:  // Brave
      drawShieldIcon();
      break;

    case 2:  // Games
      drawStarIcon();
      break;

    case 3:  // Unmapped
      drawQuestionIcon();
      break;

    case 4:  // Discord
      drawChatIcon();
      break;

    case 5:  // Steam
      drawGamepadIcon();
      break;

    case 6:  // VLC
      drawConeIcon();
      break;

    case 7:  // Master
      drawSpeakerIcon();
      break;
  }
}

// =======================================

void drawAppScreen() {
  if (appMuted[selectedApp]) {
    drawMuteIcon();
  } else {
    drawAppIcon(selectedApp);
  }

  centerText(
    apps[selectedApp],
    24,
    2);

  char status[16];

  if (appMuted[selectedApp]) {
    strcpy(status, "MUTED");
  } else {
    sprintf(
      status,
      "Vol %d%%",
      appVolume[selectedApp]);
  }

  centerText(
    status,
    46,
    1);

  char counter[12];

  sprintf(
    counter,
    "%d/%d",
    selectedApp + 1,
    APP_COUNT);

  display.setTextSize(1);

  display.setCursor(10, 56);
  display.print("Press=Volume");

  display.setCursor(95, 56);
  display.print(counter);
}


// =======================================


void centerText(
  const char* text,
  int y,
  int size) {
  display.setTextSize(size);

  int16_t x1, y1;
  uint16_t w, h;

  display.getTextBounds(
    text,
    0,
    0,
    &x1,
    &y1,
    &w,
    &h);

  display.setCursor(
    (128 - w) / 2,
    y);

  display.print(text);
}