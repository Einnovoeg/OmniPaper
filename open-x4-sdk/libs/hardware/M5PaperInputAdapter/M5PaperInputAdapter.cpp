#include "M5PaperInputAdapter.h"
#include <Arduino.h>

// Button area definitions for touch interaction on 960x540 display
const M5PaperInputAdapter::ButtonArea M5PaperInputAdapter::BUTTON_AREAS[NUM_BUTTON_AREAS] = {
  {40, 480, 80, 40, BTN_LEFT},    // Bottom left area
  {480, 480, 80, 40, BTN_CONFIRM}, // Bottom center area  
  {840, 480, 80, 40, BTN_RIGHT},  // Bottom right area
  {40, 240, 80, 40, BTN_UP},      // Middle left area
  {840, 240, 80, 40, BTN_DOWN},    // Middle right area
  {880, 40, 80, 40, BTN_BACK}     // Top right area
};

const char* M5PaperInputAdapter::BUTTON_NAMES[] = {
  "BACK", "CONFIRM", "LEFT", "RIGHT", "UP", "DOWN", "POWER"
};

M5PaperInputAdapter::M5PaperInputAdapter() 
  : currentState(0), lastState(0), pressedEvents(0), releasedEvents(0),
    lastDebounceTime(0), buttonPressStart(0), buttonPressFinish(0),
    touchActive(false), lastTouchActive(false), touchTapDetected(false),
    touchX(0), touchY(0), lastTouchX(0), lastTouchY(0), touchStartTime(0) {
}

void M5PaperInputAdapter::begin() {
  // Initialize M5Paper buttons (handled by M5.begin())
  // Touch controller will be initialized separately
  
  currentState = 0;
  lastState = 0;
  pressedEvents = 0;
  releasedEvents = 0;
}

void M5PaperInputAdapter::update() {
  // Refresh M5 button state
  M5.update();

  // Store last state
  lastState = currentState;
  lastTouchActive = touchActive;
  lastTouchX = touchX;
  lastTouchY = touchY;
  
  // Update button states
  updateButtonStates();
  
  // Update touch states
  updateTouchState();
  
  // Calculate events
  uint8_t stateChanges = currentState ^ lastState;
  pressedEvents = stateChanges & currentState;
  releasedEvents = stateChanges & lastState;
  
  // Handle button timing
  if (wasAnyPressed()) {
    buttonPressStart = millis();
  }
  if (wasAnyReleased()) {
    buttonPressFinish = millis();
  }
}

uint8_t M5PaperInputAdapter::getState() {
  return currentState;
}

bool M5PaperInputAdapter::isPressed(uint8_t buttonIndex) const {
  if (!isValidButtonIndex(buttonIndex)) return false;
  return (currentState >> buttonIndex) & 0x01;
}

bool M5PaperInputAdapter::wasPressed(uint8_t buttonIndex) const {
  if (!isValidButtonIndex(buttonIndex)) return false;
  return (pressedEvents >> buttonIndex) & 0x01;
}

bool M5PaperInputAdapter::wasAnyPressed() const {
  return pressedEvents != 0;
}

bool M5PaperInputAdapter::wasReleased(uint8_t buttonIndex) const {
  if (!isValidButtonIndex(buttonIndex)) return false;
  return (releasedEvents >> buttonIndex) & 0x01;
}

bool M5PaperInputAdapter::wasAnyReleased() const {
  return releasedEvents != 0;
}

bool M5PaperInputAdapter::isPowerButtonPressed() const {
  return isPressed(BTN_POWER);
}

unsigned long M5PaperInputAdapter::getHeldTime() const {
  if (isPressed(BTN_CONFIRM) || isPressed(BTN_LEFT) || isPressed(BTN_RIGHT) || isPressed(BTN_POWER)) {
    return millis() - buttonPressStart;
  }
  return 0;
}

bool M5PaperInputAdapter::isTouchPressed() const {
  return touchActive;
}

uint16_t M5PaperInputAdapter::getTouchX() const {
  return touchX;
}

uint16_t M5PaperInputAdapter::getTouchY() const {
  return touchY;
}

bool M5PaperInputAdapter::wasTapped() const {
  return touchTapDetected;
}

const char* M5PaperInputAdapter::getButtonName(uint8_t buttonIndex) const {
  if (buttonIndex < 7) {
    return BUTTON_NAMES[buttonIndex];
  }
  return "UNKNOWN";
}

// Private helper methods

uint8_t M5PaperInputAdapter::mapM5ButtonsToLogical() {
  uint8_t logicalState = 0;

  bool rightPressed = M5.BtnA.isPressed();
  bool centerPressed = M5.BtnB.isPressed();
  bool leftPressed = M5.BtnC.isPressed();

  // Use raw GPIO fallback where available for robustness.
#if defined(PLATFORM_M5PAPERS3)
  rightPressed = rightPressed || (digitalRead(GPIO_NUM_42) == LOW);
  centerPressed = centerPressed || (digitalRead(GPIO_NUM_41) == LOW);
#else
  // M5Paper pin map (GPIO37/38/39) corresponds to right/center/left.
  rightPressed = rightPressed || (digitalRead(GPIO_NUM_37) == LOW);
  centerPressed = centerPressed || (digitalRead(GPIO_NUM_38) == LOW);
  leftPressed = leftPressed || (digitalRead(GPIO_NUM_39) == LOW);
#endif

  if (leftPressed) {
    logicalState |= (1 << BTN_LEFT);
  }
  if (centerPressed) {
    logicalState |= (1 << BTN_CONFIRM);
  }
  if (rightPressed) {
    logicalState |= (1 << BTN_RIGHT);
  }
  
  // Map touch areas to logical buttons if touch is active
  if (touchActive) {
    uint8_t touchButton = getButtonFromTouch(touchX, touchY);
    if (touchButton < 7) {
      logicalState |= (1 << touchButton);
    }
  }
  
  return logicalState;
}

void M5PaperInputAdapter::updateTouchState() {
  touchTapDetected = false;
  
  const auto detail = M5.Touch.getDetail();

  if (detail.isPressed()) {
    touchActive = true;
    touchX = detail.x;
    touchY = detail.y;

    if (!lastTouchActive) {
      touchStartTime = millis();
    }
  } else {
    if (touchActive && lastTouchActive) {
      // Touch released
      touchActive = false;

      // Check if this was a tap (short duration)
      unsigned long touchDuration = millis() - touchStartTime;
      if (touchDuration < TAP_TIMEOUT) {
        touchTapDetected = true;
      }
    } else {
      touchActive = false;
    }
  }
}

void M5PaperInputAdapter::updateButtonStates() {
  // Update button states by mapping M5Paper buttons
  currentState = mapM5ButtonsToLogical();
}

bool M5PaperInputAdapter::isValidButtonIndex(uint8_t buttonIndex) const {
  return buttonIndex < 7; // BTN_POWER is index 6
}

bool M5PaperInputAdapter::isTouchInButtonArea(uint16_t x, uint16_t y, uint8_t buttonIndex) {
  for (uint8_t i = 0; i < NUM_BUTTON_AREAS; i++) {
    if (BUTTON_AREAS[i].logicalButton == buttonIndex) {
      const ButtonArea& area = BUTTON_AREAS[i];
      return (x >= area.x && x < (area.x + area.width) &&
              y >= area.y && y < (area.y + area.height));
    }
  }
  return false;
}

uint8_t M5PaperInputAdapter::getButtonFromTouch(uint16_t x, uint16_t y) {
  // Check which button area the touch is in
  for (uint8_t i = 0; i < NUM_BUTTON_AREAS; i++) {
    const ButtonArea& area = BUTTON_AREAS[i];
    if (x >= area.x && x < (area.x + area.width) &&
        y >= area.y && y < (area.y + area.height)) {
      return area.logicalButton;
    }
  }
  
  // No virtual-button hit
  return UINT8_MAX;
}
