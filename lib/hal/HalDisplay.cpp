#include <HalDisplay.h>
#include <HalGPIO.h>

#define SD_SPI_MISO 7

#ifndef PLATFORM_M5PAPER

HalDisplay::HalDisplay() : einkDisplay(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY) {}

HalDisplay::~HalDisplay() {}

void HalDisplay::begin() { einkDisplay.begin(); }

void HalDisplay::clearScreen(uint8_t color) const { einkDisplay.clearScreen(color); }

void HalDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           bool fromProgmem) const {
  einkDisplay.drawImage(imageData, x, y, w, h, fromProgmem);
}

EInkDisplay::RefreshMode convertRefreshMode(HalDisplay::RefreshMode mode) {
  switch (mode) {
    case HalDisplay::FULL_REFRESH:
      return EInkDisplay::FULL_REFRESH;
    case HalDisplay::HALF_REFRESH:
      return EInkDisplay::HALF_REFRESH;
    case HalDisplay::FAST_REFRESH:
    default:
      return EInkDisplay::FAST_REFRESH;
  }
}

void HalDisplay::displayBuffer(HalDisplay::RefreshMode mode) { einkDisplay.displayBuffer(convertRefreshMode(mode)); }

void HalDisplay::refreshDisplay(HalDisplay::RefreshMode mode, bool turnOffScreen) {
  einkDisplay.refreshDisplay(convertRefreshMode(mode), turnOffScreen);
}

void HalDisplay::deepSleep() { einkDisplay.deepSleep(); }

uint8_t* HalDisplay::getFrameBuffer() const { return einkDisplay.getFrameBuffer(); }

void HalDisplay::copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) {
  einkDisplay.copyGrayscaleBuffers(lsbBuffer, msbBuffer);
}

void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) { einkDisplay.copyGrayscaleLsbBuffers(lsbBuffer); }

void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) { einkDisplay.copyGrayscaleMsbBuffers(msbBuffer); }

void HalDisplay::cleanupGrayscaleBuffers(const uint8_t* bwBuffer) { einkDisplay.cleanupGrayscaleBuffers(bwBuffer); }

void HalDisplay::displayGrayBuffer() { einkDisplay.displayGrayBuffer(); }

#else

#include <cstring>
#include <esp32-hal-psram.h>

namespace {
constexpr uint32_t kDisplayWaitTimeoutMs = 4000;

void waitDisplayWithTimeout() {
  const uint32_t start = millis();
  while (M5.Display.displayBusy()) {
    if (millis() - start >= kDisplayWaitTimeoutMs) {
      Serial.println("[M5Paper] displayBusy timeout in HalDisplay");
      break;
    }
    delay(1);
  }
}
}  // namespace

static IDisplayDriver::RefreshMode mapRefreshMode(HalDisplay::RefreshMode mode) {
  switch (mode) {
    case HalDisplay::FULL_REFRESH:
      return IDisplayDriver::FULL_REFRESH;
    case HalDisplay::HALF_REFRESH:
      return IDisplayDriver::HALF_REFRESH;
    case HalDisplay::FAST_REFRESH:
    default:
      return IDisplayDriver::FAST_REFRESH;
  }
}

HalDisplay::HalDisplay() : frameBuffer(nullptr) {}

HalDisplay::~HalDisplay() {
  if (frameBuffer) {
    free(frameBuffer);
    frameBuffer = nullptr;
  }
}

void HalDisplay::begin() {
  if (!ensureBuffer()) {
    Serial.println("[M5Paper] Failed to allocate HAL frame buffer");
  }
  epdDisplay.begin();

  // Force one direct IT8951 refresh after reset/flash so stale content is always cleared.
  if (M5.Display.width() < M5.Display.height()) {
    M5.Display.setRotation((M5.Display.getRotation() + 1) & 3);
  }
  M5.Display.setEpdMode(lgfx::epd_quality);
  M5.Display.setAutoDisplay(false);
  M5.Display.fillScreen(0xFFFFFF);
  M5.Display.drawRect(0, 0, M5.Display.width(), M5.Display.height(), 0x000000);
  M5.Display.display(0, 0, M5.Display.width(), M5.Display.height());
  waitDisplayWithTimeout();

  if (frameBuffer) {
    memset(frameBuffer, 0xFF, BUFFER_SIZE);
  }
}

void HalDisplay::clearScreen(uint8_t color) const {
  if (!frameBuffer) {
    return;
  }
  memset(frameBuffer, color, BUFFER_SIZE);
}

void HalDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           bool fromProgmem) const {
  if (!imageData || !frameBuffer) {
    return;
  }

  const uint16_t imageWidthBytes = w / 8;

  for (uint16_t row = 0; row < h; row++) {
    const uint16_t destY = y + row;
    if (destY >= DISPLAY_HEIGHT) {
      break;
    }

    const uint16_t destOffset = destY * DISPLAY_WIDTH_BYTES + (x / 8);
    const uint16_t srcOffset = row * imageWidthBytes;

    for (uint16_t col = 0; col < imageWidthBytes; col++) {
      if ((x / 8 + col) >= DISPLAY_WIDTH_BYTES) {
        break;
      }

      if (fromProgmem) {
        frameBuffer[destOffset + col] = pgm_read_byte(&imageData[srcOffset + col]);
      } else {
        frameBuffer[destOffset + col] = imageData[srcOffset + col];
      }
    }
  }
}

void HalDisplay::displayBuffer(HalDisplay::RefreshMode mode) {
  if (!frameBuffer) {
    return;
  }
  epdDisplay.setFramebuffer(frameBuffer);
  epdDisplay.displayBuffer(mapRefreshMode(mode));
}

void HalDisplay::refreshDisplay(HalDisplay::RefreshMode mode, bool turnOffScreen) {
  (void)turnOffScreen;
  displayBuffer(mode);
}

void HalDisplay::deepSleep() { epdDisplay.deepSleep(); }

uint8_t* HalDisplay::getFrameBuffer() const { return frameBuffer; }

void HalDisplay::copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) {
  epdDisplay.copyGrayscaleBuffers(lsbBuffer, msbBuffer);
}

void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) { epdDisplay.copyGrayscaleLsbBuffers(lsbBuffer); }

void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) { epdDisplay.copyGrayscaleMsbBuffers(msbBuffer); }

void HalDisplay::cleanupGrayscaleBuffers(const uint8_t* bwBuffer) {
  (void)bwBuffer;
}

void HalDisplay::displayGrayBuffer() { displayBuffer(FAST_REFRESH); }

bool HalDisplay::ensureBuffer() {
  if (frameBuffer) {
    return true;
  }

  frameBuffer = static_cast<uint8_t*>(ps_malloc(BUFFER_SIZE));
  if (!frameBuffer) {
    frameBuffer = static_cast<uint8_t*>(malloc(BUFFER_SIZE));
  }

  if (frameBuffer) {
    memset(frameBuffer, 0xFF, BUFFER_SIZE);
  }

  return frameBuffer != nullptr;
}

#endif
