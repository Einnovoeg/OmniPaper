#include "SettingsActivity.h"

#include <GfxRenderer.h>
#include <HardwareSerial.h>

#include "../apps/PaperS3Ui.h"
#include "CategorySettingsActivity.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "fontIds.h"

const char* SettingsActivity::categoryNames[categoryCount] = {"Display", "Reader", "Device", "System"};

namespace {
constexpr int displaySettingsCount = 7;
const SettingInfo displaySettings[displaySettingsCount] = {
    // Should match with SLEEP_SCREEN_MODE
    SettingInfo::Enum("Sleep Screen", &CrossPointSettings::sleepScreen, {"Dark", "Light", "Custom", "Cover", "None"}),
    SettingInfo::Enum("Sleep Screen Cover Mode", &CrossPointSettings::sleepScreenCoverMode, {"Fit", "Crop"}),
    SettingInfo::Enum("Sleep Screen Cover Filter", &CrossPointSettings::sleepScreenCoverFilter,
                      {"None", "Contrast", "Inverted"}),
    SettingInfo::Enum("Status Bar", &CrossPointSettings::statusBar,
                      {"None", "No Progress", "Full w/ Percentage", "Full w/ Progress Bar", "Progress Bar"}),
    SettingInfo::Enum("Info Overlay", &CrossPointSettings::infoOverlayPosition,
                      {"Off", "Top", "Bottom", "Top Corners", "Bottom Corners"}),
    SettingInfo::Enum("Hide Battery %", &CrossPointSettings::hideBatteryPercentage, {"Never", "In Reader", "Always"}),
    SettingInfo::Enum("Refresh Frequency", &CrossPointSettings::refreshFrequency,
                      {"1 page", "5 pages", "10 pages", "15 pages", "30 pages"})};

constexpr int readerSettingsCount = 9;
const SettingInfo readerSettings[readerSettingsCount] = {
#if defined(PLATFORM_M5PAPERS3)
    SettingInfo::Enum("Font Family", &CrossPointSettings::fontFamily, {"Bookerly", "Noto Sans", "Open Dyslexic"}),
#else
    SettingInfo::Enum("Font Family", &CrossPointSettings::fontFamily,
                      {"Bookerly", "Noto Sans", "Open Dyslexic", "Custom (SD)"}),
#endif
    SettingInfo::Enum("Font Size", &CrossPointSettings::fontSize, {"Small", "Medium", "Large", "X Large"}),
    SettingInfo::Enum("Line Spacing", &CrossPointSettings::lineSpacing, {"Tight", "Normal", "Wide"}),
    SettingInfo::Value("Screen Margin", &CrossPointSettings::screenMargin, {5, 40, 5}),
    SettingInfo::Enum("Paragraph Alignment", &CrossPointSettings::paragraphAlignment,
                      {"Justify", "Left", "Center", "Right"}),
    SettingInfo::Toggle("Hyphenation", &CrossPointSettings::hyphenationEnabled),
#if defined(PLATFORM_M5PAPERS3)
    SettingInfo::Action("Reading Orientation (Portrait only)"),
#else
    SettingInfo::Enum("Reading Orientation", &CrossPointSettings::orientation,
                      {"Portrait", "Landscape CW", "Inverted", "Landscape CCW"}),
#endif
    SettingInfo::Toggle("Extra Paragraph Spacing", &CrossPointSettings::extraParagraphSpacing),
    SettingInfo::Toggle("Text Anti-Aliasing", &CrossPointSettings::textAntiAliasing)};

constexpr int controlsSettingsCount = 4;
const SettingInfo controlsSettings[controlsSettingsCount] = {
#if defined(PLATFORM_M5PAPERS3)
    SettingInfo::Action("Touchscreen Navigation"), SettingInfo::Action("Built-in Fonts Enabled"),
    SettingInfo::Toggle("Long-press Chapter Skip", &CrossPointSettings::longPressChapterSkip),
    SettingInfo::Enum("Short Power Button Click", &CrossPointSettings::shortPwrBtn, {"Ignore", "Sleep", "Page Turn"})};
#else
    SettingInfo::Enum(
        "Front Button Layout", &CrossPointSettings::frontButtonLayout,
        {"Bck, Cnfrm, Lft, Rght", "Lft, Rght, Bck, Cnfrm", "Lft, Bck, Cnfrm, Rght", "Bck, Cnfrm, Rght, Lft"}),
    SettingInfo::Enum("Side Button Layout (reader)", &CrossPointSettings::sideButtonLayout,
                      {"Prev, Next", "Next, Prev"}),
    SettingInfo::Toggle("Long-press Chapter Skip", &CrossPointSettings::longPressChapterSkip),
    SettingInfo::Enum("Short Power Button Click", &CrossPointSettings::shortPwrBtn, {"Ignore", "Sleep", "Page Turn"})};
#endif

constexpr int systemSettingsCount = 6;
const SettingInfo systemSettings[systemSettingsCount] = {
    SettingInfo::Toggle("Idle Hotspot Web UI", &CrossPointSettings::idleHotspotWebUi),
    SettingInfo::Enum("Time to Sleep", &CrossPointSettings::sleepTimeout,
                      {"1 min", "5 min", "10 min", "15 min", "30 min"}),
    SettingInfo::Action("KOReader Sync"),
    SettingInfo::Action("OPDS Browser"),
    SettingInfo::Action("Clear Cache"),
    SettingInfo::Action("Check for updates")};

#if defined(PLATFORM_M5PAPERS3)
PaperS3Ui::Rect categoryRowRect(const GfxRenderer& renderer, const int index) {
  return {PaperS3Ui::kOuterMargin,
          PaperS3Ui::kListTopY + index * (PaperS3Ui::kListRowHeight + PaperS3Ui::kListRowGap),
          renderer.getScreenWidth() - PaperS3Ui::kOuterMargin * 2, PaperS3Ui::kListRowHeight};
}
#endif
}  // namespace

void SettingsActivity::taskTrampoline(void* param) {
  auto* self = static_cast<SettingsActivity*>(param);
  self->displayTaskLoop();
}

void SettingsActivity::onEnter() {
  Activity::onEnter();
  renderingMutex = xSemaphoreCreateMutex();

  // Reset selection to first category
  selectedCategoryIndex = 0;

  // Trigger first update
  updateRequired = true;

#if !defined(PLATFORM_M5PAPERS3)
  xTaskCreate(&SettingsActivity::taskTrampoline, "SettingsActivityTask",
              4096,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
#endif
}

void SettingsActivity::onExit() {
  ActivityWithSubactivity::onExit();

  // Wait until not rendering to delete task to avoid killing mid-instruction to EPD
  if (renderingMutex) {
    xSemaphoreTake(renderingMutex, portMAX_DELAY);
    if (displayTaskHandle) {
      vTaskDelete(displayTaskHandle);
      displayTaskHandle = nullptr;
    }
    vSemaphoreDelete(renderingMutex);
    renderingMutex = nullptr;
  }
}

void SettingsActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }

#if defined(PLATFORM_M5PAPERS3)
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasTapped() &&
      PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), tapX, tapY)) {
    if (PaperS3Ui::backButtonRect(renderer).contains(tapX, tapY)) {
      SETTINGS.saveToFile();
      onGoHome();
      return;
    }

    for (int i = 0; i < categoryCount; i++) {
      if (!categoryRowRect(renderer, i).contains(tapX, tapY)) {
        continue;
      }
      selectedCategoryIndex = i;
      updateRequired = true;
      enterCategory(i);
      return;
    }
  }
#endif

  // Handle category selection
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    enterCategory(selectedCategoryIndex);
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    SETTINGS.saveToFile();
    onGoHome();
    return;
  }

  // Handle navigation
  if (mappedInput.wasPressed(MappedInputManager::Button::Up) ||
      mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    // Move selection up (with wrap-around)
    selectedCategoryIndex = (selectedCategoryIndex > 0) ? (selectedCategoryIndex - 1) : (categoryCount - 1);
    updateRequired = true;
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down) ||
             mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    // Move selection down (with wrap around)
    selectedCategoryIndex = (selectedCategoryIndex < categoryCount - 1) ? (selectedCategoryIndex + 1) : 0;
    updateRequired = true;
  }

#if defined(PLATFORM_M5PAPERS3)
  if (updateRequired && !subActivity && renderingMutex) {
    updateRequired = false;
    xSemaphoreTake(renderingMutex, portMAX_DELAY);
    render();
    xSemaphoreGive(renderingMutex);
  }
#endif
}

void SettingsActivity::enterCategory(int categoryIndex) {
  if (categoryIndex < 0 || categoryIndex >= categoryCount) {
    return;
  }

  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  exitActivity();

  const SettingInfo* settingsList = nullptr;
  int settingsCount = 0;

  switch (categoryIndex) {
    case 0:  // Display
      settingsList = displaySettings;
      settingsCount = displaySettingsCount;
      break;
    case 1:  // Reader
      settingsList = readerSettings;
      settingsCount = readerSettingsCount;
      break;
    case 2:  // Controls
      settingsList = controlsSettings;
      settingsCount = controlsSettingsCount;
      break;
    case 3:  // System
      settingsList = systemSettings;
      settingsCount = systemSettingsCount;
      break;
  }

  enterNewActivity(new CategorySettingsActivity(renderer, mappedInput, categoryNames[categoryIndex], settingsList,
                                                settingsCount, [this] {
                                                  exitActivity();
                                                  updateRequired = true;
                                                }));
  xSemaphoreGive(renderingMutex);
}

void SettingsActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired && !subActivity) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      render();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void SettingsActivity::render() const {
  renderer.clearScreen();
#if defined(PLATFORM_M5PAPERS3)
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
#endif

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

#if defined(PLATFORM_M5PAPERS3)
  PaperS3Ui::drawScreenHeader(renderer, "Settings", "Display, reader, controls, and system");
  PaperS3Ui::drawBackButton(renderer);

  for (int i = 0; i < categoryCount; i++) {
    const auto rect = categoryRowRect(renderer, i);
    const bool selected = (i == selectedCategoryIndex);
    renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
    renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
    if (selected) {
      renderer.fillRect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2, true);
    }
    renderer.drawText(UI_12_FONT_ID, rect.x + 18, rect.y + 20, categoryNames[i], !selected, EpdFontFamily::BOLD);
  }

  renderer.drawText(UI_10_FONT_ID, pageWidth - 24 - renderer.getTextWidth(UI_10_FONT_ID, CROSSPOINT_VERSION),
                    pageHeight - 124, CROSSPOINT_VERSION);
  PaperS3Ui::drawFooter(renderer, "Tap a section to open");
  renderer.displayBuffer();
  return;
#endif

  // Draw header
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 22, "Settings", true, EpdFontFamily::BOLD);

  // Draw selection
  renderer.fillRect(0, 60 + selectedCategoryIndex * 30 - 2, pageWidth - 1, 30);

  // Draw all categories
  for (int i = 0; i < categoryCount; i++) {
    const int categoryY = 60 + i * 30;  // 30 pixels between categories

    // Draw category name
    renderer.drawText(UI_10_FONT_ID, 20, categoryY, categoryNames[i], i != selectedCategoryIndex);
  }

  // Draw version text above button hints
  renderer.drawText(SMALL_FONT_ID, pageWidth - 20 - renderer.getTextWidth(SMALL_FONT_ID, CROSSPOINT_VERSION),
                    pageHeight - 60, CROSSPOINT_VERSION);

  // Draw help text
  const auto labels = mappedInput.mapLabels("« Back", "Select", "", "");
  renderer.drawButtonHints(UI_10_FONT_ID, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  // Always use standard refresh for settings screen
  renderer.displayBuffer();
}
