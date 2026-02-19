#include "SensorsActivity.h"

#include <Arduino.h>
#include <cmath>
#include <WiFi.h>
#include <Wire.h>

#include <array>

#ifdef PLATFORM_M5PAPER
#include <M5Unified.h>
#endif

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"

namespace {
constexpr unsigned long kUpdateIntervalMs = 5000;

const char* boolLabel(bool value) { return value ? "YES" : "NO"; }

bool readSht30(float& tempC, float& humidity) {
  constexpr uint8_t kAddr = 0x44;
  uint8_t data[6];
#ifdef PLATFORM_M5PAPER
  if (!M5.In_I2C.start(kAddr << 1, false, 100000)) {
    return false;
  }
  if (!M5.In_I2C.write(0x2C) || !M5.In_I2C.write(0x06)) {
    M5.In_I2C.stop();
    return false;
  }
  if (!M5.In_I2C.stop()) {
    return false;
  }

  delay(15);
  if (!M5.In_I2C.start(kAddr << 1, true, 100000)) {
    return false;
  }
  const bool ok = M5.In_I2C.read(data, sizeof(data), true) && M5.In_I2C.stop();
  if (!ok) {
    return false;
  }
#else
  Wire.begin();
  Wire.beginTransmission(kAddr);
  Wire.write(0x2C);
  Wire.write(0x06);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(15);
  Wire.requestFrom(kAddr, static_cast<uint8_t>(6));
  if (Wire.available() < 6) {
    return false;
  }
  for (int i = 0; i < 6; i++) {
    data[i] = Wire.read();
  }
#endif

  auto crc8 = [](const uint8_t* buf, int len) -> uint8_t {
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
      crc ^= buf[i];
      for (int b = 0; b < 8; b++) {
        if (crc & 0x80) {
          crc = (crc << 1) ^ 0x31;
        } else {
          crc <<= 1;
        }
      }
    }
    return crc;
  };

  if (crc8(data, 2) != data[2] || crc8(data + 3, 2) != data[5]) {
    return false;
  }

  const uint16_t rawT = (static_cast<uint16_t>(data[0]) << 8) | data[1];
  const uint16_t rawH = (static_cast<uint16_t>(data[3]) << 8) | data[4];

  tempC = -45.0f + 175.0f * (static_cast<float>(rawT) / 65535.0f);
  humidity = 100.0f * (static_cast<float>(rawH) / 65535.0f);
  return true;
}
}

SensorsActivity::SensorsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, HalGPIO& gpio, Mode mode,
                                 const std::function<void()>& onExit)
    : Activity("Sensors", renderer, mappedInput), gpio(gpio), mode(mode), onExit(onExit) {}

void SensorsActivity::onEnter() {
  Activity::onEnter();
  lastUpdate = 0;
  needsRender = true;
  updateData();
}

void SensorsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExit) {
      onExit();
    }
    return;
  }

  if (millis() - lastUpdate > kUpdateIntervalMs) {
    updateData();
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void SensorsActivity::updateData() {
  lastUpdate = millis();
  snapshot.uptimeMs = millis();
  snapshot.batteryPercent = gpio.getBatteryPercentage();

#ifdef PLATFORM_M5PAPER
  snapshot.batteryMv = M5.Power.getBatteryVoltage();
#else
  snapshot.batteryMv = 0;
#endif

  if (WiFi.status() == WL_CONNECTED) {
    snapshot.wifiRssi = WiFi.RSSI();
  } else {
    snapshot.wifiRssi = 0;
  }

  // Temperature/humidity: use placeholders unless external sensor exists
  snapshot.temperatureC = NAN;
  snapshot.humidity = NAN;

  if (mode == Mode::BuiltIn) {
    float t = 0.0f;
    float h = 0.0f;
    if (readSht30(t, h)) {
      snapshot.temperatureC = t;
      snapshot.humidity = h;
    }
  }

  if (mode == Mode::External) {
    scanI2c();
  }

  needsRender = true;
}

void SensorsActivity::scanI2c() {
  externalI2cDevices.clear();
#ifdef PLATFORM_M5PAPER
  if (!M5.Ex_I2C.begin()) {
    return;
  }
  std::array<bool, 120> found{};
  found.fill(false);
  M5.Ex_I2C.scanID(found.data(), 100000);
  for (uint8_t address = 1; address < 0x78; address++) {
    if (address < found.size() && found[address]) {
      externalI2cDevices.push_back(address);
    }
  }
#else
  Wire.begin();
  for (uint8_t address = 1; address < 0x78; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      externalI2cDevices.push_back(address);
    }
  }
#endif
}

void SensorsActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);

  const int titleY = 16;
  renderer.drawCenteredText(UI_12_FONT_ID, titleY, mode == Mode::BuiltIn ? "Sensors (Built-In)" : "Sensors (External)");

  if (mode == Mode::BuiltIn) {
    renderBuiltIn();
  } else {
    renderExternal();
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Back: Menu");
  renderer.displayBuffer();
}

void SensorsActivity::renderBuiltIn() {
  const int x = 40;
  int y = 70;

  char line[64];

  snprintf(line, sizeof(line), "Battery: %d%%", snapshot.batteryPercent);
  renderer.drawText(UI_12_FONT_ID, x, y, line);
  y += 30;

  if (snapshot.batteryMv > 0) {
    snprintf(line, sizeof(line), "Battery Voltage: %d mV", snapshot.batteryMv);
    renderer.drawText(UI_12_FONT_ID, x, y, line);
    y += 30;
  }

  snprintf(line, sizeof(line), "WiFi: %s", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  renderer.drawText(UI_12_FONT_ID, x, y, line);
  y += 30;

  if (WiFi.status() == WL_CONNECTED) {
    snprintf(line, sizeof(line), "RSSI: %d dBm", snapshot.wifiRssi);
    renderer.drawText(UI_12_FONT_ID, x, y, line);
    y += 30;
  }

  snprintf(line, sizeof(line), "Uptime: %lu s", snapshot.uptimeMs / 1000);
  renderer.drawText(UI_12_FONT_ID, x, y, line);
  y += 30;

  if (!std::isnan(snapshot.temperatureC) && !std::isnan(snapshot.humidity)) {
    char envLine[64];
    snprintf(envLine, sizeof(envLine), "Temp: %.1f C  Humidity: %.1f%%", snapshot.temperatureC, snapshot.humidity);
    renderer.drawText(UI_12_FONT_ID, x, y, envLine);
  } else {
    renderer.drawText(SMALL_FONT_ID, x, y + 10, "Temp/Humidity: Not available");
  }
}

void SensorsActivity::renderExternal() {
  const int x = 40;
  int y = 70;

  renderer.drawText(UI_12_FONT_ID, x, y, "I2C Scan Results:");
  y += 30;

  if (externalI2cDevices.empty()) {
    renderer.drawText(UI_12_FONT_ID, x, y, "No devices detected");
    return;
  }

  for (size_t i = 0; i < externalI2cDevices.size(); i++) {
    char line[32];
    snprintf(line, sizeof(line), "0x%02X", externalI2cDevices[i]);
    renderer.drawText(UI_12_FONT_ID, x, y, line);
    y += 24;
    if (y > renderer.getScreenHeight() - 60) {
      renderer.drawText(SMALL_FONT_ID, x, y, "(More devices not shown)");
      break;
    }
  }
}
