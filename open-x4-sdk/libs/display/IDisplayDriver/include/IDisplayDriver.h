#pragma once
#include <Arduino.h>

// Abstract interface for e-ink display drivers
// Allows different hardware implementations (X4, M5Paper, etc.) to be used interchangeably
class IDisplayDriver {
 public:
  // Virtual destructor for proper cleanup
  virtual ~IDisplayDriver() = default;

  // Display refresh modes (mapped to hardware-specific modes)
  enum RefreshMode {
    FULL_REFRESH,   // Full refresh with complete waveform
    HALF_REFRESH,   // Balanced refresh mode
    FAST_REFRESH    // Fast refresh for quick updates
  };

  // Initialize display hardware and driver
  virtual void begin() = 0;

  // Display dimensions (must be provided by implementation)
  virtual uint16_t getWidth() const = 0;
  virtual uint16_t getHeight() const = 0;
  virtual uint32_t getBufferSize() const = 0;

  // Frame buffer operations
  virtual void clearScreen(uint8_t color = 0xFF) = 0;
  virtual void drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool fromProgmem = false) = 0;

  // Frame buffer access and management
  virtual uint8_t* getFrameBuffer() = 0;

  // Display update operations
  virtual void displayBuffer(RefreshMode mode = FAST_REFRESH) = 0;
  
  // Optional windowed update (default implementation does full refresh)
  virtual void displayWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    displayBuffer(); // Default to full refresh
  }

  // Power management
  virtual void deepSleep() = 0;

  // Advanced framebuffer operations for grayscale
  virtual void setFramebuffer(const uint8_t* buffer) = 0;
  virtual void copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) = 0;
  virtual void copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) = 0;
  virtual void copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) = 0;
  
#ifdef EINK_DISPLAY_SINGLE_BUFFER_MODE
  virtual void cleanupGrayscaleBuffers(const uint8_t* bwBuffer) = 0;
#else
  virtual void swapBuffers() = 0;
#endif

  // Display capabilities (can be overridden by implementations)
  virtual bool supportsGrayscale() const { return true; }
  virtual bool supportsWindowedUpdates() const { return false; }
  virtual uint8_t getGrayscaleLevels() const { return 16; }
};