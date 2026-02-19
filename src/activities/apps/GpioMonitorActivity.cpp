#include "GpioMonitorActivity.h"

// Source reference:
// - Diagnostics source project: https://github.com/geo-tp/ESP32-Bus-Pirate
// - Local implementation for OmniPaper is maintained in this file.

#include <algorithm>
#include <cstdlib>

#include <Arduino.h>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"

namespace {
constexpr int kMaxPins = 8;

const int kPortB[] = {33, 26};
const int kPortC[] = {19, 18};
const int kExtI2C[] = {32, 25};
}  // namespace

GpioMonitorActivity::GpioMonitorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                         const std::function<void()>& onExit)
    : ActivityWithSubactivity("GpioMonitor", renderer, mappedInput), onExitCb(onExit) {}

void GpioMonitorActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
  statusMessage.clear();
  setProfile(profile);
  lastPollMs = 0;
  needsRender = true;
}

void GpioMonitorActivity::setProfile(Profile next) {
  profile = next;
  pins.clear();
  switch (profile) {
    case Profile::PortB:
      pins.assign(std::begin(kPortB), std::end(kPortB));
      break;
    case Profile::PortC:
      pins.assign(std::begin(kPortC), std::end(kPortC));
      break;
    case Profile::ExternalI2C:
      pins.assign(std::begin(kExtI2C), std::end(kExtI2C));
      break;
    case Profile::Custom:
      break;
  }
  applyPins();
  values.assign(pins.size(), 0);
  lastValues.assign(pins.size(), -1);
  needsRender = true;
}

void GpioMonitorActivity::applyPins() {
  for (int pin : pins) {
    if (pin < 0) continue;
    pinMode(pin, pullupEnabled ? INPUT_PULLUP : INPUT);
  }
}

void GpioMonitorActivity::updateValues() {
  if (pins.empty()) return;
  bool changed = false;
  for (size_t i = 0; i < pins.size(); i++) {
    const int val = digitalRead(pins[i]);
    values[i] = val;
    if (lastValues[i] != val) {
      changed = true;
    }
  }
  if (changed) {
    lastValues = values;
    needsRender = true;
  }
}

void GpioMonitorActivity::loop() {
  if (subActivity) {
    ActivityWithSubactivity::loop();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExitCb) {
      onExitCb();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    const int idx = (static_cast<int>(profile) - 1 + 4) % 4;
    setProfile(static_cast<Profile>(idx));
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    const int idx = (static_cast<int>(profile) + 1) % 4;
    setProfile(static_cast<Profile>(idx));
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    pullupEnabled = !pullupEnabled;
    applyPins();
    needsRender = true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (profile != Profile::Custom) {
      setProfile(Profile::Custom);
    }
    promptCustomPins();
    return;
  }

  const unsigned long now = millis();
  if (now - lastPollMs > pollIntervalMs) {
    lastPollMs = now;
    updateValues();
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void GpioMonitorActivity::promptCustomPins() {
  std::string initial;
  for (size_t i = 0; i < pins.size(); i++) {
    if (i > 0) initial += ",";
    initial += std::to_string(pins[i]);
  }

  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, "Pins (e.g. 33,26,19)", initial, 24, 0, false,
      [this](const std::string& value) {
        std::vector<int> parsed;
        if (parsePins(value, parsed)) {
          pins = parsed;
          statusMessage.clear();
          applyPins();
          values.assign(pins.size(), 0);
          lastValues.assign(pins.size(), -1);
        } else {
          statusMessage = "No valid pins";
        }
        exitActivity();
        needsRender = true;
      },
      [this]() {
        exitActivity();
        needsRender = true;
      }));
}

bool GpioMonitorActivity::parsePins(const std::string& text, std::vector<int>& out) const {
  out.clear();
  std::string token;
  auto flushToken = [&]() {
    if (token.empty()) return;
    char* end = nullptr;
    long value = 0;
    if (token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0) {
      value = strtol(token.c_str(), &end, 16);
    } else {
      value = strtol(token.c_str(), &end, 10);
    }
    if (end && *end == '\0' && value >= 0 && value <= 39) {
      if (std::find(out.begin(), out.end(), static_cast<int>(value)) == out.end()) {
        out.push_back(static_cast<int>(value));
      }
    }
    token.clear();
  };

  for (char c : text) {
    if (c == ',' || c == ' ' || c == ';') {
      flushToken();
    } else {
      token += c;
    }
  }
  flushToken();

  if (out.size() > kMaxPins) {
    out.resize(kMaxPins);
  }
  return !out.empty();
}

const char* GpioMonitorActivity::profileLabel(Profile p) const {
  switch (p) {
    case Profile::PortB:
      return "Port B (33,26)";
    case Profile::PortC:
      return "Port C (19,18)";
    case Profile::ExternalI2C:
      return "EXT I2C (32,25)";
    case Profile::Custom:
      return "Custom";
    default:
      return "Unknown";
  }
}

void GpioMonitorActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  renderer.drawCenteredText(UI_12_FONT_ID, 16, "GPIO Monitor");

  renderer.drawCenteredText(SMALL_FONT_ID, 44, profileLabel(profile));
  renderer.drawCenteredText(SMALL_FONT_ID, 64, pullupEnabled ? "Pullup: ON" : "Pullup: OFF");

  if (!statusMessage.empty()) {
    renderer.drawCenteredText(SMALL_FONT_ID, 84, statusMessage.c_str());
  }

  if (pins.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "No pins configured");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                              "Confirm: Set Pins   Back: Menu");
    renderer.displayBuffer();
    return;
  }

  int y = 110;
  for (size_t i = 0; i < pins.size(); i++) {
    char line[48];
    snprintf(line, sizeof(line), "GPIO%02d : %s", pins[i], values[i] ? "HIGH" : "LOW");
    renderer.drawText(UI_12_FONT_ID, 40, y, line);
    y += 26;
    if (y > renderer.getScreenHeight() - 50) break;
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                            "Left/Right: Profile   Up: Pullup   Confirm: Pins   Back: Menu");
  renderer.displayBuffer();
}
