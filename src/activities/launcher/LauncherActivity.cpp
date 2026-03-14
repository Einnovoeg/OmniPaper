#include "LauncherActivity.h"

#include <algorithm>

#include <GfxRenderer.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "../apps/PaperS3Ui.h"
#include "ScreenComponents.h"
#include "fontIds.h"

namespace {
const LauncherActivity::MenuItem kAllItems[] = {
    {LauncherItemId::Reader, "Reader", false},
    {LauncherItemId::Dashboard, "Dashboard", false},
    {LauncherItemId::Sensors, "Sensors", true},
    {LauncherItemId::Weather, "Weather", false},
    {LauncherItemId::Network, "Network", true},
    {LauncherItemId::Games, "Games", true},
    {LauncherItemId::Images, "Images", true},
    {LauncherItemId::Tools, "Tools", true},
    {LauncherItemId::Settings, "Settings", true},
    {LauncherItemId::Calculator, "Calculator", false},
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
      {"Built-in", LauncherAction::SensorsBuiltIn},
      {"Please connect", LauncherAction::SensorsPeripherals},
      {"I2C Tools", LauncherAction::SensorsI2CTools},
      {"GPIO Monitor", LauncherAction::SensorsGpio},
      {"UART RX", LauncherAction::SensorsUartRx},
      {"EEPROM Dump", LauncherAction::SensorsEepromDump},
  };
#endif
}

std::vector<LauncherActivity::SubmenuItem> buildGamesMenu() {
  return {
      {"Poodle", LauncherAction::GamePoodle},
      {"Sudoku", LauncherAction::GameSudoku},
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
      {"WiFi Status", LauncherAction::NetworkWifiStatus},
      {"WiFi Scan", LauncherAction::NetworkWifiScan},
      {"WiFi Tests", LauncherAction::NetworkWifiTests},
      {"Channels", LauncherAction::NetworkWifiChannels},
      {"BLE Scan", LauncherAction::NetworkBleScan},
      {"Web Portal", LauncherAction::NetworkWebPortal},
      {"Host Keyboard", LauncherAction::NetworkKeyboardHost},
      {"SSH Client", LauncherAction::NetworkSshClient},
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
  constexpr int tileGap = 18;
  constexpr int topY = 108;
  constexpr int bottomReserve = 116;
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
  constexpr int outerMargin = 24;
  constexpr int topY = 112;
  constexpr int rowHeight = 62;
  constexpr int rowGap = 12;
  PaperS3Ui::Rect rect;
  rect.x = outerMargin;
  rect.y = topY + index * (rowHeight + rowGap);
  rect.width = renderer.getScreenWidth() - outerMargin * 2;
  rect.height = rowHeight;
  return rect;
}

bool readPaperS3Tap(const MappedInputManager& mappedInput, int& x, int& y) {
  if (!mappedInput.wasTapped()) {
    return false;
  }
  return PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), x, y);
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
#endif
}

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
  if (SETTINGS.infoOverlayPosition != CrossPointSettings::OVERLAY_OFF &&
      (millis() - lastOverlayRefreshMs) >= 30000) {
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
#if defined(PLATFORM_M5PAPERS3)
    PaperS3Ui::drawFooter(renderer, "Tap a tile directly");
#else
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 20,
                              "Arrows: Move   Confirm: Select");
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
#if defined(PLATFORM_M5PAPERS3)
    PaperS3Ui::drawFooter(renderer, "Tap a row to open");
#else
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 20,
                              "Up/Down: Move   Confirm: Select   Back: Return");
#endif
  }

#if defined(PLATFORM_M5PAPERS3)
  if (menuState == MenuState::Main) {
    ScreenComponents::drawDeviceInfoOverlay(renderer, SETTINGS.infoOverlayPosition);
  }
#else
  ScreenComponents::drawDeviceInfoOverlay(renderer, SETTINGS.infoOverlayPosition);
#endif
  renderer.displayBuffer();
}

void LauncherActivity::renderMainMenu() {
#if defined(PLATFORM_M5PAPERS3)
  {
    renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 22, "OmniPaper", true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_12_FONT_ID, 54, "Launcher");

    const int count = static_cast<int>(visibleItems.size());
    for (int i = 0; i < count; i++) {
      const auto rect = mainTileRect(renderer, i, count);
      const bool selected = (i == selectionIndex);
      const int iconBoxSize = 52;
      const int iconBoxX = rect.x + 18;
      const int iconBoxY = rect.y + (rect.height - iconBoxSize) / 2;
      const int labelX = iconBoxX + iconBoxSize + 18;
      const int labelY = rect.y + 26;

      renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
      renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
      if (selected) {
        renderer.fillRect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2, true);
        renderer.drawRect(rect.x + 3, rect.y + 3, rect.width - 6, rect.height - 6, false);
      }

      renderer.fillRect(iconBoxX, iconBoxY, iconBoxSize, iconBoxSize, selected ? false : true);
      renderer.drawRect(iconBoxX, iconBoxY, iconBoxSize, iconBoxSize, selected ? false : true);
      drawIconSymbol(iconBoxX + iconBoxSize / 2, iconBoxY + iconBoxSize / 2, visibleItems[i].id, selected);

      renderer.drawText(UI_12_FONT_ID, labelX, labelY, visibleItems[i].label, !selected, EpdFontFamily::BOLD);
      renderer.drawText(UI_10_FONT_ID, labelX, labelY + 24,
                        visibleItems[i].hasSubmenu ? "Open section" : "Tap to launch", !selected);
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
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 22, title, true, EpdFontFamily::BOLD);
  PaperS3Ui::drawBackButton(renderer);

  for (size_t i = 0; i < items.size(); i++) {
    const auto rect = submenuRowRect(renderer, static_cast<int>(i));
    const bool selected = static_cast<int>(i) == submenuSelection;
    const int iconBoxSize = 36;
    const int iconBoxX = rect.x + 16;
    const int iconBoxY = rect.y + (rect.height - iconBoxSize) / 2;

    renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
    renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
    if (selected) {
      renderer.fillRect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2, true);
    }

    renderer.fillRect(iconBoxX, iconBoxY, iconBoxSize, iconBoxSize, selected ? false : true);
    renderer.drawRect(iconBoxX, iconBoxY, iconBoxSize, iconBoxSize, selected ? false : true);

    // Submenu entries use explicit per-action badges so touch-first lists do
    // not look like repeated generic category rows on the PaperS3.
    const char* badge = submenuBadge(items[i].action);
    const int badgeWidth = renderer.getTextWidth(UI_10_FONT_ID, badge);
    const int badgeX = iconBoxX + (iconBoxSize - badgeWidth) / 2;
    const int badgeY = iconBoxY + (iconBoxSize - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    renderer.drawText(UI_10_FONT_ID, badgeX, badgeY, badge, !selected, EpdFontFamily::BOLD);

    renderer.drawText(UI_12_FONT_ID, iconBoxX + iconBoxSize + 18, rect.y + 18, items[i].label, !selected,
                      EpdFontFamily::BOLD);
  }

  const auto backRect = submenuRowRect(renderer, static_cast<int>(items.size()));
  const bool selected = submenuSelection == static_cast<int>(items.size());
  renderer.fillRect(backRect.x, backRect.y, backRect.width, backRect.height, false);
  renderer.drawRect(backRect.x, backRect.y, backRect.width, backRect.height);
  if (selected) {
    renderer.fillRect(backRect.x + 1, backRect.y + 1, backRect.width - 2, backRect.height - 2, true);
  }
  renderer.drawText(UI_12_FONT_ID, backRect.x + 18, backRect.y + 18, "Back", !selected, EpdFontFamily::BOLD);
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

void LauncherActivity::drawCircle(int cx, int cy, int radius, bool fill) const {
  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      int dist2 = dx * dx + dy * dy;
      if (fill) {
        if (dist2 <= radius * radius) {
          renderer.drawPixel(cx + dx, cy + dy, true);
        }
      } else {
        if (dist2 <= radius * radius && dist2 >= (radius - 2) * (radius - 2)) {
          renderer.drawPixel(cx + dx, cy + dy, true);
        }
      }
    }
  }
}

void LauncherActivity::drawIconSymbol(int cx, int cy, LauncherItemId id, bool selected) const {
  bool black = !selected; // invert when selected
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
      drawCircle(cx, cy, 10, false);
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
      drawCircle(cx, cy, 12, false);
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
