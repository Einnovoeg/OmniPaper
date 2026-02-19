#pragma once
#include "../../IDisplayDriver/include/IDisplayDriver.h"
#include <M5GFX.h>
#include <M5Unified.h>

// M5Paper display driver adapter
// Implements IDisplayDriver interface for M5Stack M5Paper hardware
class M5PaperDisplayAdapter : public IDisplayDriver {
 public:
  // Constructor with M5EPD driver reference
  M5PaperDisplayAdapter();
  
  // Destructor
  ~M5PaperDisplayAdapter() override;

  // IDisplayDriver interface implementation
  void begin() override;
  
  uint16_t getWidth() const override { return 960; }
  uint16_t getHeight() const override { return 540; }
  uint32_t getBufferSize() const override { return (960 * 540) / 2; } // 4bpp
  
  void clearScreen(uint8_t color = 0xFF) override;
  void drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool fromProgmem = false) override;
  
  uint8_t* getFrameBuffer() override { return frameBuffer; }
  
  void displayBuffer(RefreshMode mode = FAST_REFRESH) override;
  void displayWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) override;
  
  void deepSleep() override;
  
  void setFramebuffer(const uint8_t* buffer) override;
  void copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) override;
  void copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) override;
  void copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) override;
  
#ifdef EINK_DISPLAY_SINGLE_BUFFER_MODE
  void cleanupGrayscaleBuffers(const uint8_t* bwBuffer) override;
#else
  void swapBuffers() override;
#endif

  // M5Paper-specific capabilities
  bool supportsWindowedUpdates() const override { return true; }
  uint8_t getGrayscaleLevels() const override { return 16; }

 private:
  // M5Unified/M5GFX display instance (owned by M5)
  M5GFX* display;
  
  // Frame buffer for M5Paper (4bpp - 16 grayscale levels)
  static constexpr uint16_t DISPLAY_WIDTH = 960;
  static constexpr uint16_t DISPLAY_HEIGHT = 540;
  static constexpr uint32_t BUFFER_SIZE = (DISPLAY_WIDTH * DISPLAY_HEIGHT) / 2; // 4bpp
  
  uint8_t* frameBuffer;
  
  // Helper methods
  lgfx::epd_mode_t mapRefreshMode(RefreshMode mode);
  bool ensureBuffer();
  void convert1bppTo4bpp(const uint8_t* src, uint8_t* dest, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void convertGrayscaleTo4bpp(const uint8_t* lsbBuffer, const uint8_t* msbBuffer);
  void setPixel4bpp(uint16_t x, uint16_t y, uint8_t grayscale);
  uint8_t getPixel4bpp(uint16_t x, uint16_t y) const;
};
