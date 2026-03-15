#include "M5PaperInputAdapter.h"

#include <Arduino.h>

#include <cstdlib>

// Legacy fallback touch areas. PaperS3 uses logical portrait hit-testing in
// getButtonFromTouch() so the touch regions stay aligned with the shared
// PaperS3 UI chrome.
#if defined(PLATFORM_M5PAPERS3)
const M5PaperInputAdapter::ButtonArea M5PaperInputAdapter::BUTTON_AREAS[NUM_BUTTON_AREAS] = {
    {0, 0, 0, 0, BTN_LEFT}, {0, 0, 0, 0, BTN_CONFIRM}, {0, 0, 0, 0, BTN_RIGHT},
    {0, 0, 0, 0, BTN_UP},   {0, 0, 0, 0, BTN_DOWN},    {0, 0, 0, 0, BTN_BACK}};
#else
// Legacy M5Paper touch zones remain narrow because most navigation still comes
// from the front buttons rather than touch-first interaction.
const M5PaperInputAdapter::ButtonArea M5PaperInputAdapter::BUTTON_AREAS[NUM_BUTTON_AREAS] = {
    {40, 480, 80, 40, BTN_LEFT}, {480, 480, 80, 40, BTN_CONFIRM}, {840, 480, 80, 40, BTN_RIGHT},
    {40, 240, 80, 40, BTN_UP},   {840, 240, 80, 40, BTN_DOWN},    {880, 40, 80, 40, BTN_BACK}};
#endif

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

  // Touch-generated virtual key events are one-shot.
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

  // One-shot virtual key events from touch releases.
  logicalState |= pendingTouchEventMask;
  return logicalState;
}

void M5PaperInputAdapter::updateTouchState() {
  touchTapDetected = false;

  if (!M5.Touch.isEnabled()) {
    touchActive = false;
    return;
  }

  const auto detail = M5.Touch.getDetail();
  const bool touchPressed = detail.isPressed() || (M5.Touch.getCount() > 0);
  if (touchPressed) {
    touchActive = true;
    touchX = detail.x;
    touchY = detail.y;

#if defined(PLATFORM_M5PAPERS3)
    // PaperS3 feels much more responsive if touch-first screens react on the
    // initial press event instead of waiting for a quick release. The release
    // path is still tracked for drag/travel bookkeeping, but direct-touch UI
    // affordances should fire as soon as the finger lands.
    if (detail.wasPressed() || !lastTouchActive) {
      touchTapDetected = true;
      const uint8_t touchButton = getButtonFromTouch(touchX, touchY);
      if (touchButton < 7) {
        pendingTouchEventMask |= static_cast<uint8_t>(1 << touchButton);
      }
    }
#endif

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

#if defined(PLATFORM_M5PAPERS3)
  // PaperS3 one-shot tap events are generated on touch begin above so direct
  // touch feels immediate. Release still updates active state and travel data,
  // but does not emit a second tap event here.
  return;
#endif

  const unsigned long touchDuration = millis() - touchStartTime;
  const int dx = static_cast<int>(lastTouchX) - static_cast<int>(touchStartX);
  const int dy = static_cast<int>(lastTouchY) - static_cast<int>(touchStartY);
  const int adx = std::abs(dx);
  const int ady = std::abs(dy);

  // Short low-travel release -> tap. PaperS3 is now explicitly tap-first:
  // we intentionally do not interpret casual horizontal motion as navigation
  // because that made direct-touch targets feel unreliable in practice.
  if (touchDuration <= TAP_TIMEOUT && adx <= TAP_MAX_TRAVEL && ady <= TAP_MAX_TRAVEL) {
    touchTapDetected = true;
    const uint8_t touchButton = getButtonFromTouch(lastTouchX, lastTouchY);
    if (touchButton < 7) {
      pendingTouchEventMask |= static_cast<uint8_t>(1 << touchButton);
    }
    return;
  }
}

void M5PaperInputAdapter::updateButtonStates() { currentState = mapM5ButtonsToLogical(); }

bool M5PaperInputAdapter::isValidButtonIndex(const uint8_t buttonIndex) const { return buttonIndex < 7; }

uint8_t M5PaperInputAdapter::getButtonFromTouch(const uint16_t x, const uint16_t y) {
#if defined(PLATFORM_M5PAPERS3)
  // Convert raw panel coordinates into the shared 540x960 logical portrait
  // space used by the PaperS3 UI.
  const int logicalX = M5.Display.height() - 1 - static_cast<int>(y);
  const int logicalY = static_cast<int>(x);

  struct LogicalArea {
    int x;
    int y;
    int width;
    int height;
    uint8_t button;
  };

  constexpr LogicalArea logicalAreas[] = {
      {20, 848, 156, 72, BTN_LEFT}, {182, 848, 176, 72, BTN_CONFIRM}, {364, 848, 156, 72, BTN_RIGHT},
      {20, 238, 126, 66, BTN_UP},   {394, 238, 126, 66, BTN_DOWN},    {388, 94, 128, 56, BTN_BACK},
  };

  for (const auto& area : logicalAreas) {
    if (logicalX >= area.x && logicalX < (area.x + area.width) && logicalY >= area.y &&
        logicalY < (area.y + area.height)) {
      return area.button;
    }
  }
  return UINT8_MAX;
#endif

  for (uint8_t i = 0; i < NUM_BUTTON_AREAS; i++) {
    const ButtonArea& area = BUTTON_AREAS[i];
    if (x >= area.x && x < (area.x + area.width) && y >= area.y && y < (area.y + area.height)) {
      return area.logicalButton;
    }
  }
  return UINT8_MAX;
}
