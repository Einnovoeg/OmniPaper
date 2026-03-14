#include "OptionalDevicesActivity.h"

#include <SDCardManager.h>

#include <array>

#ifdef PLATFORM_M5PAPER
#include <M5Unified.h>
#endif

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "PaperS3Ui.h"

namespace {
constexpr uint8_t kSht30Address = 0x44;
constexpr uint8_t kAs7331Address = 0x74;
constexpr const char* kUvLogFile = "/logs/uv.csv";
}  // namespace

void OptionalDevicesActivity::onEnter() {
  Activity::onEnter();
  refreshRows();
  needsRender = true;
}

void OptionalDevicesActivity::refreshRows() {
  rows.clear();

#ifdef PLATFORM_M5PAPER
  std::array<bool, 120> found{};
  found.fill(false);

  if (M5.Ex_I2C.begin()) {
    M5.Ex_I2C.scanID(found.data(), 100000);
  }

  if (kSht30Address < found.size() && found[kSht30Address]) {
    rows.push_back({"Environmental Sensor", "Detected on the external I2C bus", DeviceAction::ExternalSensors});
  }

  if (kAs7331Address < found.size() && found[kAs7331Address]) {
    rows.push_back({"UV Sensor (AS7331)", "Detected on the external I2C bus", DeviceAction::UvSensor});
  }
#endif

  if (SdMan.exists(kUvLogFile)) {
    rows.push_back({"UV Logs", "Saved measurements from a connected UV sensor", DeviceAction::UvLogs});
  }

  if (rows.empty()) {
    rows.push_back(
        {"No supported peripherals detected", "Please connect an external sensor, then refresh", DeviceAction::None});
  }

  selectedIndex = 0;
}

void OptionalDevicesActivity::loop() {
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
      refreshRows();
      needsRender = true;
      return;
    }

    for (int i = 0; i < static_cast<int>(rows.size()); i++) {
      if (!PaperS3Ui::listRowRect(renderer, i).contains(tapX, tapY)) {
        continue;
      }
      selectedIndex = i;
      needsRender = true;
      if (rows[i].action != DeviceAction::None && onAction) {
        onAction(rows[i].action);
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

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (!rows.empty() && rows[selectedIndex].action != DeviceAction::None && onAction) {
      onAction(rows[selectedIndex].action);
      return;
    }
    refreshRows();
    needsRender = true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Up) && !rows.empty()) {
    selectedIndex = (selectedIndex - 1 + static_cast<int>(rows.size())) % static_cast<int>(rows.size());
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down) && !rows.empty()) {
    selectedIndex = (selectedIndex + 1) % static_cast<int>(rows.size());
    needsRender = true;
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void OptionalDevicesActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  PaperS3Ui::drawScreenHeader(renderer, "Optional Devices", "Detected on the external bus");
  PaperS3Ui::drawBackButton(renderer);

  for (int i = 0; i < static_cast<int>(rows.size()); i++) {
    const auto rect = PaperS3Ui::listRowRect(renderer, i);
    const bool selected = (i == selectedIndex);
    PaperS3Ui::drawListRow(renderer, rect, selected, rows[i].title.c_str(), "", rows[i].subtitle.c_str());
  }

  PaperS3Ui::drawPrimaryActionButton(renderer, "Refresh");
  PaperS3Ui::drawFooter(renderer, "Connected peripherals are listed here");
  renderer.displayBuffer();
}
