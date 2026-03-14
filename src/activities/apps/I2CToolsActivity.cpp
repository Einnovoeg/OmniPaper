#include "I2CToolsActivity.h"

// Source reference:
// - Diagnostics source project: https://github.com/geo-tp/ESP32-Bus-Pirate
// - Local implementation for OmniPaper is maintained in this file.

#include <algorithm>
#include <array>
#include <cstdlib>

#ifdef PLATFORM_M5PAPER
#include <M5Unified.h>
#else
#include <Wire.h>
#endif

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "PaperS3Ui.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"

namespace {
constexpr int kItemsPerPage = 12;
constexpr int kPaperS3ItemsPerPage = 6;
constexpr uint32_t kI2CFreq = 100000;

struct KnownDevice {
  uint8_t address;
  const char* label;
};

const KnownDevice kKnownDevices[] = {
    {0x23, "BH1750 Light"},
    {0x29, "VL53L0X ToF"},
    {0x3C, "SSD1306 OLED"},
    {0x3D, "SSD1306 OLED"},
    {0x40, "INA219/INA226"},
    {0x44, "SHT30/31"},
    {0x48, "ADS1115"},
    {0x5A, "MLX90614"},
    {0x68, "MPU6050/DS3231"},
    {0x69, "MPU6050"},
    {0x74, "AS7331 UV"},
    {0x76, "BMP280/BME280"},
    {0x77, "BMP280/BME280"},
};

const char* matchKnown(uint8_t address) {
  for (const auto& dev : kKnownDevices) {
    if (dev.address == address) {
      return dev.label;
    }
  }
  return "Unknown";
}
}  // namespace

I2CToolsActivity::I2CToolsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                   const std::function<void()>& onExit)
    : ActivityWithSubactivity("I2CTools", renderer, mappedInput), onExitCb(onExit) {}

void I2CToolsActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
  screen = Screen::Scan;
  selectionIndex = 0;
  statusMessage.clear();
  hasLast = false;

#ifdef PLATFORM_M5PAPER
  busReady = M5.Ex_I2C.begin();
#else
  Wire.begin();
  busReady = true;
#endif

  scanBus();
  needsRender = true;
}

void I2CToolsActivity::scanBus() {
  addresses.clear();
  statusMessage.clear();

  if (!busReady) {
    statusMessage = "External I2C not ready";
    return;
  }

#ifdef PLATFORM_M5PAPER
  std::array<bool, 120> found{};
  found.fill(false);
  M5.Ex_I2C.scanID(found.data(), kI2CFreq);
  for (uint8_t addr = 1; addr < 0x78; addr++) {
    if (addr < found.size() && found[addr]) {
      addresses.push_back(addr);
    }
  }
#else
  for (uint8_t addr = 1; addr < 0x78; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      addresses.push_back(addr);
    }
  }
#endif

  selectionIndex = 0;
}

void I2CToolsActivity::loop() {
  if (subActivity) {
    ActivityWithSubactivity::loop();
    return;
  }

#if defined(PLATFORM_M5PAPERS3)
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasTapped() && PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), tapX, tapY)) {
    if (PaperS3Ui::backButtonRect(renderer).contains(tapX, tapY)) {
      if (screen != Screen::Scan) {
        screen = Screen::Scan;
        needsRender = true;
      } else if (onExitCb) {
        onExitCb();
      }
      return;
    }

    if (screen == Screen::Scan) {
      const int page = selectionIndex / kPaperS3ItemsPerPage;
      const int start = page * kPaperS3ItemsPerPage;
      const int end = std::min(start + kPaperS3ItemsPerPage, static_cast<int>(addresses.size()));
      for (int i = start; i < end; i++) {
        if (PaperS3Ui::listRowRect(renderer, i - start).contains(tapX, tapY)) {
          selectionIndex = i;
          needsRender = true;
          return;
        }
      }

      if (PaperS3Ui::sideActionRect(renderer, false).contains(tapX, tapY)) {
        scanBus();
        needsRender = true;
        return;
      }
      if (PaperS3Ui::primaryActionRect(renderer).contains(tapX, tapY)) {
        enterIdentify();
        needsRender = true;
        return;
      }
      if (PaperS3Ui::sideActionRect(renderer, true).contains(tapX, tapY)) {
        enterMonitor();
        needsRender = true;
        return;
      }
    } else if (screen == Screen::Identify) {
      if (PaperS3Ui::primaryActionRect(renderer).contains(tapX, tapY)) {
        enterMonitor();
        needsRender = true;
        return;
      }
    } else if (screen == Screen::Monitor) {
      if (PaperS3Ui::listRowRect(renderer, 1).contains(tapX, tapY)) {
        promptMonitorBase();
        return;
      }
      if (PaperS3Ui::listRowRect(renderer, 2).contains(tapX, tapY)) {
        promptMonitorLength();
        return;
      }
      if (PaperS3Ui::sideActionRect(renderer, false).contains(tapX, tapY)) {
        monitorIntervalMs = std::max<uint16_t>(500, monitorIntervalMs / 2);
        needsRender = true;
        return;
      }
      if (PaperS3Ui::sideActionRect(renderer, true).contains(tapX, tapY)) {
        monitorIntervalMs = std::min<uint16_t>(5000, monitorIntervalMs * 2);
        needsRender = true;
        return;
      }
    }
  }
#endif

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (screen != Screen::Scan) {
      screen = Screen::Scan;
      needsRender = true;
      return;
    }
    if (onExitCb) {
      onExitCb();
    }
    return;
  }

  if (screen == Screen::Scan) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
      scanBus();
      needsRender = true;
      return;
    }
    if (!addresses.empty()) {
      if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
        selectionIndex = (selectionIndex - 1 + static_cast<int>(addresses.size())) % static_cast<int>(addresses.size());
        needsRender = true;
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
        selectionIndex = (selectionIndex + 1) % static_cast<int>(addresses.size());
        needsRender = true;
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
        enterIdentify();
        needsRender = true;
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
        enterMonitor();
        needsRender = true;
      }
    }
  } else if (screen == Screen::Identify) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      enterMonitor();
      needsRender = true;
    }
  } else if (screen == Screen::Monitor) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
      monitorIntervalMs = std::max<uint16_t>(500, monitorIntervalMs / 2);
      needsRender = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      monitorIntervalMs = std::min<uint16_t>(5000, monitorIntervalMs * 2);
      needsRender = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      promptMonitorBase();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      promptMonitorLength();
      return;
    }
    updateMonitor();
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void I2CToolsActivity::enterIdentify() {
  if (addresses.empty()) {
    return;
  }
  screen = Screen::Identify;
}

void I2CToolsActivity::enterMonitor() {
  if (addresses.empty()) {
    return;
  }
  screen = Screen::Monitor;
  hasLast = false;
  lastPollMs = millis() - monitorIntervalMs;
  statusMessage.clear();
  updateMonitor();
}

void I2CToolsActivity::updateMonitor() {
  if (addresses.empty()) {
    return;
  }
  const unsigned long now = millis();
  if (now - lastPollMs < monitorIntervalMs) {
    return;
  }
  lastPollMs = now;

  const size_t len = std::max<uint8_t>(1, std::min<uint8_t>(monitorLen, curRegs.size()));
  if (!readRegisters(addresses[selectionIndex], monitorBaseReg, curRegs.data(), len)) {
    statusMessage = "Read failed";
    return;
  }

  if (hasLast) {
    for (size_t i = 0; i < len; i++) {
      changed[i] = (curRegs[i] != lastRegs[i]);
    }
    for (size_t i = len; i < changed.size(); i++) {
      changed[i] = false;
    }
  } else {
    changed.fill(false);
  }
  lastRegs = curRegs;
  hasLast = true;
  needsRender = true;
}

void I2CToolsActivity::promptMonitorBase() {
  char initial[8];
  snprintf(initial, sizeof(initial), "0x%02X", monitorBaseReg);
  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, "Start Reg (hex)", initial, 4, 0, false,
      [this](const std::string& value) {
        std::string v = value;
        if (v.rfind("0x", 0) == 0 || v.rfind("0X", 0) == 0) {
          v = v.substr(2);
        }
        char* end = nullptr;
        const long reg = strtol(v.c_str(), &end, 16);
        if (end && *end == '\0' && reg >= 0 && reg <= 0xFF) {
          monitorBaseReg = static_cast<uint8_t>(reg);
          statusMessage = "Base updated";
          hasLast = false;
          lastPollMs = 0;
        } else {
          statusMessage = "Invalid base";
        }
        exitActivity();
        needsRender = true;
      },
      [this]() {
        exitActivity();
        needsRender = true;
      }));
}

void I2CToolsActivity::promptMonitorLength() {
  char initial[8];
  snprintf(initial, sizeof(initial), "%u", monitorLen);
  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, "Length (1-32)", initial, 2, 0, false,
      [this](const std::string& value) {
        char* end = nullptr;
        const long len = strtol(value.c_str(), &end, 10);
        if (end && *end == '\0' && len >= 1 && len <= 32) {
          monitorLen = static_cast<uint8_t>(len);
          statusMessage = "Length updated";
          hasLast = false;
          lastPollMs = 0;
        } else {
          statusMessage = "Invalid length";
        }
        exitActivity();
        needsRender = true;
      },
      [this]() {
        exitActivity();
        needsRender = true;
      }));
}

bool I2CToolsActivity::readRegisters(uint8_t address, uint8_t reg, uint8_t* data, size_t len) {
  if (!busReady) {
    return false;
  }
#ifdef PLATFORM_M5PAPER
  return M5.Ex_I2C.readRegister(address, reg, data, len, kI2CFreq);
#else
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  const size_t readCount = Wire.requestFrom(address, static_cast<uint8_t>(len));
  if (readCount < len) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    data[i] = Wire.read();
  }
  return true;
#endif
}

const char* I2CToolsActivity::identifyHint(uint8_t address) const { return matchKnown(address); }

void I2CToolsActivity::render() {
  renderer.clearScreen();
#if defined(PLATFORM_M5PAPERS3)
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  PaperS3Ui::drawScreenHeader(renderer, "I2C Tools",
                              screen == Screen::Scan ? "External bus scan"
                              : screen == Screen::Identify ? "Device identify"
                                                            : "Register monitor");
  PaperS3Ui::drawBackButton(renderer);

  switch (screen) {
    case Screen::Scan:
      renderScan();
      break;
    case Screen::Identify:
      renderIdentify();
      break;
    case Screen::Monitor:
      renderMonitor();
      break;
  }

  renderer.displayBuffer();
  return;
#endif

  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  renderer.drawCenteredText(UI_12_FONT_ID, 16, "I2C Tools (External)");

  switch (screen) {
    case Screen::Scan:
      renderScan();
      break;
    case Screen::Identify:
      renderIdentify();
      break;
    case Screen::Monitor:
      renderMonitor();
      break;
  }

  renderer.displayBuffer();
}

void I2CToolsActivity::renderScan() {
#if defined(PLATFORM_M5PAPERS3)
  {
    if (addresses.empty()) {
      renderer.drawCenteredText(UI_12_FONT_ID, 320, "No devices detected");
      PaperS3Ui::drawSideActionButton(renderer, false, "Rescan");
      PaperS3Ui::drawFooter(renderer, busReady ? "Connect a device, then rescan" : "External I2C not ready");
      return;
    }

    const int page = selectionIndex / kPaperS3ItemsPerPage;
    const int start = page * kPaperS3ItemsPerPage;
    const int end = std::min(start + kPaperS3ItemsPerPage, static_cast<int>(addresses.size()));
    for (int i = start; i < end; i++) {
      char title[16];
      snprintf(title, sizeof(title), "0x%02X", addresses[i]);
      PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, i - start), i == selectionIndex, title, "",
                             identifyHint(addresses[i]));
    }

    PaperS3Ui::drawSideActionButton(renderer, false, "Rescan");
    PaperS3Ui::drawPrimaryActionButton(renderer, "Identify");
    PaperS3Ui::drawSideActionButton(renderer, true, "Monitor");
    if (!statusMessage.empty()) {
      PaperS3Ui::drawFooterStatus(renderer, statusMessage.c_str());
    }
    char footer[48];
    snprintf(footer, sizeof(footer), "Showing %d-%d of %d", start + 1, end, static_cast<int>(addresses.size()));
    PaperS3Ui::drawFooter(renderer, footer);
    return;
  }
#endif

  if (!statusMessage.empty()) {
    renderer.drawCenteredText(SMALL_FONT_ID, 40, statusMessage.c_str());
  }

  if (addresses.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "No devices found");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                              "Left: Rescan   Back: Menu");
    return;
  }

  const int page = selectionIndex / kItemsPerPage;
  const int start = page * kItemsPerPage;
  const int end = std::min(start + kItemsPerPage, static_cast<int>(addresses.size()));
  int y = 70;
  for (int i = start; i < end; i++) {
    char line[32];
    snprintf(line, sizeof(line), "0x%02X", addresses[i]);
    if (i == selectionIndex) {
      renderer.fillRect(40, y - 6, renderer.getScreenWidth() - 80, 22, true);
      renderer.drawText(UI_12_FONT_ID, 50, y, line, false);
    } else {
      renderer.drawText(UI_12_FONT_ID, 50, y, line);
    }
    y += 24;
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                            "Left: Rescan   Confirm: Identify   Right: Monitor   Back: Menu");
}

void I2CToolsActivity::renderIdentify() {
#if defined(PLATFORM_M5PAPERS3)
  {
    if (addresses.empty()) {
      renderer.drawCenteredText(UI_12_FONT_ID, 320, "No device selected");
      PaperS3Ui::drawFooter(renderer, "Tap Back to return");
      return;
    }

    const uint8_t addr = addresses[selectionIndex];
    char addrLine[16];
    snprintf(addrLine, sizeof(addrLine), "0x%02X", addr);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 0), true, "Address", addrLine);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 1), false, "Detected as", identifyHint(addr));
    PaperS3Ui::drawPrimaryActionButton(renderer, "Monitor");
    PaperS3Ui::drawFooter(renderer, "Tap Monitor to watch registers");
    return;
  }
#endif

  if (addresses.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "No device selected");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Back: List");
    return;
  }

  const uint8_t addr = addresses[selectionIndex];
  char addrLine[32];
  snprintf(addrLine, sizeof(addrLine), "Address: 0x%02X", addr);
  renderer.drawCenteredText(UI_12_FONT_ID, 80, addrLine);

  const char* hint = identifyHint(addr);
  renderer.drawCenteredText(UI_12_FONT_ID, 120, "Possible:");
  renderer.drawCenteredText(UI_12_FONT_ID, 150, hint);

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Right: Monitor   Back: List");
}

void I2CToolsActivity::renderMonitor() {
#if defined(PLATFORM_M5PAPERS3)
  {
    if (addresses.empty()) {
      renderer.drawCenteredText(UI_12_FONT_ID, 320, "No device selected");
      PaperS3Ui::drawFooter(renderer, "Tap Back to return");
      return;
    }

    const uint8_t addr = addresses[selectionIndex];
    char addrLine[16];
    snprintf(addrLine, sizeof(addrLine), "0x%02X", addr);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 0), false, "Address", addrLine);

    char baseLine[16];
    snprintf(baseLine, sizeof(baseLine), "0x%02X", monitorBaseReg);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 1), false, "Base Register", baseLine,
                           "Tap row to edit");

    char lenLine[16];
    snprintf(lenLine, sizeof(lenLine), "%u bytes", monitorLen);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 2), false, "Length", lenLine,
                           "Tap row to edit");

    char intervalLine[16];
    snprintf(intervalLine, sizeof(intervalLine), "%ums", monitorIntervalMs);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 3), false, "Interval", intervalLine);

    const size_t len = std::max<uint8_t>(1, std::min<uint8_t>(monitorLen, curRegs.size()));
    const size_t rows = std::min<size_t>((len + 3) / 4, 3);
    for (size_t row = 0; row < rows; row++) {
      const size_t baseIndex = row * 4;
      const uint8_t reg = static_cast<uint8_t>(monitorBaseReg + baseIndex);
      char line[96];
      char* p = line;
      p += snprintf(p, sizeof(line), "%02X:", reg);
      for (size_t col = 0; col < 4; col++) {
        const size_t idx = baseIndex + col;
        if (idx >= len) {
          break;
        }
        p += snprintf(p, sizeof(line) - (p - line), " %02X%s", curRegs[idx], changed[idx] ? "*" : "");
      }
      PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, static_cast<int>(row) + 4), false, line);
    }

    PaperS3Ui::drawSideActionButton(renderer, false, "Slower");
    PaperS3Ui::drawSideActionButton(renderer, true, "Faster");
    if (!statusMessage.empty()) {
      PaperS3Ui::drawFooterStatus(renderer, statusMessage.c_str());
    }
    PaperS3Ui::drawFooter(renderer, "Tap Base or Length to open the keyboard");
    return;
  }
#endif

  if (addresses.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "No device selected");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Back: List");
    return;
  }

  const uint8_t addr = addresses[selectionIndex];
  char header[64];
  snprintf(header, sizeof(header), "0x%02X reg 0x%02X len %u  %ums", addr, monitorBaseReg, monitorLen,
           monitorIntervalMs);
  renderer.drawCenteredText(SMALL_FONT_ID, 44, header);

  int y = 70;
  const size_t len = std::max<uint8_t>(1, std::min<uint8_t>(monitorLen, curRegs.size()));
  const size_t rows = (len + 3) / 4;
  for (size_t row = 0; row < rows; row++) {
    const size_t baseIndex = row * 4;
    const uint8_t reg = static_cast<uint8_t>(monitorBaseReg + baseIndex);
    char line[96];
    char* p = line;
    p += snprintf(p, sizeof(line), "%02X:", reg);
    for (size_t col = 0; col < 4; col++) {
      const size_t idx = baseIndex + col;
      if (idx >= len) break;
      p += snprintf(p, sizeof(line) - (p - line), " %02X%s", curRegs[idx], changed[idx] ? "*" : "");
    }
    renderer.drawText(UI_12_FONT_ID, 40, y, line);
    y += 26;
    if (y > renderer.getScreenHeight() - 60) break;
  }

  if (!statusMessage.empty()) {
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 44, statusMessage.c_str());
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                            "Left/Right: Interval   Confirm: Base   Down: Len   Back: List");
}
