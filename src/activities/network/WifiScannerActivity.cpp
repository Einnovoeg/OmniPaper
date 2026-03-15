#include "WifiScannerActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <WiFi.h>

#include <ctime>

#include "MappedInputManager.h"
#include "activities/apps/PaperS3Ui.h"
#include "fontIds.h"
#include "util/TimeUtils.h"

namespace {
constexpr int kItemsPerPage = 12;
constexpr int kPaperS3ItemsPerPage = 6;
constexpr const char* kLogDir = "/logs";
constexpr const char* kLogFile = "/logs/wifi_scan.csv";
}  // namespace

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
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_OFF);
  }
}

void WifiScannerActivity::startScan() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(false, true);
  WiFi.scanDelete();
  delay(150);
#if defined(PLATFORM_M5PAPERS3)
  scanning = true;
  needsRender = true;
  render();
  needsRender = false;

  const int16_t count = WiFi.scanNetworks(false, true);
  networks.clear();
  if (count > 0) {
    networks.reserve(count);
    for (int i = 0; i < count; i++) {
      NetworkEntry entry;
      entry.ssid = WiFi.SSID(i).c_str();
      entry.rssi = WiFi.RSSI(i);
      entry.channel = WiFi.channel(i);
      entry.open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
      networks.push_back(entry);
    }
  }
  WiFi.scanDelete();
  scanning = false;
  selectionIndex = 0;
  statusMessage = count < 0 ? "Scan failed" : "";
  needsRender = true;
#else
  WiFi.scanNetworks(true, true);
  scanning = true;
#endif
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
#if defined(PLATFORM_M5PAPERS3)
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasTapped() &&
      PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), tapX, tapY)) {
    if (PaperS3Ui::backButtonRect(renderer).contains(tapX, tapY)) {
      if (onExitCb) {
        onExitCb();
      }
      return;
    }

    if (!scanning) {
      const int page = selectionIndex / kPaperS3ItemsPerPage;
      const int start = page * kPaperS3ItemsPerPage;
      const int end = std::min(start + kPaperS3ItemsPerPage, static_cast<int>(networks.size()));
      for (int i = start; i < end; i++) {
        if (PaperS3Ui::listRowRect(renderer, i - start).contains(tapX, tapY)) {
          selectionIndex = i;
          needsRender = true;
          return;
        }
      }

      if (PaperS3Ui::sideActionRect(renderer, false).contains(tapX, tapY)) {
        saveResults();
        needsRender = true;
        return;
      }
      if (PaperS3Ui::primaryActionRect(renderer).contains(tapX, tapY)) {
        startScan();
        needsRender = true;
        return;
      }
    }
  }
#endif

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
#if defined(PLATFORM_M5PAPERS3)
    if (mappedInput.wasPressed(MappedInputManager::Button::Up) && !networks.empty()) {
      const int page = selectionIndex / kPaperS3ItemsPerPage;
      if (page > 0) {
        selectionIndex = (page - 1) * kPaperS3ItemsPerPage;
        needsRender = true;
      }
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Down) && !networks.empty()) {
      const int page = selectionIndex / kPaperS3ItemsPerPage;
      const int nextStart = (page + 1) * kPaperS3ItemsPerPage;
      if (nextStart < static_cast<int>(networks.size())) {
        selectionIndex = nextStart;
        needsRender = true;
      }
    }
#else
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
#endif
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
#if defined(PLATFORM_M5PAPERS3)
  {
    renderer.setOrientation(GfxRenderer::Orientation::Portrait);
    PaperS3Ui::drawScreenHeader(renderer, "WiFi Scan", scanning ? "Scanning" : "Nearby networks");
    PaperS3Ui::drawBackButton(renderer);

    if (scanning) {
      renderer.drawCenteredText(UI_12_FONT_ID, 320, "Scanning...");
      PaperS3Ui::drawFooter(renderer, "Tap Back to cancel");
      renderer.displayBuffer();
      return;
    }

    if (networks.empty()) {
      renderer.drawCenteredText(UI_12_FONT_ID, 320, statusMessage.empty() ? "No networks found" : statusMessage.c_str());
      PaperS3Ui::drawPrimaryActionButton(renderer, "Rescan");
      PaperS3Ui::drawFooter(renderer, "Touch Rescan to try again");
      renderer.displayBuffer();
      return;
    }

    const int page = selectionIndex / kPaperS3ItemsPerPage;
    const int start = page * kPaperS3ItemsPerPage;
    const int end = std::min(start + kPaperS3ItemsPerPage, static_cast<int>(networks.size()));
    for (int i = start; i < end; i++) {
      char subtitle[64];
      snprintf(subtitle, sizeof(subtitle), "%ddBm  ch%d  %s", networks[i].rssi, networks[i].channel,
               networks[i].open ? "open" : "secure");
      PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, i - start), i == selectionIndex,
                             networks[i].ssid.c_str(), "", subtitle);
    }

    PaperS3Ui::drawSideActionButton(renderer, false, "Save");
    PaperS3Ui::drawPrimaryActionButton(renderer, "Rescan");
    renderer.drawSideButtonHints(UI_10_FONT_ID, start > 0 ? "Prev" : "",
                                 end < static_cast<int>(networks.size()) ? "Next" : "");
    if (!statusMessage.empty()) {
      PaperS3Ui::drawFooterStatus(renderer, statusMessage.c_str());
    }
    char footer[48];
    snprintf(footer, sizeof(footer), "Showing %d-%d of %d", start + 1, end, static_cast<int>(networks.size()));
    PaperS3Ui::drawFooter(renderer, footer);
    renderer.displayBuffer();
    return;
  }
#endif

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
