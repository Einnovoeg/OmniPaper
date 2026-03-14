#include "SensorsActivity.h"

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>

#include <array>
#include <cmath>

#ifdef PLATFORM_M5PAPER
#include <M5Unified.h>
#endif

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "PaperS3Ui.h"
#include "fontIds.h"

namespace {
constexpr unsigned long kUpdateIntervalMs = 5000;

std::string formatUptime(const unsigned long uptimeMs) {
  const unsigned long totalSeconds = uptimeMs / 1000;
  const unsigned long hours = totalSeconds / 3600;
  const unsigned long minutes = (totalSeconds % 3600) / 60;
  const unsigned long seconds = totalSeconds % 60;

  char line[32];
  snprintf(line, sizeof(line), "%luh %lum %lus", hours, minutes, seconds);
  return std::string(line);
}

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
        crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31) : static_cast<uint8_t>(crc << 1);
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

#ifdef PLATFORM_M5PAPER
bool readExternalSht30(float& tempC, float& humidity) {
  constexpr uint8_t kAddr = 0x44;
  uint8_t data[6];

  if (!M5.Ex_I2C.begin()) {
    return false;
  }
  if (!M5.Ex_I2C.start(kAddr << 1, false, 100000)) {
    return false;
  }
  if (!M5.Ex_I2C.write(0x2C) || !M5.Ex_I2C.write(0x06)) {
    M5.Ex_I2C.stop();
    return false;
  }
  if (!M5.Ex_I2C.stop()) {
    return false;
  }

  delay(15);
  if (!M5.Ex_I2C.start(kAddr << 1, true, 100000)) {
    return false;
  }
  const bool ok = M5.Ex_I2C.read(data, sizeof(data), true) && M5.Ex_I2C.stop();
  if (!ok) {
    return false;
  }

  auto crc8 = [](const uint8_t* buf, int len) -> uint8_t {
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
      crc ^= buf[i];
      for (int b = 0; b < 8; b++) {
        crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31) : static_cast<uint8_t>(crc << 1);
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
#endif

#ifdef PLATFORM_M5PAPER
bool readImu(float& ax, float& ay, float& az, float& gx, float& gy, float& gz) {
  if (!M5.Imu.isEnabled()) {
    return false;
  }

  M5.Imu.update();
  const m5::imu_data_t data = M5.Imu.getImuData();

  ax = data.accel.x;
  ay = data.accel.y;
  az = data.accel.z;
  gx = data.gyro.x;
  gy = data.gyro.y;
  gz = data.gyro.z;
  return true;
}
#endif
}  // namespace

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
#if defined(PLATFORM_M5PAPERS3)
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasTapped() &&
      PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), tapX, tapY)) {
    if (PaperS3Ui::backButtonRect(renderer).contains(tapX, tapY)) {
      if (onExit) {
        onExit();
      }
      return;
    }
  }
#endif

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
  snapshot.batteryMv = 0;
  snapshot.batteryCurrentMa = 0;
  snapshot.vbusMv = 0;
  snapshot.usbCablePresent = false;
  snapshot.charging = false;
  snapshot.rtcAvailable = false;
  snapshot.speakerAvailable = false;
  snapshot.usbSerialOpen = false;
  snapshot.touchCount = 0;
  snapshot.touchX = 0;
  snapshot.touchY = 0;

#ifdef PLATFORM_M5PAPER
  snapshot.batteryMv = M5.Power.getBatteryVoltage();
  snapshot.batteryCurrentMa = M5.Power.getBatteryCurrent();
  snapshot.charging = (M5.Power.isCharging() == m5::Power_Class::is_charging);
  snapshot.rtcAvailable = M5.Rtc.isEnabled();
  snapshot.speakerAvailable = M5.Speaker.isEnabled();
#if defined(PLATFORM_M5PAPERS3)
  snapshot.vbusMv = M5.Power.getVBUSVoltage();
  snapshot.usbCablePresent = PaperS3Ui::usbCablePresent(snapshot.vbusMv);
  snapshot.usbSerialOpen = static_cast<bool>(Serial);
  snapshot.touchCount = M5.Touch.isEnabled() ? M5.Touch.getCount() : 0;
  if (snapshot.touchCount > 0) {
    const auto detail = M5.Touch.getDetail();
    snapshot.touchX = detail.x;
    snapshot.touchY = detail.y;
  }
#endif
#endif

  if (WiFi.status() == WL_CONNECTED) {
    snapshot.wifiRssi = WiFi.RSSI();
  } else {
    snapshot.wifiRssi = 0;
  }

  snapshot.temperatureC = NAN;
  snapshot.humidity = NAN;
  snapshot.hasSht30 = false;
  snapshot.hasImu = false;

  if (mode == Mode::BuiltIn) {
#if !defined(PLATFORM_M5PAPERS3)
    float t = 0.0f;
    float h = 0.0f;
    if (readSht30(t, h)) {
      snapshot.temperatureC = t;
      snapshot.humidity = h;
      snapshot.hasSht30 = true;
    }
#endif

#ifdef PLATFORM_M5PAPER
    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 0.0f;
    if (readImu(ax, ay, az, gx, gy, gz)) {
      snapshot.hasImu = true;
      snapshot.accelX = ax;
      snapshot.accelY = ay;
      snapshot.accelZ = az;
      snapshot.gyroX = gx;
      snapshot.gyroY = gy;
      snapshot.gyroZ = gz;
    }
#endif
  }

  if (mode == Mode::External) {
    float t = 0.0f;
    float h = 0.0f;
#ifdef PLATFORM_M5PAPER
    if (readExternalSht30(t, h)) {
      snapshot.temperatureC = t;
      snapshot.humidity = h;
      snapshot.hasSht30 = true;
    }
#endif
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
  PaperS3Ui::applyDefaultOrientation(renderer);

#if defined(PLATFORM_M5PAPERS3)
  if (mode == Mode::BuiltIn) {
    renderPaperS3BuiltIn();
    PaperS3Ui::drawBackButton(renderer);
    PaperS3Ui::drawFooter(renderer, "Tap Back to return");
    renderer.displayBuffer();
    return;
  }
  if (mode == Mode::External) {
    renderExternal();
    PaperS3Ui::drawFooter(renderer, "Tap Back to return");
    renderer.displayBuffer();
    return;
  }
#endif

  renderer.drawCenteredText(UI_12_FONT_ID, 16, mode == Mode::BuiltIn ? "Sensors (Built-In)" : "Sensors (External)");

  if (mode == Mode::BuiltIn) {
    renderBuiltIn();
  } else {
    renderExternal();
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Back: Menu");
  renderer.displayBuffer();
}

void SensorsActivity::renderBuiltIn() {
#if defined(PLATFORM_M5PAPERS3)
  renderPaperS3BuiltIn();
  return;
#endif

  const int x = 40;
  int y = 62;

  char line[96];

  snprintf(line, sizeof(line), "Battery: %d%%", snapshot.batteryPercent);
  renderer.drawText(UI_12_FONT_ID, x, y, line);
  y += 28;

  if (snapshot.batteryMv > 0) {
    snprintf(line, sizeof(line), "Battery Voltage: %d mV", snapshot.batteryMv);
    renderer.drawText(UI_10_FONT_ID, x, y, line);
    y += 22;
  }

  snprintf(line, sizeof(line), "Charging: %s", snapshot.charging ? "Yes" : "No");
  renderer.drawText(UI_10_FONT_ID, x, y, line);
  y += 22;

  snprintf(line, sizeof(line), "WiFi: %s", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  renderer.drawText(UI_10_FONT_ID, x, y, line);
  y += 22;

  if (WiFi.status() == WL_CONNECTED) {
    snprintf(line, sizeof(line), "RSSI: %d dBm", snapshot.wifiRssi);
    renderer.drawText(UI_10_FONT_ID, x, y, line);
    y += 22;
  }

  snprintf(line, sizeof(line), "Uptime: %lu s", snapshot.uptimeMs / 1000);
  renderer.drawText(UI_10_FONT_ID, x, y, line);
  y += 22;

  if (snapshot.hasSht30 && !std::isnan(snapshot.temperatureC) && !std::isnan(snapshot.humidity)) {
    snprintf(line, sizeof(line), "SHT30: %.1f C  %.1f%% RH", snapshot.temperatureC, snapshot.humidity);
    renderer.drawText(UI_10_FONT_ID, x, y, line);
    y += 22;
  } else {
    renderer.drawText(SMALL_FONT_ID, x, y + 4, "SHT30: Not available");
    y += 20;
  }

  if (snapshot.hasImu) {
    snprintf(line, sizeof(line), "IMU Acc: %.2f %.2f %.2f", snapshot.accelX, snapshot.accelY, snapshot.accelZ);
    renderer.drawText(SMALL_FONT_ID, x, y, line);
    y += 18;
    snprintf(line, sizeof(line), "IMU Gyr: %.2f %.2f %.2f", snapshot.gyroX, snapshot.gyroY, snapshot.gyroZ);
    renderer.drawText(SMALL_FONT_ID, x, y, line);
  }
}

#if defined(PLATFORM_M5PAPERS3)
void SensorsActivity::renderPaperS3BuiltIn() {
  const auto layout = PaperS3Ui::fourCardLayout(renderer);
  const int smallLineStep = renderer.getLineHeight(SMALL_FONT_ID) + 4;
  const std::string uptimeLabel = formatUptime(snapshot.uptimeMs);

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "Sensors (Built-In)");

  PaperS3Ui::drawCard(renderer, layout.leftX, layout.topY, layout.cardWidth, layout.cardHeight, "Power");
  PaperS3Ui::drawCard(renderer, layout.rightX, layout.topY, layout.cardWidth, layout.cardHeight, "Radio + System");
  PaperS3Ui::drawCard(renderer, layout.leftX, layout.bottomY, layout.cardWidth, layout.cardHeight, "Peripherals");
  PaperS3Ui::drawCard(renderer, layout.rightX, layout.bottomY, layout.cardWidth, layout.cardHeight, "Motion + Input");

  char line[128];
  int x = PaperS3Ui::bodyX(layout.leftX);
  int y = PaperS3Ui::bodyY(layout.topY);
  snprintf(line, sizeof(line), "Battery: %d%%", snapshot.batteryPercent);
  renderer.drawText(UI_12_FONT_ID, x, y, line);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 6;
  snprintf(line, sizeof(line), "Voltage: %d mV", snapshot.batteryMv);
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Current: %ld mA", static_cast<long>(snapshot.batteryCurrentMa));
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Charging: %s", PaperS3Ui::yesNo(snapshot.charging));
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "VBUS: %d mV", snapshot.vbusMv);
  renderer.drawText(SMALL_FONT_ID, x, y, line);

  x = PaperS3Ui::bodyX(layout.rightX);
  y = PaperS3Ui::bodyY(layout.topY);
  snprintf(line, sizeof(line), "WiFi: %s", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  if (WiFi.status() == WL_CONNECTED) {
    snprintf(line, sizeof(line), "RSSI: %d dBm", snapshot.wifiRssi);
    renderer.drawText(SMALL_FONT_ID, x, y, line);
    y += smallLineStep;
  }
  snprintf(line, sizeof(line), "USB cable: %s", PaperS3Ui::presentAbsent(snapshot.usbCablePresent));
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "CDC session: %s", PaperS3Ui::openWaiting(snapshot.usbSerialOpen));
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Uptime: %s", uptimeLabel.c_str());
  renderer.drawText(SMALL_FONT_ID, x, y, line);

  x = PaperS3Ui::bodyX(layout.leftX);
  y = PaperS3Ui::bodyY(layout.bottomY);
  renderer.drawText(SMALL_FONT_ID, x, y, "No built-in UV sensor");
  y += smallLineStep;
  renderer.drawText(SMALL_FONT_ID, x, y, "No built-in env sensor");
  y += smallLineStep;
  renderer.drawText(SMALL_FONT_ID, x, y, "Use Optional Devices");
  y += smallLineStep;
  snprintf(line, sizeof(line), "RTC: %s", PaperS3Ui::readyOff(snapshot.rtcAvailable));
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Speaker path: %s", PaperS3Ui::readyOff(snapshot.speakerAvailable));
  renderer.drawText(SMALL_FONT_ID, x, y, line);

  x = PaperS3Ui::bodyX(layout.rightX);
  y = PaperS3Ui::bodyY(layout.bottomY);
  snprintf(line, sizeof(line), "Touch points: %u", snapshot.touchCount);
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  if (snapshot.touchCount > 0) {
    snprintf(line, sizeof(line), "Touch XY: (%d, %d)", snapshot.touchX, snapshot.touchY);
    renderer.drawText(SMALL_FONT_ID, x, y, line);
    y += smallLineStep;
  }

  if (snapshot.hasImu) {
    snprintf(line, sizeof(line), "Acc: %.2f %.2f %.2f", snapshot.accelX, snapshot.accelY, snapshot.accelZ);
    renderer.drawText(SMALL_FONT_ID, x, y, line);
    y += smallLineStep;
    snprintf(line, sizeof(line), "Gyr: %.2f %.2f %.2f", snapshot.gyroX, snapshot.gyroY, snapshot.gyroZ);
    renderer.drawText(SMALL_FONT_ID, x, y, line);
  } else {
    renderer.drawText(SMALL_FONT_ID, x, y, "IMU: Not available");
  }
}
#endif

void SensorsActivity::renderExternal() {
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  PaperS3Ui::drawScreenHeader(renderer, "Optional Devices", "External sensor bus");
  PaperS3Ui::drawBackButton(renderer);

  int row = 0;
  if (snapshot.hasSht30 && !std::isnan(snapshot.temperatureC) && !std::isnan(snapshot.humidity)) {
    char subtitle[64];
    snprintf(subtitle, sizeof(subtitle), "%.1f C  |  %.1f%% RH", snapshot.temperatureC, snapshot.humidity);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, row++), false, "Environmental Sensor", "Active",
                           subtitle);
  }

  for (size_t i = 0; i < externalI2cDevices.size() && row < 8; i++, row++) {
    char title[32];
    snprintf(title, sizeof(title), "I2C device 0x%02X", externalI2cDevices[i]);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, row), false, title, "Detected", "External bus");
  }

  if (row == 0) {
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 0), false, "No devices detected", "",
                           "Connect a peripheral to the Grove / I2C port");
  }
}
