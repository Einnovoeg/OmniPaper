#include "M5PaperInputAdapter.h"

#include <Arduino.h>
#include <cstdlib>

// Virtual touch zones for the 960x540 launcher/UI canvas.
const M5PaperInputAdapter::ButtonArea M5PaperInputAdapter::BUTTON_AREAS[NUM_BUTTON_AREAS] = {
    {40, 480, 80, 40, BTN_LEFT},      // Bottom-left
    {480, 480, 80, 40, BTN_CONFIRM},  // Bottom-center
    {840, 480, 80, 40, BTN_RIGHT},    // Bottom-right
    {40, 240, 80, 40, BTN_UP},        // Mid-left
    {840, 240, 80, 40, BTN_DOWN},     // Mid-right
    {880, 40, 80, 40, BTN_BACK}       // Top-right
};

const char* M5PaperInputAdapter::BUTTON_NAMES[] = {"BACK", "CONFIRM", "LEFT", "RIGHT", "UP", "DOWN", "POWER"};

M5PaperInputAdapter::M5PaperInputAdapter()
    : currentState(0),
      lastState(0),
      pressedEvents(0),
      releasedEvents(0),
      buttonPressStart(0),
      buttonPressFinish(0),
      touchActive(false),
      lastTouchActive(false),
      touchTapDetected(false),
      touchX(0),
      touchY(0),
      lastTouchX(0),
      lastTouchY(0),
      touchStartX(0),
      touchStartY(0),
      touchStartTime(0),
      pendingTouchEventMask(0) {}

void M5PaperInputAdapter::begin() {
  currentState = 0;
  lastState = 0;
  pressedEvents = 0;
  releasedEvents = 0;
  pendingTouchEventMask = 0;

  // Ensure touch is available on whichever M5Paper-family display was selected.
  if (!M5.Touch.isEnabled() && M5.Display.touch()) {
    M5.Touch.begin(&M5.Display);
  }
}

void M5PaperInputAdapter::update() {
  M5.update();

  lastState = currentState;
  lastTouchActive = touchActive;
  lastTouchX = touchX;
  lastTouchY = touchY;

  updateTouchState();
  updateButtonStates();

  const uint8_t stateChanges = currentState ^ lastState;
  pressedEvents = stateChanges & currentState;
  releasedEvents = stateChanges & lastState;

  if (wasAnyPressed()) {
    buttonPressStart = millis();
  }
  if (wasAnyReleased()) {
    buttonPressFinish = millis();
  }

  // Gesture-generated directions are one-shot events.
  pendingTouchEventMask = 0;
}

uint8_t M5PaperInputAdapter::getState() { return currentState; }

bool M5PaperInputAdapter::isPressed(const uint8_t buttonIndex) const {
  if (!isValidButtonIndex(buttonIndex)) {
    return false;
  }
  return (currentState >> buttonIndex) & 0x01;
}

bool M5PaperInputAdapter::wasPressed(const uint8_t buttonIndex) const {
  if (!isValidButtonIndex(buttonIndex)) {
    return false;
  }
  return (pressedEvents >> buttonIndex) & 0x01;
}

bool M5PaperInputAdapter::wasAnyPressed() const { return pressedEvents != 0; }

bool M5PaperInputAdapter::wasReleased(const uint8_t buttonIndex) const {
  if (!isValidButtonIndex(buttonIndex)) {
    return false;
  }
  return (releasedEvents >> buttonIndex) & 0x01;
}

bool M5PaperInputAdapter::wasAnyReleased() const { return releasedEvents != 0; }

bool M5PaperInputAdapter::isPowerButtonPressed() const { return isPressed(BTN_POWER); }

unsigned long M5PaperInputAdapter::getHeldTime() const {
  if (currentState != 0) {
    return millis() - buttonPressStart;
  }
  return 0;
}

bool M5PaperInputAdapter::isTouchPressed() const { return touchActive; }

uint16_t M5PaperInputAdapter::getTouchX() const { return touchX; }

uint16_t M5PaperInputAdapter::getTouchY() const { return touchY; }

bool M5PaperInputAdapter::wasTapped() const { return touchTapDetected; }

const char* M5PaperInputAdapter::getButtonName(const uint8_t buttonIndex) const {
  if (buttonIndex < 7) {
    return BUTTON_NAMES[buttonIndex];
  }
  return "UNKNOWN";
}

uint8_t M5PaperInputAdapter::mapM5ButtonsToLogical() {
  uint8_t logicalState = 0;

#if defined(PLATFORM_M5PAPERS3)
  // PaperS3 has one physical power key. Map it to both POWER and CONFIRM so
  // the UI remains operable even without touch.
  if (M5.BtnPWR.isPressed()) {
    logicalState |= (1 << BTN_POWER);
    logicalState |= (1 << BTN_CONFIRM);
  }
#else
  bool rightPressed = M5.BtnA.isPressed();
  bool centerPressed = M5.BtnB.isPressed();
  bool leftPressed = M5.BtnC.isPressed();

  // Legacy M5Paper raw GPIO fallback.
  rightPressed = rightPressed || (digitalRead(GPIO_NUM_37) == LOW);
  centerPressed = centerPressed || (digitalRead(GPIO_NUM_38) == LOW);
  leftPressed = leftPressed || (digitalRead(GPIO_NUM_39) == LOW);

  if (leftPressed) {
    logicalState |= (1 << BTN_LEFT);
  }
  if (centerPressed) {
    logicalState |= (1 << BTN_CONFIRM);
  }
  if (rightPressed) {
    logicalState |= (1 << BTN_RIGHT);
  }

  if (M5.BtnPWR.isPressed()) {
    logicalState |= (1 << BTN_POWER);
  }
#endif

  // Active touch in a virtual key zone acts as a held key.
  if (touchActive) {
    const uint8_t touchButton = getButtonFromTouch(touchX, touchY);
    if (touchButton < 7) {
      logicalState |= (1 << touchButton);
    }
  }

  // One-shot directional events from swipe gestures.
  logicalState |= pendingTouchEventMask;
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
      touchStartX = touchX;
      touchStartY = touchY;
    }
    return;
  }

  // Touch released.
  touchActive = false;
  if (!lastTouchActive) {
    return;
  }

  const unsigned long touchDuration = millis() - touchStartTime;
  const int dx = static_cast<int>(lastTouchX) - static_cast<int>(touchStartX);
  const int dy = static_cast<int>(lastTouchY) - static_cast<int>(touchStartY);
  const int adx = std::abs(dx);
  const int ady = std::abs(dy);

  // Short low-travel release -> tap.
  if (touchDuration <= TAP_TIMEOUT && adx <= TAP_MAX_TRAVEL && ady <= TAP_MAX_TRAVEL) {
    touchTapDetected = true;
    return;
  }

  // Swipe -> one directional key event.
  if (adx >= SWIPE_MIN_DISTANCE || ady >= SWIPE_MIN_DISTANCE) {
    if (adx > ady) {
      pendingTouchEventMask |= static_cast<uint8_t>(1 << (dx > 0 ? BTN_RIGHT : BTN_LEFT));
    } else {
      pendingTouchEventMask |= static_cast<uint8_t>(1 << (dy > 0 ? BTN_DOWN : BTN_UP));
    }
  }
}

void M5PaperInputAdapter::updateButtonStates() { currentState = mapM5ButtonsToLogical(); }

bool M5PaperInputAdapter::isValidButtonIndex(const uint8_t buttonIndex) const { return buttonIndex < 7; }

uint8_t M5PaperInputAdapter::getButtonFromTouch(const uint16_t x, const uint16_t y) {
  for (uint8_t i = 0; i < NUM_BUTTON_AREAS; i++) {
    const ButtonArea& area = BUTTON_AREAS[i];
    if (x >= area.x && x < (area.x + area.width) && y >= area.y && y < (area.y + area.height)) {
      return area.logicalButton;
    }
  }
  return UINT8_MAX;
}
