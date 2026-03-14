#pragma once

#include <M5Unified.h>

#include "../../IInputManager/include/IInputManager.h"

// Input adapter for M5Paper family boards.
//
// Legacy M5Paper exposes three front keys (A/B/C). PaperS3 exposes one physical
// power key plus touch. This adapter normalizes both into the logical button set
// used by the app (Back/Confirm/Left/Right/Up/Down/Power).
class M5PaperInputAdapter : public IInputManager {
 public:
  M5PaperInputAdapter();
  ~M5PaperInputAdapter() override = default;

  // IInputManager implementation
  void begin() override;
  void update() override;
  uint8_t getState() override;

  bool isPressed(uint8_t buttonIndex) const override;
  bool wasPressed(uint8_t buttonIndex) const override;
  bool wasAnyPressed() const override;
  bool wasReleased(uint8_t buttonIndex) const override;
  bool wasAnyReleased() const override;

  bool isPowerButtonPressed() const override;
  unsigned long getHeldTime() const override;

  // Touch support
  bool hasTouchSupport() const override { return true; }
  bool isTouchPressed() const override;
  uint16_t getTouchX() const override;
  uint16_t getTouchY() const override;
  bool wasTapped() const override;

  const char* getButtonName(uint8_t buttonIndex) const override;
  bool hasPhysicalButtons() const override { return true; }
  uint8_t getButtonCount() const override { return 7; }

 private:
  // Button state
  uint8_t currentState;
  uint8_t lastState;
  uint8_t pressedEvents;
  uint8_t releasedEvents;
  unsigned long buttonPressStart;
  unsigned long buttonPressFinish;

  // Touch state
  bool touchActive;
  bool lastTouchActive;
  bool touchTapDetected;
  uint16_t touchX;
  uint16_t touchY;
  uint16_t lastTouchX;
  uint16_t lastTouchY;
  uint16_t touchStartX;
  uint16_t touchStartY;
  unsigned long touchStartTime;

  // One-shot virtual button events generated from touch releases.
  uint8_t pendingTouchEventMask;

  // PaperS3 taps feel slower than a phone because the e-paper pipeline adds
  // visual latency. Keep tap recognition intentionally forgiving so normal
  // human taps still register even when the user waits for the screen to react.
  static constexpr unsigned long TAP_TIMEOUT = 650;  // ms
  static constexpr int TAP_MAX_TRAVEL = 48;          // px

  // Helpers
  uint8_t mapM5ButtonsToLogical();
  void updateTouchState();
  void updateButtonStates();
  bool isValidButtonIndex(uint8_t buttonIndex) const;
  uint8_t getButtonFromTouch(uint16_t x, uint16_t y);

  struct ButtonArea {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint8_t logicalButton;
  };

  static const ButtonArea BUTTON_AREAS[];
  static constexpr uint8_t NUM_BUTTON_AREAS = 6;
  static const char* BUTTON_NAMES[];
};
