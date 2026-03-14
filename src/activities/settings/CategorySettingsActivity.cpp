#include "CategorySettingsActivity.h"

#include <GfxRenderer.h>
#include <HardwareSerial.h>

#include <cstring>

#include "CalibreSettingsActivity.h"
#include "ClearCacheActivity.h"
#include "CrossPointSettings.h"
#include "KOReaderSettingsActivity.h"
#include "MappedInputManager.h"
#include "OtaUpdateActivity.h"
#include "../apps/PaperS3Ui.h"
#include "fontIds.h"

namespace {
#if defined(PLATFORM_M5PAPERS3)
PaperS3Ui::Rect settingRowRect(const GfxRenderer& renderer, const int index) {
  constexpr int outerMargin = 24;
  constexpr int topY = 120;
  constexpr int rowHeight = 68;
  constexpr int rowGap = 12;
  PaperS3Ui::Rect rect;
  rect.x = outerMargin;
  rect.y = topY + index * (rowHeight + rowGap);
  rect.width = renderer.getScreenWidth() - outerMargin * 2;
  rect.height = rowHeight;
  return rect;
}
#endif
}

void CategorySettingsActivity::taskTrampoline(void* param) {
  auto* self = static_cast<CategorySettingsActivity*>(param);
  self->displayTaskLoop();
}

void CategorySettingsActivity::onEnter() {
  Activity::onEnter();
  renderingMutex = xSemaphoreCreateMutex();

  selectedSettingIndex = 0;
  updateRequired = true;

  xTaskCreate(&CategorySettingsActivity::taskTrampoline, "CategorySettingsActivityTask", 4096, this, 1,
              &displayTaskHandle);
}

void CategorySettingsActivity::onExit() {
  ActivityWithSubactivity::onExit();

  // Wait until not rendering to delete task to avoid killing mid-instruction to EPD
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

void CategorySettingsActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }

#if defined(PLATFORM_M5PAPERS3)
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasTapped() && PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), tapX, tapY)) {
    if (PaperS3Ui::backButtonRect(renderer).contains(tapX, tapY)) {
      SETTINGS.saveToFile();
      onGoBack();
      return;
    }

    for (int i = 0; i < settingsCount; i++) {
      if (!settingRowRect(renderer, i).contains(tapX, tapY)) {
        continue;
      }
      selectedSettingIndex = i;
      toggleCurrentSetting();
      updateRequired = true;
      return;
    }
  }
#endif

  // Handle actions with early return
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    toggleCurrentSetting();
    updateRequired = true;
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    SETTINGS.saveToFile();
    onGoBack();
    return;
  }

  // Handle navigation
  if (mappedInput.wasPressed(MappedInputManager::Button::Up) ||
      mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    selectedSettingIndex = (selectedSettingIndex > 0) ? (selectedSettingIndex - 1) : (settingsCount - 1);
    updateRequired = true;
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down) ||
             mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    selectedSettingIndex = (selectedSettingIndex < settingsCount - 1) ? (selectedSettingIndex + 1) : 0;
    updateRequired = true;
  }
}

void CategorySettingsActivity::toggleCurrentSetting() {
  if (selectedSettingIndex < 0 || selectedSettingIndex >= settingsCount) {
    return;
  }

  const auto& setting = settingsList[selectedSettingIndex];

  if (setting.type == SettingType::TOGGLE && setting.valuePtr != nullptr) {
    // Toggle the boolean value using the member pointer
    const bool currentValue = SETTINGS.*(setting.valuePtr);
    SETTINGS.*(setting.valuePtr) = !currentValue;
  } else if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
    const uint8_t currentValue = SETTINGS.*(setting.valuePtr);
    SETTINGS.*(setting.valuePtr) = (currentValue + 1) % static_cast<uint8_t>(setting.enumValues.size());
  } else if (setting.type == SettingType::VALUE && setting.valuePtr != nullptr) {
    const int8_t currentValue = SETTINGS.*(setting.valuePtr);
    if (currentValue + setting.valueRange.step > setting.valueRange.max) {
      SETTINGS.*(setting.valuePtr) = setting.valueRange.min;
    } else {
      SETTINGS.*(setting.valuePtr) = currentValue + setting.valueRange.step;
    }
  } else if (setting.type == SettingType::ACTION) {
    if (strcmp(setting.name, "Touchscreen Navigation") == 0 || strcmp(setting.name, "Built-in Fonts Enabled") == 0 ||
        strcmp(setting.name, "Reading Orientation (Portrait only)") == 0) {
      return;
    } else if (strcmp(setting.name, "KOReader Sync") == 0) {
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      exitActivity();
      enterNewActivity(new KOReaderSettingsActivity(renderer, mappedInput, [this] {
        exitActivity();
        updateRequired = true;
      }));
      xSemaphoreGive(renderingMutex);
    } else if (strcmp(setting.name, "OPDS Browser") == 0) {
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      exitActivity();
      enterNewActivity(new CalibreSettingsActivity(renderer, mappedInput, [this] {
        exitActivity();
        updateRequired = true;
      }));
      xSemaphoreGive(renderingMutex);
    } else if (strcmp(setting.name, "Clear Cache") == 0) {
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      exitActivity();
      enterNewActivity(new ClearCacheActivity(renderer, mappedInput, [this] {
        exitActivity();
        updateRequired = true;
      }));
      xSemaphoreGive(renderingMutex);
    } else if (strcmp(setting.name, "Check for updates") == 0) {
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      exitActivity();
      enterNewActivity(new OtaUpdateActivity(renderer, mappedInput, [this] {
        exitActivity();
        updateRequired = true;
      }));
      xSemaphoreGive(renderingMutex);
    }
  } else {
    return;
  }

  SETTINGS.saveToFile();
}

void CategorySettingsActivity::displayTaskLoop() {
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

void CategorySettingsActivity::render() const {
  renderer.clearScreen();
#if defined(PLATFORM_M5PAPERS3)
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
#endif

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 22, categoryName, true, EpdFontFamily::BOLD);

#if defined(PLATFORM_M5PAPERS3)
  PaperS3Ui::drawBackButton(renderer);

  for (int i = 0; i < settingsCount; i++) {
    const auto rect = settingRowRect(renderer, i);
    const bool isSelected = (i == selectedSettingIndex);

    renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
    renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
    if (isSelected) {
      renderer.fillRect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2, true);
    }

    renderer.drawText(UI_12_FONT_ID, rect.x + 18, rect.y + 18, settingsList[i].name, !isSelected, EpdFontFamily::BOLD);

    std::string valueText;
    if (settingsList[i].type == SettingType::TOGGLE && settingsList[i].valuePtr != nullptr) {
      valueText = (SETTINGS.*(settingsList[i].valuePtr)) ? "ON" : "OFF";
    } else if (settingsList[i].type == SettingType::ENUM && settingsList[i].valuePtr != nullptr) {
      valueText = settingsList[i].enumValues[SETTINGS.*(settingsList[i].valuePtr)];
    } else if (settingsList[i].type == SettingType::VALUE && settingsList[i].valuePtr != nullptr) {
      valueText = std::to_string(SETTINGS.*(settingsList[i].valuePtr));
    } else if (settingsList[i].type == SettingType::ACTION) {
      if (strcmp(settingsList[i].name, "Touchscreen Navigation") == 0 ||
          strcmp(settingsList[i].name, "Built-in Fonts Enabled") == 0) {
        valueText = "Enabled";
      } else if (strcmp(settingsList[i].name, "Reading Orientation (Portrait only)") == 0) {
        valueText = "Locked";
      } else {
        valueText = "Run";
      }
    }

    if (!valueText.empty()) {
      const int width = renderer.getTextWidth(UI_10_FONT_ID, valueText.c_str());
      renderer.drawText(UI_10_FONT_ID, rect.x + rect.width - width - 18, rect.y + 22, valueText.c_str(), !isSelected);
    }
  }

  renderer.drawText(UI_10_FONT_ID, pageWidth - 24 - renderer.getTextWidth(UI_10_FONT_ID, CROSSPOINT_VERSION),
                    pageHeight - 124, CROSSPOINT_VERSION);
  PaperS3Ui::drawFooter(renderer, "Tap a row to change it");
  renderer.displayBuffer();
  return;
#endif

  // Draw selection highlight
  renderer.fillRect(0, 60 + selectedSettingIndex * 30 - 2, pageWidth - 1, 30);

  // Draw all settings
  for (int i = 0; i < settingsCount; i++) {
    const int settingY = 60 + i * 30;  // 30 pixels between settings
    const bool isSelected = (i == selectedSettingIndex);

    // Draw setting name
    renderer.drawText(UI_10_FONT_ID, 20, settingY, settingsList[i].name, !isSelected);

    // Draw value based on setting type
    std::string valueText;
    if (settingsList[i].type == SettingType::TOGGLE && settingsList[i].valuePtr != nullptr) {
      const bool value = SETTINGS.*(settingsList[i].valuePtr);
      valueText = value ? "ON" : "OFF";
    } else if (settingsList[i].type == SettingType::ENUM && settingsList[i].valuePtr != nullptr) {
      const uint8_t value = SETTINGS.*(settingsList[i].valuePtr);
      valueText = settingsList[i].enumValues[value];
    } else if (settingsList[i].type == SettingType::VALUE && settingsList[i].valuePtr != nullptr) {
      valueText = std::to_string(SETTINGS.*(settingsList[i].valuePtr));
    }
    if (!valueText.empty()) {
      const auto width = renderer.getTextWidth(UI_10_FONT_ID, valueText.c_str());
      renderer.drawText(UI_10_FONT_ID, pageWidth - 20 - width, settingY, valueText.c_str(), !isSelected);
    }
  }

  renderer.drawText(SMALL_FONT_ID, pageWidth - 20 - renderer.getTextWidth(SMALL_FONT_ID, CROSSPOINT_VERSION),
                    pageHeight - 60, CROSSPOINT_VERSION);

  const auto labels = mappedInput.mapLabels("« Back", "Toggle", "", "");
  renderer.drawButtonHints(UI_10_FONT_ID, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
