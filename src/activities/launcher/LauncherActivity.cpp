#include "LauncherActivity.h"

#include <GfxRenderer.h>
#include <WiFi.h>

#include <algorithm>
#include <string>

#include "../apps/PaperS3Ui.h"
#include "util/TimeUtils.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "ScreenComponents.h"
#include "fontIds.h"

#ifdef PLATFORM_M5PAPER
#include <M5Unified.h>
#endif

namespace {
const LauncherActivity::MenuItem kAllItems[] = {
    {LauncherItemId::Reader, "Reader", false},    {LauncherItemId::Dashboard, "Dashboard", false},
    {LauncherItemId::Sensors, "Sensors", true},   {LauncherItemId::Weather, "Weather", false},
    {LauncherItemId::Network, "Network", true},   {LauncherItemId::Games, "Games", true},
    {LauncherItemId::Images, "Images", true},     {LauncherItemId::Tools, "Tools", true},
    {LauncherItemId::Settings, "Settings", true}, {LauncherItemId::Calculator, "Calculator", false},
};

std::vector<LauncherActivity::SubmenuItem> buildSensorsMenu() {
#if defined(PLATFORM_M5PAPERS3)
  return {
      {"Built-in", LauncherAction::SensorsBuiltIn},
      {"Optional Devices", LauncherAction::SensorsPeripherals},
      {"I2C Tools", LauncherAction::SensorsI2CTools},
  };
#else
  return {
      {"Built-in", LauncherAction::SensorsBuiltIn},   {"Please connect", LauncherAction::SensorsPeripherals},
      {"I2C Tools", LauncherAction::SensorsI2CTools}, {"GPIO Monitor", LauncherAction::SensorsGpio},
      {"UART RX", LauncherAction::SensorsUartRx},     {"EEPROM Dump", LauncherAction::SensorsEepromDump},
  };
#endif
}

std::vector<LauncherActivity::SubmenuItem> buildGamesMenu() {
  return {
      {"Poodle", LauncherAction::GamePoodle},
      {"Sudoku", LauncherAction::GameSudoku},
      {"Mines", LauncherAction::GameMinesweeper},
      {"Tetris", LauncherAction::GameTetris},
  };
}

std::vector<LauncherActivity::SubmenuItem> buildImagesMenu() {
  return {
      {"Viewer", LauncherAction::ImagesViewer},
      {"Drawing", LauncherAction::ImagesDraw},
  };
}

std::vector<LauncherActivity::SubmenuItem> buildNetworkMenu() {
  return {
      {"WiFi Status", LauncherAction::NetworkWifiStatus},     {"WiFi Scan", LauncherAction::NetworkWifiScan},
      {"WiFi Tests", LauncherAction::NetworkWifiTests},       {"Channels", LauncherAction::NetworkWifiChannels},
      {"BLE Scan", LauncherAction::NetworkBleScan},           {"Web Portal", LauncherAction::NetworkWebPortal},
      {"Host Keyboard", LauncherAction::NetworkKeyboardHost}, {"SSH Client", LauncherAction::NetworkSshClient},
  };
}

std::vector<LauncherActivity::SubmenuItem> buildToolsMenu() {
  return {
      {"Notes", LauncherAction::ToolsNotes},
      {"File Manager", LauncherAction::ToolsFileManager},
      {"Time", LauncherAction::ToolsTime},
      {"Trackpad", LauncherAction::NetworkTrackpad},
      {"Sleep Timer", LauncherAction::ToolsSleepTimer},
      {"OTA Update", LauncherAction::ToolsOtaUpdate},
  };
}

std::vector<LauncherActivity::SubmenuItem> buildSettingsMenu() {
  return {
      {"WiFi", LauncherAction::SettingsWifi},
      {"Hardware Test", LauncherAction::SettingsHardwareTest},
  };
}

#if defined(PLATFORM_M5PAPERS3)
PaperS3Ui::Rect mainTileRect(const GfxRenderer& renderer, const int index, const int itemCount) {
  constexpr int columns = 2;
  const int rows = (itemCount + columns - 1) / columns;
  constexpr int outerMargin = 24;
  constexpr int tileGap = 16;
  constexpr int topY = 198;
  constexpr int bottomReserve = 132;
  const int tileWidth = (renderer.getScreenWidth() - outerMargin * 2 - tileGap) / columns;
  const int tileHeight = (renderer.getScreenHeight() - topY - bottomReserve - tileGap * (rows - 1)) / rows;
  const int row = index / columns;
  const int col = index % columns;
  PaperS3Ui::Rect rect;
  rect.x = outerMargin + col * (tileWidth + tileGap);
  rect.y = topY + row * (tileHeight + tileGap);
  rect.width = tileWidth;
  rect.height = tileHeight;
  return rect;
}

PaperS3Ui::Rect submenuRowRect(const GfxRenderer& renderer, const int index) {
  PaperS3Ui::Rect rect;
  rect.x = PaperS3Ui::kOuterMargin;
  rect.y = PaperS3Ui::kListTopY + index * (PaperS3Ui::kListRowHeight + PaperS3Ui::kListRowGap);
  rect.width = renderer.getScreenWidth() - PaperS3Ui::kOuterMargin * 2;
  rect.height = PaperS3Ui::kListRowHeight;
  return rect;
}

bool readPaperS3Tap(const MappedInputManager& mappedInput, int& x, int& y) {
  if (!mappedInput.wasTapped()) {
    return false;
  }
  return PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), x, y);
}

std::string launcherDateTimeText() {
  std::tm localTime{};
  if (!TimeUtils::getLocalTimeWithOffset(localTime, SETTINGS.timezoneOffsetMinutes)) {
    return "--:--";
  }

  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%02d:%02d  %02d/%02d", localTime.tm_hour, localTime.tm_min, localTime.tm_mon + 1,
           localTime.tm_mday);
  return std::string(buffer);
}

std::string launcherWifiText() {
  if (WiFi.status() != WL_CONNECTED) {
    return "WiFi off";
  }

  char buffer[24];
  snprintf(buffer, sizeof(buffer), "WiFi %ddBm", WiFi.RSSI());
  return std::string(buffer);
}

void drawPaperS3StatusStrip(GfxRenderer& renderer) {
  constexpr int stripY = 26;
  constexpr int stripHeight = 44;
  constexpr int inset = 24;

  renderer.drawRect(inset, stripY, renderer.getScreenWidth() - inset * 2, stripHeight);

  char batteryText[24];
#ifdef PLATFORM_M5PAPER
  int batteryPct = M5.Power.getBatteryLevel();
  if (batteryPct < 0) {
    batteryPct = 0;
  }
  if (batteryPct > 100) {
    batteryPct = 100;
  }
#else
  constexpr int batteryPct = 0;
#endif
  snprintf(batteryText, sizeof(batteryText), "Battery %d%%", batteryPct);
  renderer.drawText(UI_10_FONT_ID, inset + 14, stripY + 11, batteryText, true, EpdFontFamily::BOLD);

  const std::string wifi = launcherWifiText();
  const int wifiWidth = renderer.getTextWidth(UI_10_FONT_ID, wifi.c_str(), EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, (renderer.getScreenWidth() - wifiWidth) / 2, stripY + 11, wifi.c_str(), true,
                    EpdFontFamily::BOLD);

  const std::string dateTime = launcherDateTimeText();
  const int dateTimeWidth = renderer.getTextWidth(UI_10_FONT_ID, dateTime.c_str(), EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, renderer.getScreenWidth() - inset - dateTimeWidth - 14, stripY + 11,
                    dateTime.c_str(), true, EpdFontFamily::BOLD);
}

LauncherItemId submenuItemToIcon(const LauncherAction action) {
  switch (action) {
    case LauncherAction::Reader:
      return LauncherItemId::Reader;
    case LauncherAction::Dashboard:
      return LauncherItemId::Dashboard;
    case LauncherAction::SensorsBuiltIn:
    case LauncherAction::SensorsPeripherals:
    case LauncherAction::SensorsExternal:
    case LauncherAction::SensorsI2CTools:
    case LauncherAction::SensorsUv:
    case LauncherAction::SensorsGpio:
    case LauncherAction::SensorsUvLogs:
    case LauncherAction::SensorsUartRx:
    case LauncherAction::SensorsEepromDump:
      return LauncherItemId::Sensors;
    case LauncherAction::Weather:
      return LauncherItemId::Weather;
    case LauncherAction::NetworkWifiScan:
    case LauncherAction::NetworkWifiStatus:
    case LauncherAction::NetworkWifiTests:
    case LauncherAction::NetworkWifiChannels:
    case LauncherAction::NetworkBleScan:
    case LauncherAction::NetworkWebPortal:
    case LauncherAction::NetworkKeyboardHost:
    case LauncherAction::NetworkTrackpad:
    case LauncherAction::NetworkSshClient:
      return LauncherItemId::Network;
    case LauncherAction::GamePoodle:
    case LauncherAction::GameSudoku:
    case LauncherAction::GameMinesweeper:
    case LauncherAction::GameTetris:
      return LauncherItemId::Games;
    case LauncherAction::ImagesViewer:
    case LauncherAction::ImagesDraw:
      return LauncherItemId::Images;
    case LauncherAction::ToolsNotes:
    case LauncherAction::ToolsFileManager:
    case LauncherAction::ToolsTime:
    case LauncherAction::ToolsSleepTimer:
    case LauncherAction::ToolsOtaUpdate:
      return LauncherItemId::Tools;
    case LauncherAction::SettingsWifi:
    case LauncherAction::SettingsHardwareTest:
      return LauncherItemId::Settings;
    case LauncherAction::Calculator:
      return LauncherItemId::Calculator;
    case LauncherAction::None:
    default:
      return LauncherItemId::Tools;
  }
}

const char* submenuBadge(const LauncherAction action) {
  switch (action) {
    case LauncherAction::Reader:
      return "RD";
    case LauncherAction::Dashboard:
      return "DB";
    case LauncherAction::SensorsBuiltIn:
      return "BI";
    case LauncherAction::SensorsPeripherals:
      return "DEV";
    case LauncherAction::SensorsExternal:
      return "EXT";
    case LauncherAction::SensorsI2CTools:
      return "I2C";
    case LauncherAction::SensorsUv:
      return "UV";
    case LauncherAction::SensorsGpio:
      return "GPIO";
    case LauncherAction::SensorsUvLogs:
      return "LOG";
    case LauncherAction::SensorsUartRx:
      return "UART";
    case LauncherAction::SensorsEepromDump:
      return "ROM";
    case LauncherAction::Weather:
      return "WX";
    case LauncherAction::NetworkWifiScan:
      return "SCAN";
    case LauncherAction::NetworkWifiStatus:
      return "WIFI";
    case LauncherAction::NetworkWifiTests:
      return "TEST";
    case LauncherAction::NetworkWifiChannels:
      return "CH";
    case LauncherAction::NetworkBleScan:
      return "BLE";
    case LauncherAction::NetworkWebPortal:
      return "WEB";
    case LauncherAction::NetworkKeyboardHost:
      return "KEY";
    case LauncherAction::NetworkTrackpad:
      return "PAD";
    case LauncherAction::NetworkSshClient:
      return "SSH";
    case LauncherAction::GamePoodle:
      return "PDL";
    case LauncherAction::GameSudoku:
      return "SDK";
    case LauncherAction::GameMinesweeper:
      return "MINE";
    case LauncherAction::GameTetris:
      return "TET";
    case LauncherAction::ImagesViewer:
      return "IMG";
    case LauncherAction::ImagesDraw:
      return "DRAW";
    case LauncherAction::ToolsNotes:
      return "NOTE";
    case LauncherAction::ToolsFileManager:
      return "FILE";
    case LauncherAction::ToolsTime:
      return "TIME";
    case LauncherAction::ToolsSleepTimer:
      return "SLEEP";
    case LauncherAction::ToolsOtaUpdate:
      return "OTA";
    case LauncherAction::SettingsWifi:
      return "WIFI";
    case LauncherAction::SettingsHardwareTest:
      return "HW";
    case LauncherAction::Calculator:
      return "CALC";
    case LauncherAction::None:
    default:
      return "";
  }
}

const char* mainTileDescription(const LauncherItemId id) {
  switch (id) {
    case LauncherItemId::Reader:
      return "Browse storage and open books";
    case LauncherItemId::Dashboard:
      return "Battery, USB, weather";
    case LauncherItemId::Sensors:
      return "Built-in and I2C tools";
    case LauncherItemId::Weather:
      return "Forecast and conditions";
    case LauncherItemId::Network:
      return "Wi-Fi, BLE and SSH";
    case LauncherItemId::Games:
      return "Poodle, Sudoku, Mines, Tetris";
    case LauncherItemId::Images:
      return "Viewer and sketchpad";
    case LauncherItemId::Tools:
      return "Notes, files and time";
    case LauncherItemId::Settings:
      return "Wireless and hardware";
    case LauncherItemId::Calculator:
      return "Scientific calculator";
    default:
      return "";
  }
}

const char* mainTileTapHint(const LauncherActivity::MenuItem& item) {
  (void)item;
  return "";
}

const char* submenuDescription(const LauncherAction action) {
  switch (action) {
    case LauncherAction::SensorsBuiltIn:
      return "PaperS3 battery, RTC, touch and IMU";
    case LauncherAction::SensorsPeripherals:
      return "Inspect supported external boards";
    case LauncherAction::SensorsI2CTools:
      return "Scan and inspect the I2C bus";
    case LauncherAction::NetworkWifiStatus:
      return "Connection state, IP and RSSI";
    case LauncherAction::NetworkWifiScan:
      return "Find nearby wireless networks";
    case LauncherAction::NetworkWifiTests:
      return "Run connectivity checks";
    case LauncherAction::NetworkWifiChannels:
      return "Watch activity by Wi-Fi channel";
    case LauncherAction::NetworkBleScan:
      return "Scan nearby BLE advertisements";
    case LauncherAction::NetworkWebPortal:
      return "Serve local files over Wi-Fi";
    case LauncherAction::NetworkKeyboardHost:
      return "USB or BLE host keyboard bridge";
    case LauncherAction::NetworkSshClient:
      return "Run remote commands over SSH";
    case LauncherAction::GamePoodle:
      return "Touch-friendly daily word game";
    case LauncherAction::GameSudoku:
      return "Tap-to-fill number puzzle";
    case LauncherAction::GameMinesweeper:
      return "Reveal cells and mark hidden mines";
    case LauncherAction::GameTetris:
      return "Fast drop block puzzler";
    case LauncherAction::ImagesViewer:
      return "Browse images from storage";
    case LauncherAction::ImagesDraw:
      return "Sketch with the pen layer";
    case LauncherAction::ToolsNotes:
      return "Quick notes with the touch keyboard";
    case LauncherAction::ToolsFileManager:
      return "Browse SD and internal files";
    case LauncherAction::ToolsTime:
      return "Clock, RTC and timezone setup";
    case LauncherAction::NetworkTrackpad:
      return "Use touch as a remote pointer";
    case LauncherAction::ToolsSleepTimer:
      return "Schedule display sleep";
    case LauncherAction::ToolsOtaUpdate:
      return "Check and install firmware updates";
    case LauncherAction::SettingsWifi:
      return "Manage saved Wi-Fi credentials";
    case LauncherAction::SettingsHardwareTest:
      return "Run PaperS3 hardware diagnostics";
    default:
      return "";
  }
}
#endif
}  // namespace

LauncherActivity::LauncherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                   const ActionCallback& onAction)
    : Activity("Launcher", renderer, mappedInput), onAction(onAction) {}

void LauncherActivity::onEnter() {
  Activity::onEnter();
  rebuildVisibleItems();
  selectionIndex = 0;
  submenuSelection = 0;
  menuState = MenuState::Main;
  lastOverlayRefreshMs = millis();
  needsRender = true;
}

void LauncherActivity::loop() {
  if (SETTINGS.infoOverlayPosition != CrossPointSettings::OVERLAY_OFF && (millis() - lastOverlayRefreshMs) >= 30000) {
    lastOverlayRefreshMs = millis();
    needsRender = true;
  }

  handleInput();
  if (needsRender) {
    render();
    needsRender = false;
  }
}

void LauncherActivity::rebuildVisibleItems() {
  visibleItems.clear();
  for (const auto& item : kAllItems) {
    visibleItems.push_back(item);
  }
}

void LauncherActivity::handleInput() {
  if (menuState == MenuState::Main) {
    handleMainInput();
  } else {
    switch (menuState) {
      case MenuState::Sensors:
        handleSubmenuInput(buildSensorsMenu());
        break;
      case MenuState::Network:
        handleSubmenuInput(buildNetworkMenu());
        break;
      case MenuState::Games:
        handleSubmenuInput(buildGamesMenu());
        break;
      case MenuState::Images:
        handleSubmenuInput(buildImagesMenu());
        break;
      case MenuState::Tools:
        handleSubmenuInput(buildToolsMenu());
        break;
      case MenuState::Settings:
        handleSubmenuInput(buildSettingsMenu());
        break;
      default:
        break;
    }
  }
}

void LauncherActivity::handleMainInput() {
#if defined(PLATFORM_M5PAPERS3)
  {
    int tapX = 0;
    int tapY = 0;
    if (readPaperS3Tap(mappedInput, tapX, tapY)) {
      const int count = static_cast<int>(visibleItems.size());
      for (int i = 0; i < count; i++) {
        if (!mainTileRect(renderer, i, count).contains(tapX, tapY)) {
          continue;
        }

        selectionIndex = i;
        needsRender = true;

        const auto selected = visibleItems[selectionIndex];
        if (selected.hasSubmenu) {
          switch (selected.id) {
            case LauncherItemId::Sensors:
              menuState = MenuState::Sensors;
              break;
            case LauncherItemId::Network:
              menuState = MenuState::Network;
              break;
            case LauncherItemId::Games:
              menuState = MenuState::Games;
              break;
            case LauncherItemId::Images:
              menuState = MenuState::Images;
              break;
            case LauncherItemId::Tools:
              menuState = MenuState::Tools;
              break;
            case LauncherItemId::Settings:
              menuState = MenuState::Settings;
              break;
            default:
              break;
          }
          submenuSelection = 0;
          return;
        }

        if (onAction) {
          switch (selected.id) {
            case LauncherItemId::Reader:
              onAction(LauncherAction::Reader);
              break;
            case LauncherItemId::Dashboard:
              onAction(LauncherAction::Dashboard);
              break;
            case LauncherItemId::Weather:
              onAction(LauncherAction::Weather);
              break;
            case LauncherItemId::Calculator:
              onAction(LauncherAction::Calculator);
              break;
            default:
              break;
          }
        }
        return;
      }
    }
  }
#endif

  const int legacyCount = static_cast<int>(visibleItems.size());
  const int columns = 3;
  const int rows = (legacyCount + columns - 1) / columns;

  int row = selectionIndex / columns;
  int col = selectionIndex % columns;

  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    col = (col - 1 + columns) % columns;
    int newIndex = row * columns + col;
    if (newIndex >= legacyCount) {
      newIndex = legacyCount - 1;
    }
    selectionIndex = newIndex;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    col = (col + 1) % columns;
    int newIndex = row * columns + col;
    if (newIndex >= legacyCount) {
      newIndex = legacyCount - 1;
    }
    selectionIndex = newIndex;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    row = (row - 1 + rows) % rows;
    int newIndex = row * columns + col;
    if (newIndex >= legacyCount) {
      newIndex = legacyCount - 1;
    }
    selectionIndex = newIndex;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    row = (row + 1) % rows;
    int newIndex = row * columns + col;
    if (newIndex >= legacyCount) {
      newIndex = legacyCount - 1;
    }
    selectionIndex = newIndex;
    needsRender = true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    const auto selected = visibleItems[selectionIndex];
    if (selected.hasSubmenu) {
      switch (selected.id) {
        case LauncherItemId::Sensors:
          menuState = MenuState::Sensors;
          break;
        case LauncherItemId::Network:
          menuState = MenuState::Network;
          break;
        case LauncherItemId::Games:
          menuState = MenuState::Games;
          break;
        case LauncherItemId::Images:
          menuState = MenuState::Images;
          break;
        case LauncherItemId::Tools:
          menuState = MenuState::Tools;
          break;
        case LauncherItemId::Settings:
          menuState = MenuState::Settings;
          break;
        default:
          break;
      }
      submenuSelection = 0;
      needsRender = true;
    } else if (onAction) {
      switch (selected.id) {
        case LauncherItemId::Reader:
          onAction(LauncherAction::Reader);
          break;
        case LauncherItemId::Dashboard:
          onAction(LauncherAction::Dashboard);
          break;
        case LauncherItemId::Weather:
          onAction(LauncherAction::Weather);
          break;
        case LauncherItemId::Calculator:
          onAction(LauncherAction::Calculator);
          break;
        default:
          break;
      }
    }
  }
}

void LauncherActivity::handleSubmenuInput(const std::vector<SubmenuItem>& items) {
  const int count = static_cast<int>(items.size());

#if defined(PLATFORM_M5PAPERS3)
  {
    int tapX = 0;
    int tapY = 0;
    if (readPaperS3Tap(mappedInput, tapX, tapY)) {
      if (PaperS3Ui::backButtonRect(renderer).contains(tapX, tapY)) {
        menuState = MenuState::Main;
        needsRender = true;
        return;
      }

      for (int i = 0; i < count; i++) {
        if (!submenuRowRect(renderer, i).contains(tapX, tapY)) {
          continue;
        }
        submenuSelection = i;
        needsRender = true;
        if (onAction) {
          onAction(items[i].action);
        }
        return;
      }

      if (submenuRowRect(renderer, count).contains(tapX, tapY)) {
        submenuSelection = count;
        menuState = MenuState::Main;
        needsRender = true;
        return;
      }
    }
  }
#endif

  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    submenuSelection = (submenuSelection - 1 + count + 1) % (count + 1);
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    submenuSelection = (submenuSelection + 1) % (count + 1);
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    menuState = MenuState::Main;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (submenuSelection == count) {
      menuState = MenuState::Main;
      needsRender = true;
      return;
    }
    if (onAction) {
      onAction(items[submenuSelection].action);
    }
  }
}

void LauncherActivity::render() {
  renderer.clearScreen();

#if defined(PLATFORM_M5PAPERS3)
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
#else
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
#endif

  if (menuState == MenuState::Main) {
    renderMainMenu();
#if !defined(PLATFORM_M5PAPERS3)
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 20, "Arrows: Move   Confirm: Select");
#endif
  } else {
    switch (menuState) {
      case MenuState::Sensors:
        renderSubmenu("Sensors", buildSensorsMenu());
        break;
      case MenuState::Network:
        renderSubmenu("Network", buildNetworkMenu());
        break;
      case MenuState::Games:
        renderSubmenu("Games", buildGamesMenu());
        break;
      case MenuState::Images:
        renderSubmenu("Images", buildImagesMenu());
        break;
      case MenuState::Tools:
        renderSubmenu("Tools", buildToolsMenu());
        break;
      case MenuState::Settings:
        renderSubmenu("Settings", buildSettingsMenu());
        break;
      default:
        break;
    }
#if !defined(PLATFORM_M5PAPERS3)
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 20,
                              "Up/Down: Move   Confirm: Select   Back: Return");
#endif
  }

#if defined(PLATFORM_M5PAPERS3)
  if (menuState == MenuState::Main) {
    drawPaperS3StatusStrip(renderer);
  }
#else
  ScreenComponents::drawDeviceInfoOverlay(renderer, SETTINGS.infoOverlayPosition);
#endif
  renderer.displayBuffer();
}

void LauncherActivity::renderMainMenu() {
#if defined(PLATFORM_M5PAPERS3)
  {
    PaperS3Ui::drawScreenHeader(renderer, "OmniPaper", "Reader, tools, and diagnostics");

    const int count = static_cast<int>(visibleItems.size());
    for (int i = 0; i < count; i++) {
      const auto rect = mainTileRect(renderer, i, count);
      const bool selected = (i == selectionIndex);
      const int iconBoxSize = 56;
      const int iconBoxX = rect.x + 18;
      const int iconBoxY = rect.y + 18;
      const int labelX = iconBoxX + iconBoxSize + 18;
      const int labelY = rect.y + 22;

      renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
      renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
      if (selected) {
        renderer.fillRect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2, true);
        renderer.drawRect(rect.x + 3, rect.y + 3, rect.width - 6, rect.height - 6, false);
      }

      renderer.fillRect(iconBoxX, iconBoxY, iconBoxSize, iconBoxSize, false);
      renderer.drawRect(iconBoxX, iconBoxY, iconBoxSize, iconBoxSize, true);
      drawIconSymbol(iconBoxX + iconBoxSize / 2, iconBoxY + iconBoxSize / 2, visibleItems[i].id, selected);

      renderer.drawText(NOTOSANS_14_FONT_ID, labelX, labelY, visibleItems[i].label, !selected, EpdFontFamily::BOLD);
      const int subtitleWidth = rect.x + rect.width - labelX - 18;
      const std::string subtitle =
          renderer.truncatedText(UI_12_FONT_ID, mainTileDescription(visibleItems[i].id), subtitleWidth);
      renderer.drawText(UI_12_FONT_ID, labelX, labelY + 30, subtitle.c_str(), !selected);

      const char* hint = mainTileTapHint(visibleItems[i]);
      if (hint[0] != '\0') {
        renderer.drawText(UI_10_FONT_ID, labelX, rect.y + rect.height - 28, hint, !selected);
      }

      if (visibleItems[i].hasSubmenu) {
        renderer.drawLine(rect.x + rect.width - 34, rect.y + rect.height - 30, rect.x + rect.width - 24,
                          rect.y + rect.height - 20, !selected);
        renderer.drawLine(rect.x + rect.width - 24, rect.y + rect.height - 20, rect.x + rect.width - 34,
                          rect.y + rect.height - 10, !selected);
      }
    }
    if (!visibleItems.empty()) {
      const auto& item = visibleItems[selectionIndex];
      const std::string status = std::string(item.label) + ": " + mainTileDescription(item.id);
      PaperS3Ui::drawFooterStatus(renderer, status.c_str());
    }
    return;
  }
#endif

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "OmniPaper Launcher");

  const int count = static_cast<int>(visibleItems.size());
  const int columns = 3;
  const int rows = (count + columns - 1) / columns;
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();

  const int topMargin = 60;
  const int bottomMargin = 50;
  const int cellW = screenW / columns;
  const int cellH = (screenH - topMargin - bottomMargin) / rows;
  const int radius = std::min(cellW, cellH) / 4;

  for (int i = 0; i < count; i++) {
    int row = i / columns;
    int col = i % columns;
    int cx = col * cellW + cellW / 2;
    int cy = topMargin + row * cellH + cellH / 2 - 10;

    bool selected = (i == selectionIndex);
    if (selected) {
      drawCircle(cx, cy, radius + 4, true);
    } else {
      drawCircle(cx, cy, radius + 4, false);
    }
    drawIconSymbol(cx, cy, visibleItems[i].id, selected);
    drawLabel(cx, cy + radius + 18, visibleItems[i].label, selected);
  }
}

void LauncherActivity::renderSubmenu(const char* title, const std::vector<SubmenuItem>& items) {
#if defined(PLATFORM_M5PAPERS3)
  PaperS3Ui::drawScreenHeader(renderer, title);
  PaperS3Ui::drawBackButton(renderer);

  for (size_t i = 0; i < items.size(); i++) {
    const auto rect = submenuRowRect(renderer, static_cast<int>(i));
    const bool selected = static_cast<int>(i) == submenuSelection;
    const int iconBoxSize = 40;
    const int iconBoxX = rect.x + 16;
    const int iconBoxY = rect.y + (rect.height - iconBoxSize) / 2;
    const int textX = iconBoxX + iconBoxSize + 18;

    renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
    renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
    if (selected) {
      renderer.fillRect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2, true);
    }

    renderer.fillRect(iconBoxX, iconBoxY, iconBoxSize, iconBoxSize, false);
    renderer.drawRect(iconBoxX, iconBoxY, iconBoxSize, iconBoxSize, true);

    drawSubmenuActionIcon(iconBoxX + iconBoxSize / 2, iconBoxY + iconBoxSize / 2, items[i].action, selected);

    renderer.drawText(NOTOSANS_14_FONT_ID, textX, rect.y + 14, items[i].label, !selected, EpdFontFamily::BOLD);
    const int subtitleWidth = rect.x + rect.width - textX - 18;
    const std::string subtitle =
        renderer.truncatedText(UI_12_FONT_ID, submenuDescription(items[i].action), subtitleWidth - 24);
    renderer.drawText(UI_12_FONT_ID, textX, rect.y + 44, subtitle.c_str(), !selected);
    renderer.drawLine(rect.x + rect.width - 30, rect.y + rect.height / 2 - 8, rect.x + rect.width - 18,
                      rect.y + rect.height / 2, !selected);
    renderer.drawLine(rect.x + rect.width - 18, rect.y + rect.height / 2, rect.x + rect.width - 30,
                      rect.y + rect.height / 2 + 8, !selected);
  }

  const auto backRect = submenuRowRect(renderer, static_cast<int>(items.size()));
  const bool selected = submenuSelection == static_cast<int>(items.size());
  renderer.fillRect(backRect.x, backRect.y, backRect.width, backRect.height, false);
  renderer.drawRect(backRect.x, backRect.y, backRect.width, backRect.height);
  if (selected) {
    renderer.fillRect(backRect.x + 1, backRect.y + 1, backRect.width - 2, backRect.height - 2, true);
  }
  renderer.drawText(NOTOSANS_14_FONT_ID, backRect.x + 18, backRect.y + 14, "Back", !selected, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, backRect.x + 18, backRect.y + 44, "Return to the launcher", !selected);
  if (submenuSelection < static_cast<int>(items.size())) {
    PaperS3Ui::drawFooterStatus(renderer, submenuDescription(items[submenuSelection].action));
  } else {
    PaperS3Ui::drawFooterStatus(renderer, "Return to the launcher");
  }
  return;
#endif

  renderer.drawCenteredText(UI_12_FONT_ID, 16, title);

  const int startY = 70;
  int y = startY;
  for (size_t i = 0; i < items.size(); i++) {
    bool selected = static_cast<int>(i) == submenuSelection;
    if (selected) {
      renderer.fillRect(40, y - 6, renderer.getScreenWidth() - 80, 24, true);
      renderer.drawText(UI_12_FONT_ID, 50, y, items[i].label, false);
    } else {
      renderer.drawText(UI_12_FONT_ID, 50, y, items[i].label);
    }
    y += 28;
  }

  bool backSelected = submenuSelection == static_cast<int>(items.size());
  if (backSelected) {
    renderer.fillRect(40, y - 6, renderer.getScreenWidth() - 80, 24, true);
    renderer.drawText(UI_12_FONT_ID, 50, y, "Back", false);
  } else {
    renderer.drawText(UI_12_FONT_ID, 50, y, "Back");
  }
}

void LauncherActivity::drawCircle(int cx, int cy, int radius, bool fill, bool color) const {
  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      int dist2 = dx * dx + dy * dy;
      if (fill) {
        if (dist2 <= radius * radius) {
          renderer.drawPixel(cx + dx, cy + dy, color);
        }
      } else {
        if (dist2 <= radius * radius && dist2 >= (radius - 2) * (radius - 2)) {
          renderer.drawPixel(cx + dx, cy + dy, color);
        }
      }
    }
  }
}

void LauncherActivity::drawIconSymbol(int cx, int cy, LauncherItemId id, bool selected) const {
  (void)selected;
  const bool black = true;
  switch (id) {
    case LauncherItemId::Reader:
      renderer.drawRect(cx - 16, cy - 10, 32, 20, black);
      renderer.drawLine(cx - 16, cy - 10, cx - 16, cy + 10, black);
      break;
    case LauncherItemId::Dashboard:
      renderer.drawRect(cx - 16, cy - 10, 32, 20, black);
      renderer.drawLine(cx - 12, cy + 6, cx - 4, cy - 2, black);
      renderer.drawLine(cx - 4, cy - 2, cx + 6, cy + 8, black);
      renderer.drawLine(cx + 6, cy + 8, cx + 12, cy - 6, black);
      break;
    case LauncherItemId::Sensors:
      renderer.drawRect(cx - 8, cy - 8, 16, 16, black);
      renderer.drawLine(cx - 12, cy, cx + 12, cy, black);
      break;
    case LauncherItemId::Weather:
      drawCircle(cx, cy, 10, false, black);
      renderer.drawLine(cx - 16, cy + 6, cx + 16, cy + 6, black);
      break;
    case LauncherItemId::Network:
      renderer.drawLine(cx, cy - 10, cx, cy + 10, black);
      renderer.drawLine(cx - 10, cy + 10, cx + 10, cy + 10, black);
      renderer.drawLine(cx - 12, cy, cx + 12, cy, black);
      break;
    case LauncherItemId::Games:
      renderer.drawRect(cx - 18, cy - 6, 36, 12, black);
      renderer.drawRect(cx - 6, cy - 2, 4, 4, black);
      renderer.drawRect(cx + 4, cy - 2, 4, 4, black);
      break;
    case LauncherItemId::Images:
      renderer.drawRect(cx - 18, cy - 12, 36, 24, black);
      renderer.drawLine(cx - 10, cy + 6, cx, cy - 2, black);
      renderer.drawLine(cx, cy - 2, cx + 10, cy + 6, black);
      break;
    case LauncherItemId::Tools:
      renderer.drawRect(cx - 14, cy - 10, 28, 20, black);
      renderer.drawLine(cx - 6, cy - 10, cx - 6, cy + 10, black);
      renderer.drawLine(cx + 2, cy - 10, cx + 2, cy + 10, black);
      break;
    case LauncherItemId::Settings:
      drawCircle(cx, cy, 12, false, black);
      renderer.drawLine(cx, cy - 12, cx, cy + 12, black);
      renderer.drawLine(cx - 12, cy, cx + 12, cy, black);
      break;
    case LauncherItemId::Calculator:
      renderer.drawRect(cx - 14, cy - 18, 28, 36, black);
      renderer.drawLine(cx - 10, cy - 4, cx + 10, cy - 4, black);
      renderer.drawLine(cx - 10, cy + 4, cx + 10, cy + 4, black);
      break;
    default:
      break;
  }
}

void LauncherActivity::drawSubmenuActionIcon(int cx, int cy, LauncherAction action, bool selected) const {
  (void)selected;
  const bool black = true;
  switch (action) {
    case LauncherAction::SensorsBuiltIn:
      renderer.drawRect(cx - 11, cy - 11, 22, 22, black);
      renderer.fillRect(cx - 3, cy - 3, 6, 6, black);
      break;
    case LauncherAction::SensorsPeripherals:
      renderer.drawLine(cx - 10, cy - 8, cx - 2, cy - 8, black);
      renderer.drawLine(cx - 2, cy - 8, cx - 2, cy + 8, black);
      renderer.drawLine(cx + 2, cy - 8, cx + 2, cy + 8, black);
      renderer.drawLine(cx + 2, cy - 8, cx + 10, cy - 8, black);
      renderer.drawLine(cx + 2, cy + 8, cx + 10, cy + 8, black);
      break;
    case LauncherAction::SensorsI2CTools:
      renderer.drawLine(cx - 12, cy, cx + 12, cy, black);
      renderer.drawRect(cx - 12, cy - 6, 6, 12, black);
      renderer.drawRect(cx - 3, cy - 6, 6, 12, black);
      renderer.drawRect(cx + 6, cy - 6, 6, 12, black);
      break;
    case LauncherAction::NetworkWifiStatus:
    case LauncherAction::SettingsWifi:
      renderer.drawLine(cx - 10, cy + 6, cx, cy - 4, black);
      renderer.drawLine(cx, cy - 4, cx + 10, cy + 6, black);
      renderer.drawLine(cx - 6, cy + 2, cx, cy - 2, black);
      renderer.drawLine(cx, cy - 2, cx + 6, cy + 2, black);
      renderer.fillRect(cx - 2, cy + 8, 4, 4, black);
      break;
    case LauncherAction::NetworkWifiScan:
      drawSubmenuActionIcon(cx - 3, cy, LauncherAction::NetworkWifiStatus, selected);
      drawCircle(cx + 9, cy + 8, 6, false, black);
      renderer.drawLine(cx + 13, cy + 12, cx + 17, cy + 16, black);
      break;
    case LauncherAction::NetworkWifiTests:
      renderer.drawRect(cx - 12, cy - 12, 24, 24, black);
      renderer.drawLine(cx - 8, cy, cx - 2, cy + 6, black);
      renderer.drawLine(cx - 2, cy + 6, cx + 8, cy - 6, black);
      break;
    case LauncherAction::NetworkWifiChannels:
      renderer.fillRect(cx - 11, cy + 2, 4, 10, black);
      renderer.fillRect(cx - 3, cy - 2, 4, 14, black);
      renderer.fillRect(cx + 5, cy - 8, 4, 20, black);
      break;
    case LauncherAction::NetworkBleScan:
      renderer.drawLine(cx - 2, cy - 12, cx - 2, cy + 12, black);
      renderer.drawLine(cx - 2, cy, cx + 8, cy - 8, black);
      renderer.drawLine(cx - 2, cy, cx + 8, cy + 8, black);
      renderer.drawLine(cx - 2, cy - 12, cx + 6, cy - 4, black);
      renderer.drawLine(cx - 2, cy + 12, cx + 6, cy + 4, black);
      break;
    case LauncherAction::NetworkWebPortal:
      renderer.drawRect(cx - 12, cy - 10, 24, 20, black);
      renderer.drawLine(cx - 12, cy - 4, cx + 12, cy - 4, black);
      renderer.drawLine(cx - 6, cy + 10, cx - 1, cy + 4, black);
      renderer.drawLine(cx - 1, cy + 4, cx + 5, cy + 12, black);
      break;
    case LauncherAction::NetworkKeyboardHost:
      renderer.drawRect(cx - 13, cy - 9, 26, 18, black);
      renderer.drawLine(cx - 8, cy - 1, cx + 8, cy - 1, black);
      renderer.drawLine(cx - 8, cy + 5, cx + 8, cy + 5, black);
      renderer.drawLine(cx - 4, cy - 7, cx - 4, cy + 7, black);
      renderer.drawLine(cx + 4, cy - 7, cx + 4, cy + 7, black);
      break;
    case LauncherAction::NetworkTrackpad:
      renderer.drawRect(cx - 12, cy - 10, 24, 20, black);
      renderer.drawLine(cx - 2, cy - 10, cx - 2, cy + 10, black);
      renderer.drawLine(cx + 4, cy - 2, cx + 8, cy + 2, black);
      renderer.drawLine(cx + 8, cy + 2, cx + 4, cy + 6, black);
      break;
    case LauncherAction::NetworkSshClient:
      renderer.drawRect(cx - 12, cy - 10, 24, 20, black);
      renderer.drawLine(cx - 8, cy - 3, cx - 2, cy + 3, black);
      renderer.drawLine(cx - 8, cy + 3, cx - 2, cy - 3, black);
      renderer.drawLine(cx + 1, cy + 5, cx + 8, cy + 5, black);
      break;
    case LauncherAction::GameSudoku:
      renderer.drawRect(cx - 12, cy - 12, 24, 24, black);
      renderer.drawLine(cx - 4, cy - 12, cx - 4, cy + 12, black);
      renderer.drawLine(cx + 4, cy - 12, cx + 4, cy + 12, black);
      renderer.drawLine(cx - 12, cy - 4, cx + 12, cy - 4, black);
      renderer.drawLine(cx - 12, cy + 4, cx + 12, cy + 4, black);
      break;
    case LauncherAction::GameMinesweeper:
      renderer.drawLine(cx - 10, cy, cx + 10, cy, black);
      renderer.drawLine(cx, cy - 10, cx, cy + 10, black);
      renderer.drawLine(cx - 8, cy - 8, cx + 8, cy + 8, black);
      renderer.drawLine(cx - 8, cy + 8, cx + 8, cy - 8, black);
      renderer.fillRect(cx - 3, cy - 3, 6, 6, black);
      break;
    case LauncherAction::GameTetris:
      renderer.drawRect(cx - 12, cy - 6, 8, 8, black);
      renderer.drawRect(cx - 4, cy - 6, 8, 8, black);
      renderer.drawRect(cx + 4, cy - 6, 8, 8, black);
      renderer.drawRect(cx + 4, cy + 2, 8, 8, black);
      break;
    case LauncherAction::ImagesViewer:
      renderer.drawRect(cx - 12, cy - 10, 24, 20, black);
      renderer.drawLine(cx - 8, cy + 4, cx - 1, cy - 2, black);
      renderer.drawLine(cx - 1, cy - 2, cx + 8, cy + 6, black);
      drawCircle(cx + 5, cy - 5, 3, false, black);
      break;
    case LauncherAction::ImagesDraw:
      renderer.drawLine(cx - 11, cy + 9, cx + 8, cy - 10, black);
      renderer.drawLine(cx - 8, cy + 12, cx + 11, cy - 7, black);
      renderer.drawLine(cx - 12, cy + 8, cx - 6, cy + 14, black);
      break;
    case LauncherAction::ToolsNotes:
      renderer.drawRect(cx - 11, cy - 12, 22, 24, black);
      renderer.drawLine(cx - 7, cy - 4, cx + 7, cy - 4, black);
      renderer.drawLine(cx - 7, cy + 1, cx + 7, cy + 1, black);
      renderer.drawLine(cx - 7, cy + 6, cx + 5, cy + 6, black);
      break;
    case LauncherAction::ToolsFileManager:
      renderer.drawRect(cx - 12, cy - 6, 24, 16, black);
      renderer.drawLine(cx - 12, cy - 6, cx - 4, cy - 12, black);
      renderer.drawLine(cx - 4, cy - 12, cx + 2, cy - 12, black);
      break;
    case LauncherAction::ToolsTime:
      drawCircle(cx, cy, 11, false, black);
      renderer.drawLine(cx, cy, cx, cy - 6, black);
      renderer.drawLine(cx, cy, cx + 5, cy + 3, black);
      break;
    case LauncherAction::ToolsSleepTimer:
      drawCircle(cx - 2, cy, 9, false, black);
      renderer.fillRect(cx + 2, cy - 9, 5, 18, !black);
      renderer.drawLine(cx + 5, cy - 8, cx + 8, cy - 11, black);
      renderer.drawLine(cx + 7, cy - 5, cx + 10, cy - 8, black);
      renderer.drawLine(cx + 8, cy, cx + 12, cy, black);
      break;
    case LauncherAction::ToolsOtaUpdate:
      renderer.drawRect(cx - 12, cy - 10, 24, 20, black);
      renderer.drawLine(cx, cy - 7, cx, cy + 5, black);
      renderer.drawLine(cx - 5, cy, cx, cy + 5, black);
      renderer.drawLine(cx + 5, cy, cx, cy + 5, black);
      break;
    case LauncherAction::SettingsHardwareTest:
      renderer.drawRect(cx - 10, cy - 10, 20, 20, black);
      renderer.drawLine(cx - 10, cy, cx + 10, cy, black);
      renderer.drawLine(cx, cy - 10, cx, cy + 10, black);
      renderer.drawLine(cx - 6, cy - 6, cx + 6, cy + 6, black);
      break;
    case LauncherAction::GamePoodle:
      drawIconSymbol(cx, cy, LauncherItemId::Games, selected);
      renderer.drawRect(cx - 4, cy - 6, 8, 12, black);
      break;
    default:
      drawIconSymbol(cx, cy, LauncherItemId::Tools, selected);
      break;
  }
}

void LauncherActivity::drawLabel(int cx, int cy, const char* text, bool selected) const {
  const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, text);
  int x = cx - textWidth / 2;
  if (selected) {
    renderer.fillRect(x - 6, cy - 4, textWidth + 12, 18, true);
    renderer.drawText(UI_12_FONT_ID, x, cy, text, false);
  } else {
    renderer.drawText(UI_12_FONT_ID, x, cy, text, true);
  }
}
