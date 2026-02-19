#include "HardwareTestActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <Wire.h>
#include <cmath>

#include "MappedInputManager.h"
#include "CrossPointSettings.h"
#include "fontIds.h"
#include "util/TimeUtils.h"

#ifdef PLATFORM_M5PAPER
#include <M5Unified.h>
#endif

namespace {
constexpr unsigned long kUpdateIntervalMs = 250;

uint8_t crc8(const uint8_t* buf, int len) {
  uint8_t crc = 0xFF;
  for (int i = 0; i < len; i++) {
    crc ^= buf[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31) : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

bool readSht30Quick(float& tempC, float& humidity) {
  constexpr uint8_t kAddr = 0x44;

#ifdef PLATFORM_M5PAPER
  if (!M5.In_I2C.start(kAddr << 1, false, 100000)) return false;
  if (!M5.In_I2C.write(0x2C) || !M5.In_I2C.write(0x06)) {
    M5.In_I2C.stop();
    return false;
  }
  if (!M5.In_I2C.stop()) return false;
  delay(15);

  uint8_t data[6];
  if (!M5.In_I2C.start(kAddr << 1, true, 100000)) return false;
  const bool ok = M5.In_I2C.read(data, sizeof(data), true) && M5.In_I2C.stop();
  if (!ok) return false;
#else
  Wire.begin();
  Wire.beginTransmission(kAddr);
  Wire.write(0x2C);
  Wire.write(0x06);
  if (Wire.endTransmission() != 0) return false;
  delay(15);
  Wire.requestFrom(kAddr, static_cast<uint8_t>(6));
  if (Wire.available() < 6) return false;
  uint8_t data[6];
  for (int i = 0; i < 6; i++) data[i] = Wire.read();
#endif

  if (crc8(data, 2) != data[2] || crc8(data + 3, 2) != data[5]) return false;
  const uint16_t rawT = (static_cast<uint16_t>(data[0]) << 8) | data[1];
  const uint16_t rawH = (static_cast<uint16_t>(data[3]) << 8) | data[4];
  tempC = -45.0f + 175.0f * (static_cast<float>(rawT) / 65535.0f);
  humidity = 100.0f * (static_cast<float>(rawH) / 65535.0f);
  return true;
}
}

HardwareTestActivity::HardwareTestActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, HalGPIO& gpio,
                                           const std::function<void()>& onExit)
    : Activity("HardwareTest", renderer, mappedInput), gpio(gpio), onExit(onExit) {}

void HardwareTestActivity::onEnter() {
  Activity::onEnter();
  lastUpdate = 0;
  needsRender = true;
}

void HardwareTestActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExit) {
      onExit();
    }
    return;
  }

  if (millis() - lastUpdate > kUpdateIntervalMs) {
    needsRender = true;
    lastUpdate = millis();
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void HardwareTestActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "Hardware Test");

  int y = 60;
  auto drawState = [&](const char* label, bool pressed) {
    char line[64];
    snprintf(line, sizeof(line), "%s: %s", label, pressed ? "ON" : "off");
    renderer.drawText(UI_12_FONT_ID, 40, y, line);
    y += 26;
  };

  drawState("Back", mappedInput.isPressed(MappedInputManager::Button::Back));
  drawState("Confirm", mappedInput.isPressed(MappedInputManager::Button::Confirm));
  drawState("Left", mappedInput.isPressed(MappedInputManager::Button::Left));
  drawState("Right", mappedInput.isPressed(MappedInputManager::Button::Right));
  drawState("Up", mappedInput.isPressed(MappedInputManager::Button::Up));
  drawState("Down", mappedInput.isPressed(MappedInputManager::Button::Down));
  drawState("Power", mappedInput.isPressed(MappedInputManager::Button::Power));

#ifdef PLATFORM_M5PAPER
  const auto detail = M5.Touch.getDetail();
  char touchLine[64];
  snprintf(touchLine, sizeof(touchLine), "Touch: %s (%d,%d)", detail.isPressed() ? "ON" : "off", detail.x, detail.y);
  renderer.drawText(UI_12_FONT_ID, 40, y + 10, touchLine);
#endif

  y += 40;

  std::tm localTime {};
  char timeLine[64];
  if (TimeUtils::getLocalTimeWithOffset(localTime, SETTINGS.timezoneOffsetMinutes)) {
    snprintf(timeLine, sizeof(timeLine), "RTC: %04d/%02d/%02d %02d:%02d:%02d", localTime.tm_year + 1900,
             localTime.tm_mon + 1, localTime.tm_mday, localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
  } else {
    snprintf(timeLine, sizeof(timeLine), "RTC: Not set (sync in Time app)");
  }
  renderer.drawText(SMALL_FONT_ID, 40, y, timeLine);
  y += 18;

  const bool sdOk = SdMan.ready();
  renderer.drawText(SMALL_FONT_ID, 40, y, sdOk ? "SD: OK" : "SD: ERROR");
  y += 18;

  float tempC = NAN;
  float humidity = NAN;
  if (readSht30Quick(tempC, humidity)) {
    char shtLine[64];
    snprintf(shtLine, sizeof(shtLine), "SHT30: %.1fC %.1f%%", tempC, humidity);
    renderer.drawText(SMALL_FONT_ID, 40, y, shtLine);
  } else {
    renderer.drawText(SMALL_FONT_ID, 40, y, "SHT30: Not detected/readable");
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Back: Menu");
  renderer.displayBuffer();
}
