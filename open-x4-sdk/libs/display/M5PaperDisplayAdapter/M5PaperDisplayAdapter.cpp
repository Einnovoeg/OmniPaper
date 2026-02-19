#include "M5PaperDisplayAdapter.h"
#include <Arduino.h>
#include <SPI.h>
#include <esp32-hal-psram.h>

namespace {
// 4-bit grayscale palette (0=black .. 15=white).
const lgfx::bgr888_t kGrayPalette4bpp[16] = {
    lgfx::bgr888_t(0, 0, 0),       lgfx::bgr888_t(17, 17, 17),   lgfx::bgr888_t(34, 34, 34),
    lgfx::bgr888_t(51, 51, 51),    lgfx::bgr888_t(68, 68, 68),   lgfx::bgr888_t(85, 85, 85),
    lgfx::bgr888_t(102, 102, 102), lgfx::bgr888_t(119, 119, 119), lgfx::bgr888_t(136, 136, 136),
    lgfx::bgr888_t(153, 153, 153), lgfx::bgr888_t(170, 170, 170), lgfx::bgr888_t(187, 187, 187),
    lgfx::bgr888_t(204, 204, 204), lgfx::bgr888_t(221, 221, 221), lgfx::bgr888_t(238, 238, 238),
    lgfx::bgr888_t(255, 255, 255),
};

constexpr uint32_t kDisplayWaitTimeoutMs = 4000;

void waitDisplayWithTimeout(M5GFX* display) {
  if (!display) {
    return;
  }
  const uint32_t start = millis();
  while (display->displayBusy()) {
    if (millis() - start >= kDisplayWaitTimeoutMs) {
      Serial.println("[M5Paper] displayBusy timeout in adapter");
      break;
    }
    delay(1);
  }
}
}  // namespace

M5PaperDisplayAdapter::M5PaperDisplayAdapter() : display(nullptr), frameBuffer(nullptr) {
}

M5PaperDisplayAdapter::~M5PaperDisplayAdapter() {
  if (frameBuffer) {
    free(frameBuffer);
    frameBuffer = nullptr;
  }
}

void M5PaperDisplayAdapter::begin() {
  // M5.begin() should already be called by the platform init (see HalGPIO on M5Paper)
  display = &M5.Display;

  // Configure display for e-paper.
  if (display->width() < display->height()) {
    display->setRotation((display->getRotation() + 1) & 3);
  }
  display->setEpdMode(epd_quality);
  display->setColorDepth(lgfx::color_depth_t::grayscale_4bit);
  // We explicitly control when the IT8951 is refreshed.
  display->setAutoDisplay(false);

  Serial.printf("[M5Paper] Display configured: %dx%d rot=%d\n", display->width(), display->height(),
                display->getRotation());
  if (display->width() != DISPLAY_WIDTH || display->height() != DISPLAY_HEIGHT) {
    Serial.printf("[M5Paper] Warning: unexpected display geometry, expected %dx%d\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
  }

  if (!ensureBuffer()) {
    Serial.println("[M5Paper] Failed to allocate display buffer");
  }
}

void M5PaperDisplayAdapter::clearScreen(uint8_t color) {
  if (!ensureBuffer()) {
    return;
  }

  // Convert 1bpp color to 4bpp grayscale
  uint8_t fillColor = (color == 0x00) ? 0x00 : 0xFF;
  memset(frameBuffer, fillColor, BUFFER_SIZE);
}

void M5PaperDisplayAdapter::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool fromProgmem) {
  if (!ensureBuffer()) {
    return;
  }

  // Convert 1bpp image data to 4bpp and draw to framebuffer
  convert1bppTo4bpp(imageData, frameBuffer, x, y, w, h);
}

void M5PaperDisplayAdapter::displayBuffer(RefreshMode mode) {
  if (!display || !ensureBuffer()) {
    return;
  }

  const uint16_t targetW = display->width() < DISPLAY_WIDTH ? display->width() : DISPLAY_WIDTH;
  const uint16_t targetH = display->height() < DISPLAY_HEIGHT ? display->height() : DISPLAY_HEIGHT;

  display->setEpdMode(mapRefreshMode(mode));
  // Push 4bpp grayscale source buffer and explicitly trigger IT8951 update.
  display->pushImage(0, 0, targetW, targetH, frameBuffer, lgfx::color_depth_t::grayscale_4bit,
                     kGrayPalette4bpp);
  display->display(0, 0, targetW, targetH);
  waitDisplayWithTimeout(display);
}

void M5PaperDisplayAdapter::displayWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  displayBuffer(FAST_REFRESH);
}

void M5PaperDisplayAdapter::deepSleep() {
  // No-op for now; system deep sleep handled in HalGPIO
}

void M5PaperDisplayAdapter::setFramebuffer(const uint8_t* buffer) {
  if (!buffer || !ensureBuffer()) {
    return;
  }

  // Convert 1bpp buffer to 4bpp (2 pixels per byte)
  const uint32_t srcRowBytes = DISPLAY_WIDTH / 8;
  for (uint32_t y = 0; y < DISPLAY_HEIGHT; y++) {
    const uint8_t* srcRow = buffer + (y * srcRowBytes);
    uint32_t destIndex = (y * DISPLAY_WIDTH) / 2;
    for (uint32_t xByte = 0; xByte < srcRowBytes; xByte++) {
      const uint8_t src = srcRow[xByte];
      const uint8_t p0 = (src & 0x80) ? 0x0F : 0x00;
      const uint8_t p1 = (src & 0x40) ? 0x0F : 0x00;
      const uint8_t p2 = (src & 0x20) ? 0x0F : 0x00;
      const uint8_t p3 = (src & 0x10) ? 0x0F : 0x00;
      const uint8_t p4 = (src & 0x08) ? 0x0F : 0x00;
      const uint8_t p5 = (src & 0x04) ? 0x0F : 0x00;
      const uint8_t p6 = (src & 0x02) ? 0x0F : 0x00;
      const uint8_t p7 = (src & 0x01) ? 0x0F : 0x00;

      // Packed 4bpp format expected by LGFX pixelcopy: first pixel in high nibble.
      frameBuffer[destIndex + 0] = (p0 << 4) | p1;
      frameBuffer[destIndex + 1] = (p2 << 4) | p3;
      frameBuffer[destIndex + 2] = (p4 << 4) | p5;
      frameBuffer[destIndex + 3] = (p6 << 4) | p7;
      destIndex += 4;
    }
  }
}

void M5PaperDisplayAdapter::copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) {
  if (!ensureBuffer()) {
    return;
  }

  convertGrayscaleTo4bpp(lsbBuffer, msbBuffer);
}

void M5PaperDisplayAdapter::copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) {
  // Treat 1bpp LSB buffer as black/white for now
  setFramebuffer(lsbBuffer);
}

void M5PaperDisplayAdapter::copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) {
  // Treat 1bpp MSB buffer as black/white for now
  setFramebuffer(msbBuffer);
}

#ifdef EINK_DISPLAY_SINGLE_BUFFER_MODE
void M5PaperDisplayAdapter::cleanupGrayscaleBuffers(const uint8_t* bwBuffer) {
  // M5Paper doesn't need cleanup in single buffer mode
}
#else
void M5PaperDisplayAdapter::swapBuffers() {
  // M5Paper uses single buffer, so no swapping needed
}
#endif

// Private helper methods

lgfx::epd_mode_t M5PaperDisplayAdapter::mapRefreshMode(RefreshMode mode) {
  switch (mode) {
    case FULL_REFRESH:
      return lgfx::epd_quality;
    case HALF_REFRESH:
      return lgfx::epd_text;
    case FAST_REFRESH:
    default:
      return lgfx::epd_fastest;
  }
}

bool M5PaperDisplayAdapter::ensureBuffer() {
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

void M5PaperDisplayAdapter::convert1bppTo4bpp(const uint8_t* src, uint8_t* dest, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  // Convert 1bpp image data to 4bpp grayscale
  for (uint16_t row = 0; row < h && (y + row) < DISPLAY_HEIGHT; row++) {
    for (uint16_t col = 0; col < w && (x + col) < DISPLAY_WIDTH; col++) {
      uint16_t srcX = col;
      uint16_t srcY = row;
      uint16_t srcIndex = (srcY * (w / 8)) + (srcX / 8);
      uint8_t srcBit = 7 - (srcX % 8);
      
      bool pixelOn = (src[srcIndex] >> srcBit) & 0x01;
      uint8_t grayscale = pixelOn ? 0x0F : 0x00; // Black or white
      
      setPixel4bpp(x + col, y + row, grayscale);
    }
  }
}

void M5PaperDisplayAdapter::convertGrayscaleTo4bpp(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) {
  if (!lsbBuffer || !msbBuffer) {
    return;
  }

  const uint32_t srcRowBytes = DISPLAY_WIDTH / 8;
  for (uint32_t y = 0; y < DISPLAY_HEIGHT; y++) {
    const uint8_t* lsbRow = lsbBuffer + (y * srcRowBytes);
    const uint8_t* msbRow = msbBuffer + (y * srcRowBytes);
    uint32_t destIndex = (y * DISPLAY_WIDTH) / 2;
    for (uint32_t xByte = 0; xByte < srcRowBytes; xByte++) {
      const uint8_t lsb = lsbRow[xByte];
      const uint8_t msb = msbRow[xByte];

      const uint8_t g0 = (((msb & 0x80) ? 2 : 0) | ((lsb & 0x80) ? 1 : 0)) * 5;
      const uint8_t g1 = (((msb & 0x40) ? 2 : 0) | ((lsb & 0x40) ? 1 : 0)) * 5;
      const uint8_t g2 = (((msb & 0x20) ? 2 : 0) | ((lsb & 0x20) ? 1 : 0)) * 5;
      const uint8_t g3 = (((msb & 0x10) ? 2 : 0) | ((lsb & 0x10) ? 1 : 0)) * 5;
      const uint8_t g4 = (((msb & 0x08) ? 2 : 0) | ((lsb & 0x08) ? 1 : 0)) * 5;
      const uint8_t g5 = (((msb & 0x04) ? 2 : 0) | ((lsb & 0x04) ? 1 : 0)) * 5;
      const uint8_t g6 = (((msb & 0x02) ? 2 : 0) | ((lsb & 0x02) ? 1 : 0)) * 5;
      const uint8_t g7 = (((msb & 0x01) ? 2 : 0) | ((lsb & 0x01) ? 1 : 0)) * 5;

      frameBuffer[destIndex + 0] = (g0 << 4) | g1;
      frameBuffer[destIndex + 1] = (g2 << 4) | g3;
      frameBuffer[destIndex + 2] = (g4 << 4) | g5;
      frameBuffer[destIndex + 3] = (g6 << 4) | g7;
      destIndex += 4;
    }
  }
}

void M5PaperDisplayAdapter::setPixel4bpp(uint16_t x, uint16_t y, uint8_t grayscale) {
  if (!frameBuffer) {
    return;
  }

  if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
  
  uint32_t pixelIndex = (y * DISPLAY_WIDTH) + x;
  uint32_t byteIndex = pixelIndex / 2;
  const uint8_t value = grayscale & 0x0F;

  if ((pixelIndex & 0x01u) == 0) {
    // Even pixel index -> high nibble
    frameBuffer[byteIndex] = (frameBuffer[byteIndex] & 0x0F) | static_cast<uint8_t>(value << 4);
  } else {
    // Odd pixel index -> low nibble
    frameBuffer[byteIndex] = (frameBuffer[byteIndex] & 0xF0) | value;
  }
}

uint8_t M5PaperDisplayAdapter::getPixel4bpp(uint16_t x, uint16_t y) const {
  if (!frameBuffer) {
    return 0;
  }

  if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return 0;
  
  uint32_t pixelIndex = (y * DISPLAY_WIDTH) + x;
  uint32_t byteIndex = pixelIndex / 2;
  if ((pixelIndex & 0x01u) == 0) {
    return (frameBuffer[byteIndex] >> 4) & 0x0F;
  }
  return frameBuffer[byteIndex] & 0x0F;
}
