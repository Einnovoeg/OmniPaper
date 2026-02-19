#include "WifiScannerActivity.h"

#include <WiFi.h>
#include <SDCardManager.h>

#include <ctime>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"
#include "util/TimeUtils.h"

namespace {
constexpr int kItemsPerPage = 12;
constexpr const char* kLogDir = "/logs";
constexpr const char* kLogFile = "/logs/wifi_scan.csv";
}

WifiScannerActivity::WifiScannerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                         const std::function<void()>& onExit)
    : Activity("WifiScanner", renderer, mappedInput), onExitCb(onExit) {}

void WifiScannerActivity::onEnter() {
  Activity::onEnter();
  networks.clear();
  selectionIndex = 0;
  statusMessage.clear();
  startScan();
  needsRender = true;
}

void WifiScannerActivity::onExit() {
  Activity::onExit();
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
}

void WifiScannerActivity::startScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
  scanning = true;
}

void WifiScannerActivity::collectResults() {
  const int16_t count = WiFi.scanComplete();
  if (count < 0) {
    return;
  }

  networks.clear();
  networks.reserve(count);
  for (int i = 0; i < count; i++) {
    NetworkEntry entry;
    entry.ssid = WiFi.SSID(i).c_str();
    entry.rssi = WiFi.RSSI(i);
    entry.channel = WiFi.channel(i);
    entry.open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    networks.push_back(entry);
  }
  WiFi.scanDelete();
  scanning = false;
  selectionIndex = 0;
  needsRender = true;
}

void WifiScannerActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExitCb) {
      onExitCb();
    }
    return;
  }

  if (scanning) {
    collectResults();
  } else {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      startScan();
      needsRender = true;
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      saveResults();
      needsRender = true;
      return;
    }
    if (!networks.empty()) {
      if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
        selectionIndex = (selectionIndex - 1 + static_cast<int>(networks.size())) % static_cast<int>(networks.size());
        needsRender = true;
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
        selectionIndex = (selectionIndex + 1) % static_cast<int>(networks.size());
        needsRender = true;
      }
    }
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void WifiScannerActivity::saveResults() {
  if (!SdMan.ready()) {
    statusMessage = "SD card not ready";
    return;
  }
  if (networks.empty()) {
    statusMessage = "No scan results";
    return;
  }

  SdMan.ensureDirectoryExists(kLogDir);
  FsFile logFile = SdMan.open(kLogFile, O_WRITE | O_APPEND | O_CREAT);
  if (!logFile) {
    statusMessage = "Failed to open log";
    return;
  }

  if (logFile.fileSize() == 0) {
    logFile.println("timestamp,ssid,rssi,channel,open");
  }

  const bool hasTime = TimeUtils::isTimeValid();
  const time_t now = time(nullptr);
  for (const auto& net : networks) {
    char line[160];
    const long ts = hasTime ? static_cast<long>(now) : 0L;
    snprintf(line, sizeof(line), "%ld,\"%s\",%d,%d,%s", ts, net.ssid.c_str(), net.rssi, net.channel,
             net.open ? "open" : "secure");
    logFile.println(line);
  }
  logFile.close();
  statusMessage = "Saved to /logs/wifi_scan.csv";
}

void WifiScannerActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  renderer.drawCenteredText(UI_12_FONT_ID, 16, "WiFi Scan");

  if (scanning) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "Scanning...");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Back: Menu");
    renderer.displayBuffer();
    return;
  }

  if (networks.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "No networks found");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Confirm: Rescan   Back: Menu");
    renderer.displayBuffer();
    return;
  }

  const int page = selectionIndex / kItemsPerPage;
  const int start = page * kItemsPerPage;
  const int end = std::min(start + kItemsPerPage, static_cast<int>(networks.size()));
  int y = 70;
  for (int i = start; i < end; i++) {
    const auto& net = networks[i];
    char line[64];
    snprintf(line, sizeof(line), "%s (%ddBm) ch%d %s", net.ssid.c_str(), net.rssi, net.channel,
             net.open ? "open" : "sec");
    if (i == selectionIndex) {
      renderer.fillRect(30, y - 6, renderer.getScreenWidth() - 60, 22, true);
      renderer.drawText(UI_10_FONT_ID, 40, y, line, false);
    } else {
      renderer.drawText(UI_10_FONT_ID, 40, y, line);
    }
    y += 22;
  }

  if (!statusMessage.empty()) {
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 44, statusMessage.c_str());
  }
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                            "Confirm: Rescan   Right: Save   Back: Menu");
  renderer.displayBuffer();
}
