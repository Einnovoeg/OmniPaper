#include "TimeSettingsActivity.h"

#include <WiFi.h>

#include <string>

#include <GfxRenderer.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "PaperS3Ui.h"
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

#if defined(PLATFORM_M5PAPERS3)
PaperS3Ui::Rect timeRowRect(const GfxRenderer& renderer, const int index) {
  constexpr int outerMargin = 24;
  constexpr int topY = 150;
  constexpr int rowHeight = 68;
  constexpr int rowGap = 14;
  PaperS3Ui::Rect rect;
  rect.x = outerMargin;
  rect.y = topY + index * (rowHeight + rowGap);
  rect.width = renderer.getScreenWidth() - outerMargin * 2;
  rect.height = rowHeight;
  return rect;
}
#endif
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

#if defined(PLATFORM_M5PAPERS3)
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasTapped() && PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), tapX, tapY)) {
    if (PaperS3Ui::backButtonRect(renderer).contains(tapX, tapY)) {
      if (onExit) {
        onExit();
      }
      return;
    }

    for (int i = 0; i < kMaxItems; i++) {
      if (!timeRowRect(renderer, i).contains(tapX, tapY)) {
        continue;
      }
      selectionIndex = i;
      needsRender = true;
      if (i == 0) {
        if (WiFi.status() == WL_CONNECTED) {
          performSync();
        } else {
          launchWifiSelection();
        }
      } else if (i == 1) {
        const auto rect = timeRowRect(renderer, 1);
        const int midpoint = rect.x + rect.width / 2;
        int16_t offset = SETTINGS.timezoneOffsetMinutes;
        offset += (tapX >= midpoint) ? 30 : -30;
        if (offset < -720) offset = -720;
        if (offset > 840) offset = 840;
        SETTINGS.timezoneOffsetMinutes = offset;
        SETTINGS.saveToFile();
      } else if (i == 2 && onExit) {
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
  PaperS3Ui::applyDefaultOrientation(renderer);
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 22, "Time Settings", true, EpdFontFamily::BOLD);

#if defined(PLATFORM_M5PAPERS3)
  PaperS3Ui::drawBackButton(renderer);
#endif

  std::tm localTime {};
  if (TimeUtils::getLocalTimeWithOffset(localTime, SETTINGS.timezoneOffsetMinutes)) {
    char timeLine[32];
    snprintf(timeLine, sizeof(timeLine), "%02d:%02d:%02d", localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 78, timeLine);
  } else {
    renderer.drawCenteredText(UI_12_FONT_ID, 82, "Time not synced");
  }

  const char* items[kMaxItems] = {"Sync NTP", "Timezone", "Back"};

#if defined(PLATFORM_M5PAPERS3)
  for (int i = 0; i < kMaxItems; i++) {
    const auto rect = timeRowRect(renderer, i);
    const bool selected = (i == selectionIndex);
    renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
    renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
    if (selected) {
      renderer.fillRect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2, true);
    }

    renderer.drawText(UI_12_FONT_ID, rect.x + 18, rect.y + 18, items[i], !selected, EpdFontFamily::BOLD);

    if (i == 1) {
      const std::string tz = formatOffset(SETTINGS.timezoneOffsetMinutes);
      const int width = renderer.getTextWidth(UI_10_FONT_ID, tz.c_str());
      renderer.drawText(UI_10_FONT_ID, rect.x + rect.width - width - 18, rect.y + 22, tz.c_str(), !selected);
    }
  }

  if (!statusMessage.empty()) {
    PaperS3Ui::drawFooterStatus(renderer, statusMessage.c_str());
  }
  PaperS3Ui::drawFooter(renderer, "Tap left/right half of Timezone to adjust");
  renderer.displayBuffer();
  return;
#endif

  const int startY = 90;
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
