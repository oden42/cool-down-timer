#include <ss_oled.h>
#include <EEPROM.h>
#include <Wire.h>

// OLED Display settings
#define SDA_PIN 2
#define SCL_PIN 3
#define OLED_ADDR 0x3C
#define RESET_PIN -1
#define OLED_TYPE OLED_128x64

// Button pins
#define BUTTON_A 4
#define BUTTON_B 5

// EEPROM address for storing count
#define COUNT_ADDR 0

// Button timing
#define LONG_PRESS_TIME 2000  // Adjusted to 2 seconds for more accurate long press
#define DISPLAY_UPDATE_INTERVAL 500  // Update display every 500ms instead of 250ms

// Global variables
SSOLED oled;
unsigned long startTime = 0;
unsigned long pausedTime = 0;
int timerDuration = 5000; // Initial 5 seconds in milliseconds
bool isRunning = false;
bool isPaused = false;
bool buttonAPressed = false;
bool buttonBPressed = false;
int restartCount = 0;
unsigned long buttonAPressTime = 0;
unsigned long lastUpdateTime = 0;
char lastLine1[20] = "";
char lastLine2[20] = "";

// Time correction factor
const float TIME_CORRECTION = 0.5; // Multiply durations by 2 to compensate for faster clock

void saveCount() {
  EEPROM.put(COUNT_ADDR, restartCount);
}

void loadCount() {
  EEPROM.get(COUNT_ADDR, restartCount);
  if (restartCount < 0 || restartCount > 9999) {
    restartCount = 0;
    saveCount();
  }
}

void updateDisplay(const char* line1, const char* line2, bool forceUpdate = false) {
  // Only update if text has changed or force update is requested
  if (forceUpdate || strcmp(line1, lastLine1) != 0 || strcmp(line2, lastLine2) != 0) {
    // Clear lines individually
    oledWriteString(&oled, 0, 0, 0, "                ", FONT_NORMAL, 0, 1);
    oledWriteString(&oled, 0, 0, 2, "                ", FONT_NORMAL, 0, 1);
    // Write new text
    oledWriteString(&oled, 0, 0, 0, line1, FONT_NORMAL, 0, 1);
    oledWriteString(&oled, 0, 0, 2, line2, FONT_NORMAL, 0, 1);
    // Update last known text
    strcpy(lastLine1, line1);
    strcpy(lastLine2, line2);
  }
}

void initDisplay() {
  Wire.begin();
  delay(200);  // Increased delay for more stable initialization
  
  // Initialize OLED
  int rc = oledInit(&oled, OLED_TYPE, OLED_ADDR, 0, 0, 1, SDA_PIN, SCL_PIN, RESET_PIN, 400000L);
  if (rc != OLED_NOT_FOUND) {
    // Clear display and set contrast
    oledFill(&oled, 0, 1);
    delay(200);
    oledSetContrast(&oled, 127);
    delay(200);
    
    // Show splash screen
    oledWriteString(&oled, 0, 0, 0, "Hello!", FONT_NORMAL, 0, 1);
    delay(2000);
    
    // Show ready screen
    char countStr[20];
    sprintf(countStr, "Count: %d", restartCount);
    updateDisplay("Ready to go!", countStr, true);
  }
}

void setup() {
  // Initialize buttons
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  
  // Load saved count
  loadCount();
  
  // Initialize display
  initDisplay();
}

void loop() {
  static unsigned long lastSecondUpdate = 0;
  unsigned long currentMillis = millis();
  
  // Read button states
  bool currentButtonA = !digitalRead(BUTTON_A);
  bool currentButtonB = !digitalRead(BUTTON_B);
  
  // Button A handling with long press detection
  if (currentButtonA) {
    if (!buttonAPressed) {
      buttonAPressTime = currentMillis;
      buttonAPressed = true;
    } else if ((currentMillis - buttonAPressTime) >= LONG_PRESS_TIME) {
      // Reset counter
      restartCount = 0;
      saveCount();
      timerDuration = 5000;
      isRunning = false;
      isPaused = false;
      
      char countStr[20];
      sprintf(countStr, "Count: %d", restartCount);
      updateDisplay("Counter Reset!", countStr, true);
      delay(1000);
      updateDisplay("Ready to go!", countStr, true);
      
      while (!digitalRead(BUTTON_A)) {
        delay(10);
      }
      buttonAPressed = false;
    }
  } else if (buttonAPressed) {
    if ((currentMillis - buttonAPressTime) < LONG_PRESS_TIME) {
      if (!isRunning && !isPaused) {
        startTime = currentMillis;
        isRunning = true;
        timerDuration = (int)(5000 * TIME_CORRECTION); // Adjust timer duration
        restartCount++;
        saveCount();
        lastSecondUpdate = currentMillis;
      } else if (isPaused) {
        startTime = currentMillis - (pausedTime / TIME_CORRECTION); // Adjust paused time
        isPaused = false;
        isRunning = true;
        lastSecondUpdate = currentMillis;
      }
    }
    buttonAPressed = false;
  }
  
  // Button B (Pause) handling
  if (currentButtonB && !buttonBPressed) {
    if (isRunning && !isPaused) {
      pausedTime = (currentMillis - startTime) * TIME_CORRECTION; // Adjust paused time
      isPaused = true;
      isRunning = false;
    }
  }
  buttonBPressed = currentButtonB;
  
  // Update display if needed
  if (currentMillis - lastUpdateTime >= DISPLAY_UPDATE_INTERVAL) {
    char timeStr[20];
    char countStr[20];
    sprintf(countStr, "Count: %d", restartCount);
    
    if (isRunning) {
      unsigned long elapsed = (currentMillis - startTime) * TIME_CORRECTION;
      int remaining = ((timerDuration - elapsed + 999) / 1000); // Round up
      
      if (elapsed >= timerDuration) {
        isRunning = false;
        timerDuration = 5000;
        updateDisplay("Time's up!", countStr);
      } else {
        sprintf(timeStr, "Time: %d sec", remaining);
        updateDisplay(timeStr, countStr);
      }
    } else if (isPaused) {
      int remaining = ((timerDuration - pausedTime + 999) / 1000); // Round up
      sprintf(timeStr, "Paused: %d sec", remaining);
      updateDisplay(timeStr, countStr);
    }
    
    lastUpdateTime = currentMillis;
  }
  
  delay(20);  // Increased delay for better stability
}