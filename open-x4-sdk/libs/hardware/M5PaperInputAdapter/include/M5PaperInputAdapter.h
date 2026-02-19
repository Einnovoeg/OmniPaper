#pragma once
#include "../../IInputManager/include/IInputManager.h"
#include <M5Unified.h>

// M5Paper input manager adapter
// Implements IInputManager interface for M5Stack M5Paper hardware
// Handles both physical buttons and touch screen
class M5PaperInputAdapter : public IInputManager {
 public:
  // Constructor
  M5PaperInputAdapter();
  
  // Destructor
  ~M5PaperInputAdapter() override = default;

  // IInputManager interface implementation
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
  
  // Touch support implementation
  bool hasTouchSupport() const override { return true; }
  bool isTouchPressed() const override;
  uint16_t getTouchX() const override;
  uint16_t getTouchY() const override;
  bool wasTapped() const override;
  
  const char* getButtonName(uint8_t buttonIndex) const override;
  bool hasPhysicalButtons() const override { return true; }
  uint8_t getButtonCount() const override { return 7; }

 private:
  // Button mapping for M5Paper hardware
  // Maps logical buttons to physical M5Paper buttons
  enum M5PaperButtons {
    M5_BTN_LEFT = 0,    // GPIO 37 - Maps to BTN_LEFT
    M5_BTN_PUSH = 1,     // GPIO 38 - Maps to BTN_CONFIRM
    M5_BTN_RIGHT = 2     // GPIO 39 - Maps to BTN_RIGHT
  };
  
  // Button state tracking
  uint8_t currentState;
  uint8_t lastState;
  uint8_t pressedEvents;
  uint8_t releasedEvents;
  unsigned long lastDebounceTime;
  unsigned long buttonPressStart;
  unsigned long buttonPressFinish;
  
  // Touch state tracking
  bool touchActive;
  bool lastTouchActive;
  bool touchTapDetected;
  uint16_t touchX;
  uint16_t touchY;
  uint16_t lastTouchX;
  uint16_t lastTouchY;
  unsigned long touchStartTime;
  static constexpr unsigned long TAP_TIMEOUT = 200; // ms for tap detection
  
  // Helper methods
  uint8_t mapM5ButtonsToLogical();
  void updateTouchState();
  void updateButtonStates();
  bool isValidButtonIndex(uint8_t buttonIndex) const;
  
  // Touch-to-button mapping (for UI interactions)
  bool isTouchInButtonArea(uint16_t x, uint16_t y, uint8_t buttonIndex);
  uint8_t getButtonFromTouch(uint16_t x, uint16_t y);
  
  // Button area definitions for touch interaction
  struct ButtonArea {
    uint16_t x, y, width, height;
    uint8_t logicalButton;
  };
  
  static const ButtonArea BUTTON_AREAS[];
  static constexpr uint8_t NUM_BUTTON_AREAS = 6;
  
  static const char* BUTTON_NAMES[];
};
