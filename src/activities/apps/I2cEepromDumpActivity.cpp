#include "I2cEepromDumpActivity.h"

// Source reference:
// - Diagnostics source project: https://github.com/geo-tp/ESP32-Bus-Pirate
// - Local implementation for OmniPaper is maintained in this file.

#include <algorithm>
#include <cstdlib>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"

namespace {
#ifdef PLATFORM_M5PAPER
constexpr int kExI2cSda = 25;
constexpr int kExI2cScl = 32;
#endif
constexpr uint8_t kFirstEepromAddr = 0x50;
constexpr uint8_t kLastEepromAddr = 0x57;
constexpr int kBytesPerRow = 8;
constexpr int kRowsPerPage = 8;
}  // namespace

I2cEepromDumpActivity::I2cEepromDumpActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                             const std::function<void()>& onExit)
    : ActivityWithSubactivity("I2cEepromDump", renderer, mappedInput), onExitCb(onExit), i2c(1) {}

void I2cEepromDumpActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
  statusMessage.clear();
  screen = Screen::SelectDevice;
  selectionIndex = 0;
  viewOffset = 0;
  dumpBytes.clear();
#ifdef PLATFORM_M5PAPER
  i2c.begin(kExI2cSda, kExI2cScl);
  i2c.setClock(100000);
#else
  i2c.begin();
#endif
  scanDevices();
  needsRender = true;
}

void I2cEepromDumpActivity::scanDevices() {
  devices.clear();
  for (uint8_t addr = kFirstEepromAddr; addr <= kLastEepromAddr; addr++) {
    i2c.beginTransmission(addr);
    if (i2c.endTransmission() == 0) {
      devices.push_back(addr);
    }
  }
  if (selectionIndex >= static_cast<int>(devices.size())) {
    selectionIndex = 0;
  }
  if (devices.empty()) {
    statusMessage = "No 24xx EEPROM found";
  } else {
    statusMessage.clear();
  }
}

bool I2cEepromDumpActivity::readByte(const uint8_t deviceAddr, const uint16_t memAddr, uint8_t& out) {
  i2c.beginTransmission(deviceAddr);
  if (use16bitAddress) {
    i2c.write(static_cast<uint8_t>((memAddr >> 8) & 0xFF));
  }
  i2c.write(static_cast<uint8_t>(memAddr & 0xFF));
  if (i2c.endTransmission(false) != 0) {
    return false;
  }
  if (i2c.requestFrom(static_cast<int>(deviceAddr), 1) != 1) {
    return false;
  }
  out = static_cast<uint8_t>(i2c.read());
  return true;
}

void I2cEepromDumpActivity::dumpSelectedDevice() {
  if (devices.empty()) {
    statusMessage = "No EEPROM selected";
    return;
  }

  const uint8_t deviceAddr = devices[selectionIndex];
  dumpBytes.assign(dumpLen, 0xFF);
  statusMessage = "Reading EEPROM...";
  render();

  for (uint16_t i = 0; i < dumpLen; i++) {
    uint8_t value = 0xFF;
    if (!readByte(deviceAddr, static_cast<uint16_t>(startAddr + i), value)) {
      char msg[48];
      snprintf(msg, sizeof(msg), "Read error at 0x%04X", static_cast<unsigned>(startAddr + i));
      statusMessage = msg;
      dumpBytes.resize(i);
      break;
    }
    dumpBytes[i] = value;
  }

  if (dumpBytes.size() == dumpLen) {
    statusMessage = "Dump complete";
  }
  viewOffset = 0;
  screen = Screen::DumpView;
  needsRender = true;
}

void I2cEepromDumpActivity::promptStartAddress() {
  char initial[10];
  snprintf(initial, sizeof(initial), "0x%04X", startAddr);
  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, "Start Address", initial, 6, 0, false,
      [this](const std::string& value) {
        std::string v = value;
        if (v.rfind("0x", 0) == 0 || v.rfind("0X", 0) == 0) {
          v = v.substr(2);
        }
        char* end = nullptr;
        const long n = strtol(v.c_str(), &end, 16);
        if (end && *end == '\0' && n >= 0 && n <= 0xFFFF) {
          startAddr = static_cast<uint16_t>(n);
          statusMessage = "Start address updated";
        } else {
          statusMessage = "Invalid start address";
        }
        exitActivity();
        needsRender = true;
      },
      [this]() {
        exitActivity();
        needsRender = true;
      }));
}

void I2cEepromDumpActivity::promptLength() {
  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, "Dump Length", std::to_string(dumpLen), 4, 0, false,
      [this](const std::string& value) {
        char* end = nullptr;
        const long n = strtol(value.c_str(), &end, 10);
        if (end && *end == '\0' && n >= 16 && n <= 512) {
          dumpLen = static_cast<uint16_t>(n);
          statusMessage = "Length updated";
        } else {
          statusMessage = "Length must be 16-512";
        }
        exitActivity();
        needsRender = true;
      },
      [this]() {
        exitActivity();
        needsRender = true;
      }));
}

void I2cEepromDumpActivity::loop() {
  if (subActivity) {
    ActivityWithSubactivity::loop();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (screen == Screen::DumpView) {
      screen = Screen::SelectDevice;
      needsRender = true;
      return;
    }
    if (onExitCb) {
      onExitCb();
    }
    return;
  }

  if (screen == Screen::SelectDevice) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
      scanDevices();
      needsRender = true;
    }
    if (!devices.empty()) {
      if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
        selectionIndex = (selectionIndex - 1 + static_cast<int>(devices.size())) % static_cast<int>(devices.size());
        needsRender = true;
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
        selectionIndex = (selectionIndex + 1) % static_cast<int>(devices.size());
        needsRender = true;
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
        use16bitAddress = !use16bitAddress;
        statusMessage = use16bitAddress ? "Address mode: 16-bit" : "Address mode: 8-bit";
        needsRender = true;
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
        dumpSelectedDevice();
      }
    }
  } else {
    if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
      viewOffset = std::max(0, viewOffset - kBytesPerRow * kRowsPerPage);
      needsRender = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      const int maxStart = std::max(0, static_cast<int>(dumpBytes.size()) - kBytesPerRow);
      viewOffset = std::min(maxStart, viewOffset + kBytesPerRow * kRowsPerPage);
      needsRender = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
      promptStartAddress();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      promptLength();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      dumpSelectedDevice();
      return;
    }
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void I2cEepromDumpActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  renderer.drawCenteredText(UI_12_FONT_ID, 16, "EEPROM Dump (Read-Only)");
  if (screen == Screen::SelectDevice) {
    renderDeviceSelect();
  } else {
    renderDumpView();
  }
  renderer.displayBuffer();
}

void I2cEepromDumpActivity::renderDeviceSelect() {
  if (!statusMessage.empty()) {
    renderer.drawCenteredText(SMALL_FONT_ID, 44, statusMessage.c_str());
  }
  if (devices.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "No EEPROM devices");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                              "Left: Rescan   Back: Menu");
    return;
  }

  int y = 76;
  for (size_t i = 0; i < devices.size(); i++) {
    char line[48];
    snprintf(line, sizeof(line), "0x%02X", devices[i]);
    if (static_cast<int>(i) == selectionIndex) {
      renderer.fillRect(40, y - 6, renderer.getScreenWidth() - 80, 22, true);
      renderer.drawText(UI_12_FONT_ID, 50, y, line, false);
    } else {
      renderer.drawText(UI_12_FONT_ID, 50, y, line);
    }
    y += 24;
    if (y > renderer.getScreenHeight() - 56) break;
  }

  char cfg[72];
  snprintf(cfg, sizeof(cfg), "Mode: %s  Start:0x%04X  Len:%u", use16bitAddress ? "16-bit" : "8-bit", startAddr,
           dumpLen);
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 44, cfg);
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                            "Up/Down: Select   Confirm: Dump   Right: AddrMode   Left: Rescan");
}

void I2cEepromDumpActivity::renderDumpView() {
  if (!devices.empty()) {
    char header[96];
    snprintf(header, sizeof(header), "Device 0x%02X  Start 0x%04X  Len %u", devices[selectionIndex], startAddr,
             dumpLen);
    renderer.drawText(SMALL_FONT_ID, 20, 40, header);
  }
  if (!statusMessage.empty()) {
    renderer.drawText(SMALL_FONT_ID, 20, 56, statusMessage.c_str());
  }

  if (dumpBytes.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "No dump data");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Confirm: Retry   Back: Devices");
    return;
  }

  int y = 82;
  for (int row = 0; row < kRowsPerPage; row++) {
    const int idx = viewOffset + row * kBytesPerRow;
    if (idx >= static_cast<int>(dumpBytes.size())) break;

    char line[96];
    int p = snprintf(line, sizeof(line), "%04X:", static_cast<unsigned>(startAddr + idx));
    for (int i = 0; i < kBytesPerRow; i++) {
      const int pos = idx + i;
      if (pos >= static_cast<int>(dumpBytes.size())) break;
      p += snprintf(line + p, sizeof(line) - p, " %02X", dumpBytes[pos]);
    }
    renderer.drawText(UI_10_FONT_ID, 20, y, line);
    y += 20;
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                            "Left/Right: Page   Confirm: Redump   Up: Start   Down: Len   Back: Devices");
}
