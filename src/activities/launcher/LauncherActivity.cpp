#include "LauncherActivity.h"

#include <algorithm>

#include <GfxRenderer.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
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
  return {
      {"Built-in", LauncherAction::SensorsBuiltIn},
      {"External Scan", LauncherAction::SensorsExternal},
      {"I2C Tools", LauncherAction::SensorsI2CTools},
      {"UV (AS7331)", LauncherAction::SensorsUv},
      {"UV Logs", LauncherAction::SensorsUvLogs},
      {"GPIO Monitor", LauncherAction::SensorsGpio},
      {"UART RX", LauncherAction::SensorsUartRx},
      {"EEPROM Dump", LauncherAction::SensorsEepromDump},
  };
}

std::vector<LauncherActivity::SubmenuItem> buildGamesMenu() {
  return {
      {"Poodle", LauncherAction::GamePoodle},
      {"Sudoku", LauncherAction::GameSudoku},
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
      {"Trackpad", LauncherAction::NetworkTrackpad},
      {"SSH Client", LauncherAction::NetworkSshClient},
  };
}

std::vector<LauncherActivity::SubmenuItem> buildToolsMenu() {
  return {
      {"Notes", LauncherAction::ToolsNotes},
      {"File Manager", LauncherAction::ToolsFileManager},
      {"Time", LauncherAction::ToolsTime},
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
  const int count = static_cast<int>(visibleItems.size());
  const int columns = 3;
  const int rows = (count + columns - 1) / columns;

  int row = selectionIndex / columns;
  int col = selectionIndex % columns;

  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    col = (col - 1 + columns) % columns;
    int newIndex = row * columns + col;
    if (newIndex >= count) {
      newIndex = count - 1;
    }
    selectionIndex = newIndex;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    col = (col + 1) % columns;
    int newIndex = row * columns + col;
    if (newIndex >= count) {
      newIndex = count - 1;
    }
    selectionIndex = newIndex;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    row = (row - 1 + rows) % rows;
    int newIndex = row * columns + col;
    if (newIndex >= count) {
      newIndex = count - 1;
    }
    selectionIndex = newIndex;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    row = (row + 1) % rows;
    int newIndex = row * columns + col;
    if (newIndex >= count) {
      newIndex = count - 1;
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
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);

  if (menuState == MenuState::Main) {
    renderMainMenu();
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 20,
                              "Arrows: Move   Confirm: Select");
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
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 20,
                              "Up/Down: Move   Confirm: Select   Back: Return");
  }

  ScreenComponents::drawDeviceInfoOverlay(renderer, SETTINGS.infoOverlayPosition);
  renderer.displayBuffer();
}

void LauncherActivity::renderMainMenu() {
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
