#include "TimeSettingsActivity.h"

#include <WiFi.h>

#include <string>

#include <GfxRenderer.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "fontIds.h"
#include "util/TimeUtils.h"

namespace {
constexpr int kMaxItems = 3;

std::string formatOffset(const int16_t minutes) {
  const int total = minutes;
  const int sign = (total < 0) ? -1 : 1;
  const int absMin = total * sign;
  const int hours = absMin / 60;
  const int mins = absMin % 60;
  char buf[16];
  snprintf(buf, sizeof(buf), "UTC%c%02d:%02d", sign < 0 ? '-' : '+', hours, mins);
  return std::string(buf);
}
}  // namespace

TimeSettingsActivity::TimeSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           const std::function<void()>& onExit)
    : ActivityWithSubactivity("TimeSettings", renderer, mappedInput), onExit(onExit) {}

void TimeSettingsActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
  selectionIndex = 0;
  mode = Mode::Idle;
  statusMessage.clear();
  needsRender = true;
}

void TimeSettingsActivity::loop() {
  if (subActivity) {
    ActivityWithSubactivity::loop();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExit) {
      onExit();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selectionIndex = (selectionIndex - 1 + kMaxItems) % kMaxItems;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selectionIndex = (selectionIndex + 1) % kMaxItems;
    needsRender = true;
  }

  if (selectionIndex == 1) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
      int16_t offset = SETTINGS.timezoneOffsetMinutes;
      offset -= 30;
      if (offset < -720) offset = -720;
      SETTINGS.timezoneOffsetMinutes = offset;
      SETTINGS.saveToFile();
      needsRender = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      int16_t offset = SETTINGS.timezoneOffsetMinutes;
      offset += 30;
      if (offset > 840) offset = 840;
      SETTINGS.timezoneOffsetMinutes = offset;
      SETTINGS.saveToFile();
      needsRender = true;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (selectionIndex == 0) {
      if (WiFi.status() == WL_CONNECTED) {
        performSync();
      } else {
        launchWifiSelection();
      }
    } else if (selectionIndex == 2) {
      if (onExit) {
        onExit();
      }
      return;
    }
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void TimeSettingsActivity::launchWifiSelection() {
  enterNewActivity(new WifiSelectionActivity(renderer, mappedInput,
                                             [this](const bool connected) {
                                               exitActivity();
                                               if (connected) {
                                                 performSync();
                                               } else {
                                                 statusMessage = "WiFi connection failed";
                                                 needsRender = true;
                                               }
                                             }));
}

void TimeSettingsActivity::performSync() {
  mode = Mode::Syncing;
  statusMessage = "Syncing time...";
  needsRender = true;

  const bool ok = TimeUtils::syncTimeWithNtp();
  mode = Mode::Result;
  statusMessage = ok ? "NTP sync OK" : "NTP sync failed";
  needsRender = true;
}

void TimeSettingsActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "Time Settings");

  std::tm localTime {};
  if (TimeUtils::getLocalTimeWithOffset(localTime, SETTINGS.timezoneOffsetMinutes)) {
    char timeLine[32];
    snprintf(timeLine, sizeof(timeLine), "%02d:%02d:%02d", localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    renderer.drawText(UI_12_FONT_ID, 40, 50, timeLine);
  } else {
    renderer.drawText(UI_12_FONT_ID, 40, 50, "Time not synced");
  }

  const int startY = 90;
  const char* items[kMaxItems] = {"Sync NTP", "Timezone", "Back"};

  for (int i = 0; i < kMaxItems; i++) {
    const int y = startY + i * 26;
    if (i == selectionIndex) {
      renderer.fillRect(30, y - 6, renderer.getScreenWidth() - 60, 24, true);
      renderer.drawText(UI_12_FONT_ID, 40, y, items[i], false);
    } else {
      renderer.drawText(UI_12_FONT_ID, 40, y, items[i]);
    }

    if (i == 1) {
      const std::string tz = formatOffset(SETTINGS.timezoneOffsetMinutes);
      const int width = renderer.getTextWidth(UI_10_FONT_ID, tz.c_str());
      renderer.drawText(UI_10_FONT_ID, renderer.getScreenWidth() - width - 40, y + 2, tz.c_str(), i != selectionIndex);
    }
  }

  if (!statusMessage.empty()) {
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 44, statusMessage.c_str());
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                            "Up/Down: Select   Left/Right: TZ   Confirm: Action   Back: Menu");
  renderer.displayBuffer();
}
