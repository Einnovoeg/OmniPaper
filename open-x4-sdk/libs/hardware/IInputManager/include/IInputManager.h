#pragma once
#include <Arduino.h>

// Abstract interface for input managers
// Allows different hardware implementations (X4 ADC buttons, M5Paper buttons+touch, etc.) to be used interchangeably
class IInputManager {
 public:
  // Virtual destructor for proper cleanup
  virtual ~IInputManager() = default;

  // Button indices (standardized across implementations)
  static constexpr uint8_t BTN_BACK = 0;
  static constexpr uint8_t BTN_CONFIRM = 1;
  static constexpr uint8_t BTN_LEFT = 2;
  static constexpr uint8_t BTN_RIGHT = 3;
  static constexpr uint8_t BTN_UP = 4;
  static constexpr uint8_t BTN_DOWN = 5;
  static constexpr uint8_t BTN_POWER = 6;

  // Initialize input hardware and driver
  virtual void begin() = 0;

  // Updates input states. Should be called regularly in main loop.
  virtual void update() = 0;

  // Get current input state as bitmask
  virtual uint8_t getState() = 0;

  // Button state methods
  virtual bool isPressed(uint8_t buttonIndex) const = 0;
  virtual bool wasPressed(uint8_t buttonIndex) const = 0;
  virtual bool wasAnyPressed() const = 0;
  virtual bool wasReleased(uint8_t buttonIndex) const = 0;
  virtual bool wasAnyReleased() const = 0;

  // Power button specific methods
  virtual bool isPowerButtonPressed() const = 0;

  // Timing methods
  virtual unsigned long getHeldTime() const = 0;

  // Touch support (optional for implementations with touch screens)
  virtual bool hasTouchSupport() const { return false; }
  virtual bool isTouchPressed() const { return false; }
  virtual uint16_t getTouchX() const { return 0; }
  virtual uint16_t getTouchY() const { return 0; }
  virtual bool wasTapped() const { return false; }

  // Button names for debugging
  virtual const char* getButtonName(uint8_t buttonIndex) const = 0;

  // Input capabilities
  virtual bool hasPhysicalButtons() const { return true; }
  virtual uint8_t getButtonCount() const { return 6; }
};