#include "HardwareTestActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <Wire.h>

#include <cmath>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "PaperS3Ui.h"
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
}  // namespace

HardwareTestActivity::HardwareTestActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, HalGPIO& gpio,
                                           const std::function<void()>& onExit)
    : Activity("HardwareTest", renderer, mappedInput), gpio(gpio), onExit(onExit) {}

void HardwareTestActivity::onEnter() {
  Activity::onEnter();
  lastUpdate = 0;
  needsRender = true;
  statusMessage.clear();
  statusMessageExpiresAt = 0;
}

void HardwareTestActivity::loop() {
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
    if (PaperS3Ui::primaryActionRect(renderer).contains(tapX, tapY)) {
      runPaperS3SpeakerTest();
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

#if defined(PLATFORM_M5PAPERS3)
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    runPaperS3SpeakerTest();
  }

  if (!statusMessage.empty() && millis() >= statusMessageExpiresAt) {
    statusMessage.clear();
    statusMessageExpiresAt = 0;
    needsRender = true;
  }
#endif

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
  PaperS3Ui::applyDefaultOrientation(renderer);

#if defined(PLATFORM_M5PAPERS3)
  renderPaperS3();
  return;
#endif

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "Hardware Test");

  int y = 52;
  auto drawState = [&](const char* label, bool pressed) {
    char line[64];
    snprintf(line, sizeof(line), "%s: %s", label, pressed ? "ON" : "off");
    renderer.drawText(SMALL_FONT_ID, 30, y, line);
    y += 16;
  };

  drawState("Back", mappedInput.isPressed(MappedInputManager::Button::Back));
  drawState("Confirm", mappedInput.isPressed(MappedInputManager::Button::Confirm));
  drawState("Left", mappedInput.isPressed(MappedInputManager::Button::Left));
  drawState("Right", mappedInput.isPressed(MappedInputManager::Button::Right));
  drawState("Up", mappedInput.isPressed(MappedInputManager::Button::Up));
  drawState("Down", mappedInput.isPressed(MappedInputManager::Button::Down));
  drawState("Power", mappedInput.isPressed(MappedInputManager::Button::Power));

#ifdef PLATFORM_M5PAPER
  const auto detail = M5.Touch.isEnabled() ? M5.Touch.getDetail() : m5::touch_detail_t{};
  char touchLine[80];
  snprintf(touchLine, sizeof(touchLine), "Touch: %s (%d,%d)", detail.isPressed() ? "ON" : "off", detail.x, detail.y);
  renderer.drawText(SMALL_FONT_ID, 30, y + 4, touchLine);
#endif

  y += 24;

  std::tm localTime{};
  char line[96];
  if (TimeUtils::getLocalTimeWithOffset(localTime, SETTINGS.timezoneOffsetMinutes)) {
    snprintf(line, sizeof(line), "RTC: %04d/%02d/%02d %02d:%02d:%02d", localTime.tm_year + 1900, localTime.tm_mon + 1,
             localTime.tm_mday, localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
  } else {
    snprintf(line, sizeof(line), "RTC: Not set (sync in Time app)");
  }
  renderer.drawText(SMALL_FONT_ID, 30, y, line);
  y += 16;

  const int batteryPct = gpio.getBatteryPercentage();
  snprintf(line, sizeof(line), "Battery: %d%%", batteryPct);
  renderer.drawText(SMALL_FONT_ID, 30, y, line);
  y += 16;

#ifdef PLATFORM_M5PAPER
  const int16_t mv = M5.Power.getBatteryVoltage();
  const bool charging = (M5.Power.isCharging() == m5::Power_Class::is_charging);
  snprintf(line, sizeof(line), "Battery mV: %d  Charging: %s", mv, charging ? "Yes" : "No");
  renderer.drawText(SMALL_FONT_ID, 30, y, line);
  y += 16;
#endif

  const bool sdOk = SdMan.ready();
  renderer.drawText(SMALL_FONT_ID, 30, y, sdOk ? "SD: OK" : "SD: ERROR");
  y += 16;

  float tempC = NAN;
  float humidity = NAN;
  if (readSht30Quick(tempC, humidity)) {
    snprintf(line, sizeof(line), "SHT30: %.1fC %.1f%%", tempC, humidity);
    renderer.drawText(SMALL_FONT_ID, 30, y, line);
    y += 16;
  } else {
    renderer.drawText(SMALL_FONT_ID, 30, y, "SHT30: Not detected/readable");
    y += 16;
  }

#ifdef PLATFORM_M5PAPER
  if (M5.Imu.isEnabled()) {
    M5.Imu.update();
    const m5::imu_data_t imu = M5.Imu.getImuData();
    snprintf(line, sizeof(line), "IMU Acc: %.2f %.2f %.2f", imu.accel.x, imu.accel.y, imu.accel.z);
    renderer.drawText(SMALL_FONT_ID, 30, y, line);
    y += 16;
    snprintf(line, sizeof(line), "IMU Gyr: %.2f %.2f %.2f", imu.gyro.x, imu.gyro.y, imu.gyro.z);
    renderer.drawText(SMALL_FONT_ID, 30, y, line);
    y += 16;
  }
#endif

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Back: Menu");
  renderer.displayBuffer();
}

#if defined(PLATFORM_M5PAPERS3)
void HardwareTestActivity::runPaperS3SpeakerTest() {
  if (!M5.Speaker.isEnabled()) {
    statusMessage = "Speaker path unavailable";
    statusMessageExpiresAt = millis() + 2500;
    needsRender = true;
    return;
  }

  const bool queued = M5.Speaker.tone(1760.0f, 120);
  statusMessage = queued ? "Speaker chirp sent" : "Speaker busy";
  statusMessageExpiresAt = millis() + 2500;
  needsRender = true;
}

void HardwareTestActivity::renderPaperS3() {
  const auto layout = PaperS3Ui::fourCardLayout(renderer);
  const int smallLineStep = renderer.getLineHeight(SMALL_FONT_ID) + 4;
  const bool sdOk = SdMan.ready();
  const auto touch = M5.Touch.isEnabled() ? M5.Touch.getDetail() : m5::touch_detail_t{};
  const uint8_t touchCount = M5.Touch.isEnabled() ? M5.Touch.getCount() : 0;
  const int batteryPct = gpio.getBatteryPercentage();
  const int16_t batteryMv = M5.Power.getBatteryVoltage();
  const int32_t batteryCurrentMa = M5.Power.getBatteryCurrent();
  const int16_t vbusMv = M5.Power.getVBUSVoltage();
  const bool usbCablePresent = PaperS3Ui::usbCablePresent(vbusMv);
  const bool charging = (M5.Power.isCharging() == m5::Power_Class::is_charging);

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 22, "Hardware Test", true, EpdFontFamily::BOLD);
  PaperS3Ui::drawBackButton(renderer);
  PaperS3Ui::drawPrimaryActionButton(renderer, "Speaker Test");

  PaperS3Ui::drawCard(renderer, layout.leftX, layout.topY, layout.cardWidth, layout.cardHeight, "Navigation");
  PaperS3Ui::drawCard(renderer, layout.rightX, layout.topY, layout.cardWidth, layout.cardHeight, "Touch + Power");
  PaperS3Ui::drawCard(renderer, layout.leftX, layout.bottomY, layout.cardWidth, layout.cardHeight, "Peripherals");
  PaperS3Ui::drawCard(renderer, layout.rightX, layout.bottomY, layout.cardWidth, layout.cardHeight, "Sensors");

  char line[128];
  int x = PaperS3Ui::bodyX(layout.leftX);
  int y = PaperS3Ui::bodyY(layout.topY);
  snprintf(line, sizeof(line), "Back: %s", mappedInput.isPressed(MappedInputManager::Button::Back) ? "ON" : "off");
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Confirm: %s",
           mappedInput.isPressed(MappedInputManager::Button::Confirm) ? "ON" : "off");
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Left/Right: %s / %s",
           mappedInput.isPressed(MappedInputManager::Button::Left) ? "ON" : "off",
           mappedInput.isPressed(MappedInputManager::Button::Right) ? "ON" : "off");
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Up/Down: %s / %s", mappedInput.isPressed(MappedInputManager::Button::Up) ? "ON" : "off",
           mappedInput.isPressed(MappedInputManager::Button::Down) ? "ON" : "off");
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Power: %s", mappedInput.isPressed(MappedInputManager::Button::Power) ? "ON" : "off");
  renderer.drawText(SMALL_FONT_ID, x, y, line);

  x = PaperS3Ui::bodyX(layout.rightX);
  y = PaperS3Ui::bodyY(layout.topY);
  snprintf(line, sizeof(line), "Touch points: %u", touchCount);
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Touch state: %s", touch.isPressed() ? "Pressed" : "Idle");
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Touch XY: (%d, %d)", touch.x, touch.y);
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Battery: %d%%  %d mV", batteryPct, batteryMv);
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Current: %ld mA", static_cast<long>(batteryCurrentMa));
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Charge/VBUS: %s / %d mV", PaperS3Ui::yesNo(charging), vbusMv);
  renderer.drawText(SMALL_FONT_ID, x, y, line);

  x = PaperS3Ui::bodyX(layout.leftX);
  y = PaperS3Ui::bodyY(layout.bottomY);
  std::tm localTime{};
  if (TimeUtils::getLocalTimeWithOffset(localTime, SETTINGS.timezoneOffsetMinutes)) {
    snprintf(line, sizeof(line), "RTC: %04d/%02d/%02d %02d:%02d:%02d", localTime.tm_year + 1900, localTime.tm_mon + 1,
             localTime.tm_mday, localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
  } else {
    snprintf(line, sizeof(line), "RTC: Not synced");
  }
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "USB cable: %s", PaperS3Ui::presentAbsent(usbCablePresent));
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "CDC session: %s", PaperS3Ui::openWaiting(static_cast<bool>(Serial)));
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Speaker: %s", PaperS3Ui::readyOff(M5.Speaker.isEnabled()));
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "IMU: %s", PaperS3Ui::readyOff(M5.Imu.isEnabled()));
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  renderer.drawText(SMALL_FONT_ID, x, y, sdOk ? "SD: OK" : "SD: ERROR");

  x = PaperS3Ui::bodyX(layout.rightX);
  y = PaperS3Ui::bodyY(layout.bottomY);
  float tempC = NAN;
  float humidity = NAN;
  if (readSht30Quick(tempC, humidity)) {
    snprintf(line, sizeof(line), "SHT30: %.1f C  %.1f%% RH", tempC, humidity);
  } else {
    snprintf(line, sizeof(line), "SHT30: Not detected/readable");
  }
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;

  if (M5.Imu.isEnabled()) {
    M5.Imu.update();
    const m5::imu_data_t imu = M5.Imu.getImuData();
    snprintf(line, sizeof(line), "Acc: %.2f %.2f %.2f", imu.accel.x, imu.accel.y, imu.accel.z);
    renderer.drawText(SMALL_FONT_ID, x, y, line);
    y += smallLineStep;
    snprintf(line, sizeof(line), "Gyr: %.2f %.2f %.2f", imu.gyro.x, imu.gyro.y, imu.gyro.z);
    renderer.drawText(SMALL_FONT_ID, x, y, line);
  } else {
    renderer.drawText(SMALL_FONT_ID, x, y, "IMU: Not enabled");
  }

  if (!statusMessage.empty()) {
    PaperS3Ui::drawFooterStatus(renderer, statusMessage.c_str());
  }
  PaperS3Ui::drawFooter(renderer, "Tap Speaker Test to chirp");
  renderer.displayBuffer();
}
#endif
