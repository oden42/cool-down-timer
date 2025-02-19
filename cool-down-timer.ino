// Code mostly generated by Claude.ai! Thanks Claude!
// AI guide: Oden42
// Board: Sparkfun Pro Micro
// Pinout link: https://learn.sparkfun.com/tutorials/pro-micro--fio-v3-hookup-guide/hardware-overview-pro-micro
// Programmer: Arduino IDE 2.3.4

#include <ss_oled.h> // https://github.com/bitbank2/ss_oled
#include <EEPROM.h>
#include <Wire.h>

// OLED Display settings
#define SDA_PIN 2
#define SCL_PIN 3
#define FLIPPED 1
#define INVERTED 0
#define OLED_ADDR 0x3C
#define RESET_PIN -1
#define OLED_TYPE OLED_128x32
#define READY_SCREEN_TIMEOUT 30000  // 30 seconds in milliseconds

// Button pins
#define BUTTON_A 8
#define BUTTON_B 6
#define BUTTON_C 10

// LED pins
#define LED_GREEN 16
#define LED_RED 15

// EEPROM address for storing count
#define COUNT_ADDR 0

// Button timing
#define BUTTON_A_LONG_PRESS_TIME 2000  // 2 seconds for Button A long press (end timer)
#define BUTTON_B_LONG_PRESS_TIME 2000  // 2 seconds for LED toggle
#define BUTTON_C_LONG_PRESS_TIME 2000  // 2 seconds for Button C long press (decrement counter)
#define DISPLAY_UPDATE_INTERVAL 500     // Update display every 500ms
#define BUTTON_DEBOUNCE_TIME 50        // 50ms debounce time

// Global variables
SSOLED oled;
unsigned long startTime = 0;
unsigned long pausedTime = 0;
unsigned long timerDuration = 5000; // Initial 60 seconds in milliseconds
unsigned long baseTimerDuration = 60000; // Initial 60 seconds in milliseconds
unsigned long timerDurationIncrement = 30000; // Timer increases by this amount each count
bool isRunning = false;
bool isPaused = false;
bool oledEnabled = true;
bool ledsEnabled = true;
bool buttonAPressed = false;
bool buttonBPressed = false;
bool buttonCPressed = false;
int restartCount = 0;
unsigned long buttonAPressTime = 0;
unsigned long buttonBPressTime = 0;
unsigned long buttonCPressTime = 0;
unsigned long lastUpdateTime = 0;
unsigned long lastActivityTime = 0;  // Track when the last activity occurred
char lastLine1[20] = "";
char lastLine2[20] = "";

// Time correction factor
const float TIME_CORRECTION = 1; // Multiply durations by 2 to compensate for faster clock

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
  if (!oledEnabled) {
    return;  // Skip all updates if display is "off"
  }
  
  // Check if we need a full line clear based on text length changes
  bool needLine1Clear = strlen(line1) < strlen(lastLine1);
  bool needLine2Clear = strlen(line2) < strlen(lastLine2);
  
  // Special cases that need full line clear
  if (strstr(lastLine1, "Paused") != NULL && strstr(line1, "Paused") == NULL) {
    needLine1Clear = true;
  }
  
  // Only update if text has changed or force update is requested
  if (forceUpdate || strcmp(line1, lastLine1) != 0 || strcmp(line2, lastLine2) != 0) {
    // Clear and update lines as needed
    if (forceUpdate || strcmp(line1, lastLine1) != 0) {
      if (needLine1Clear) {
        oledWriteString(&oled, 0, 0, 0, "                ", FONT_STRETCHED, 0, 1);
      }
      oledWriteString(&oled, 0, 0, 0, line1, FONT_STRETCHED, 0, 1);
      strcpy(lastLine1, line1);
    }
    
    if (forceUpdate || strcmp(line2, lastLine2) != 0) {
      if (needLine2Clear) {
        oledWriteString(&oled, 0, 0, 3, "                ", FONT_SMALL, 0, 1);
      }
      oledWriteString(&oled, 0, 0, 3, line2, FONT_SMALL, 0, 1);
      strcpy(lastLine2, line2);
    }
  }
}

void initDisplay() {
  Wire.begin();
  delay(200);  // Increased delay for more stable initialization
  
  // Initialize OLED
  int rc = oledInit(&oled, OLED_TYPE, OLED_ADDR, FLIPPED, INVERTED, 1, SDA_PIN, SCL_PIN, RESET_PIN, 400000L);
  // Clear display and set contrast
  oledFill(&oled, 0, 1);
  delay(200);
  oledSetContrast(&oled, 127);
  delay(200);
  
  // Show splash screen
  oledWriteString(&oled, 0, 0, 0, "Pitter", FONT_STRETCHED, 0, 1);
  oledWriteString(&oled, 0, 0, 2, "patter!", FONT_STRETCHED, 0, 1);
  delay(2000);
  
  // Clear entire display before showing ready screen
  oledFill(&oled, 0, 1);
  delay(50);  // Short delay to ensure clear completes
  
  // Show ready screen
  char countStr[20];
  sprintf(countStr, "Count: %d", restartCount);
  updateDisplay("Ready!", countStr, true);

  updateLastActivity();  // Initialize the activity timer
}

// Helper function to format time as HH:MM:SS
void formatTime(unsigned long totalSeconds, char* buffer) {
  unsigned long hours = totalSeconds / 3600;
  unsigned long minutes = (totalSeconds % 3600) / 60;
  unsigned long seconds = totalSeconds % 60;
  sprintf(buffer, "%02lu:%02lu:%02lu", hours, minutes, seconds);
}

void updateLastActivity() {
  lastActivityTime = millis();
  if (!oledEnabled) {  // Turn display back on if it was off
    oledEnabled = true;
    char countStr[20];
    sprintf(countStr, "Count: %d", restartCount);
    updateDisplay("Ready!", countStr, true);
  }
}

void setup() {
  // Initialize buttons
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  // Load saved count
  loadCount();
  
  // Initialize display
  initDisplay();
}

void loop() {
  static unsigned long lastSecondUpdate = 0;
  unsigned long currentMillis = millis();
  
  // Set LED states
  if (ledsEnabled) {
    if (isRunning) {
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_RED, HIGH);
    } else if (!isPaused) {
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_RED, LOW);
    }
  } else {
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, LOW);
  }

  // Auto-timeout check for ready screen
  if (oledEnabled && (currentMillis - lastActivityTime) >= READY_SCREEN_TIMEOUT) {
    oledEnabled = false;
    oledFill(&oled, 0, 1);
    lastLine1[0] = '\0';
    lastLine2[0] = '\0';
  }

  // Read button states
  bool currentButtonA = !digitalRead(BUTTON_A);
  bool currentButtonB = !digitalRead(BUTTON_B);
  bool currentButtonC = !digitalRead(BUTTON_C);
  
  // Button A handling (Start/Pause/End)
  if (currentButtonA) {
    if (!buttonAPressed) {
      updateLastActivity();
      buttonAPressTime = currentMillis;
      buttonAPressed = true;
    } else if ((currentMillis - buttonAPressTime) >= BUTTON_A_LONG_PRESS_TIME && (isRunning || isPaused)) {
      // Long press: End current timer
      isRunning = false;
      isPaused = false;
      timerDuration = baseTimerDuration;
      
      char countStr[20];
      sprintf(countStr, "Count: %d", restartCount);
      updateDisplay("Done!", countStr, true);
      delay(1000);
      updateDisplay("Ready!", countStr, true);
      
      while (!digitalRead(BUTTON_A)) {
        delay(10);
      }
    }
  } else if (buttonAPressed) {
    if ((currentMillis - buttonAPressTime) < BUTTON_A_LONG_PRESS_TIME) {
      // Short press: Toggle between Start and Pause
      if (!isRunning && !isPaused) {
        // Start new timer
        startTime = currentMillis;
        isRunning = true;
        timerDuration = (unsigned long)(baseTimerDuration + timerDurationIncrement * restartCount);
        restartCount++;
        saveCount();
        lastSecondUpdate = currentMillis;
      } else if (isRunning) {
        // Pause current timer
        pausedTime = (currentMillis - startTime) * TIME_CORRECTION;
        isPaused = true;
        isRunning = false;
      } else if (isPaused) {
        // Resume from pause
        startTime = currentMillis - (pausedTime / TIME_CORRECTION);
        isPaused = false;
        isRunning = true;
        lastSecondUpdate = currentMillis;
      }
    }
    buttonAPressed = false;
  }
  
  // Button B handling (Display Toggle / LED Toggle)
  if (currentButtonB) {
    if (!buttonBPressed) {
      buttonBPressTime = currentMillis;
      buttonBPressed = true;
    } else if ((currentMillis - buttonBPressTime) >= BUTTON_B_LONG_PRESS_TIME) {
      // Long press: Toggle LEDs
      ledsEnabled = !ledsEnabled;
      
      // Force LED states
      if (!ledsEnabled) {
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_RED, LOW);
      }
      
      while (!digitalRead(BUTTON_B)) {
        delay(10);
      }
      buttonBPressed = false;
    }
  } else if (buttonBPressed) {
    if ((currentMillis - buttonBPressTime) < BUTTON_B_LONG_PRESS_TIME) {
      // Short press: Toggle display
      buttonBPressed = false;
      oledEnabled = !oledEnabled;
      lastActivityTime = currentMillis;  // Reset timeout timer
      
      if (!oledEnabled) {
        oledFill(&oled, 0, 1);
        lastLine1[0] = '\0';
        lastLine2[0] = '\0';
      } else {
        char countStr[20];
        sprintf(countStr, "Count: %d", restartCount);
        if (isRunning) {
          char timeStr[20];
          char formattedTime[20];
          unsigned long elapsed = (millis() - startTime) * TIME_CORRECTION;
          unsigned long remainingSeconds = ((timerDuration - elapsed + 999) / 1000);
          formatTime(remainingSeconds, formattedTime);
          sprintf(timeStr, "%s", formattedTime);
          updateDisplay(timeStr, countStr, true);
        } else if (isPaused) {
          char timeStr[20];
          char formattedTime[20];
          unsigned long remainingSeconds = ((timerDuration - pausedTime + 999) / 1000);
          formatTime(remainingSeconds, formattedTime);
          sprintf(timeStr, "Paused: %s", formattedTime);
          updateDisplay(timeStr, countStr, true);
        } else {
          updateDisplay("Ready!", countStr, true);
        }
      }
    }
  }
  
  // Button C handling (Reset/Decrement Counter)
  if (currentButtonC) {
    if (!buttonCPressed) {
      updateLastActivity();
      buttonCPressTime = currentMillis;
      buttonCPressed = true;
    } else if ((currentMillis - buttonCPressTime) >= BUTTON_C_LONG_PRESS_TIME) {
      // Long press: Reset counter
      restartCount = 0;
      saveCount();
      timerDuration = baseTimerDuration;
      isRunning = false;
      isPaused = false;
      
      char countStr[20];
      sprintf(countStr, "Count: %d", restartCount);
      updateDisplay("RESET!", countStr, true);
      delay(1000);
      updateDisplay("Ready!", countStr, true);
      
      while (!digitalRead(BUTTON_C)) {
        delay(10);
      }
    }
  } else if (buttonCPressed) {
    if ((currentMillis - buttonCPressTime) < BUTTON_C_LONG_PRESS_TIME) {
      // Short press: Decrement counter (if > 0)
      if (restartCount > 0) {
        restartCount--;
        saveCount();
        char countStr[20];
        sprintf(countStr, "Count: %d", restartCount);
        updateDisplay("Count -1", countStr, true);
        delay(1000);
        updateDisplay("Ready!", countStr, true);
      }

    }
    buttonCPressed = false;
  }

  // Update display if needed
  if (currentMillis - lastUpdateTime >= DISPLAY_UPDATE_INTERVAL) {
    char timeStr[20];
    char countStr[20];
    char formattedTime[20];
    sprintf(countStr, "Count: %d", restartCount);
    
    if (isRunning) {
      unsigned long elapsed = (currentMillis - startTime) * TIME_CORRECTION;
      
      if (elapsed >= timerDuration) {
        isRunning = false;
        timerDuration = baseTimerDuration;
        updateDisplay("Ready!", countStr);
      } else {
        unsigned long remainingSeconds = ((timerDuration - elapsed + 999) / 1000);
        formatTime(remainingSeconds, formattedTime);
        sprintf(timeStr, "%s", formattedTime);
        updateDisplay(timeStr, countStr);
      }
    } else if (isPaused) {
      updateDisplay("Paused", countStr);
    }
    
    lastUpdateTime = currentMillis;
  }
  
  delay(20);
}
