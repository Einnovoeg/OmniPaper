#include "BleScannerActivity.h"

#include <BLEDevice.h>
#include <SDCardManager.h>

#include <algorithm>
#include <ctime>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"
#include "util/TimeUtils.h"

namespace {
constexpr int kItemsPerPage = 10;
constexpr int kMaxDevices = 60;
constexpr int kMaxServicesPerDevice = 6;
constexpr const char* kLogDir = "/logs";
constexpr const char* kLogFile = "/logs/ble_scan.csv";

std::string sanitizeCsvField(const std::string& input) {
  std::string out = input;
  for (auto& ch : out) {
    if (ch == '"') {
      ch = '\'';
    }
  }
  return out;
}

std::string joinServices(const std::vector<std::string>& services) {
  std::string joined;
  for (size_t i = 0; i < services.size(); i++) {
    if (i > 0) {
      joined += ';';
    }
    joined += services[i];
  }
  return joined;
}
}  // namespace

BleScannerActivity::BleScannerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                       const std::function<void()>& onExit)
    : Activity("BleScanner", renderer, mappedInput), onExitCb(onExit) {}

void BleScannerActivity::onEnter() {
  Activity::onEnter();
  BLEDevice::init("");
  devices.clear();
  visibleIndices.clear();
  selectionIndex = 0;
  showDetail = false;
  filterNamed = false;
  statusMessage.clear();
  startScan();
  needsRender = true;
}

void BleScannerActivity::startScan() {
  scanning = true;
  needsRender = true;
  statusMessage.clear();
}

void BleScannerActivity::performScan() {
  BLEScan* scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  BLEScanResults results = scan->start(4, false);

  devices.clear();
  const int count = results.getCount();
  const int toStore = std::min(count, kMaxDevices);
  devices.reserve(toStore);
  for (int i = 0; i < toStore; i++) {
    BLEAdvertisedDevice dev = results.getDevice(i);
    BleEntry entry;
    entry.name = dev.haveName() ? dev.getName() : "Unknown";
    entry.address = dev.getAddress().toString();
    entry.rssi = dev.getRSSI();
    if (dev.haveManufacturerData()) {
      entry.mfgDataLen = static_cast<int>(dev.getManufacturerData().length());
    }
    if (dev.haveServiceUUID()) {
      const int svcCount = dev.getServiceUUIDCount();
      const int toStore = std::min(svcCount, kMaxServicesPerDevice);
      entry.services.reserve(toStore);
      for (int s = 0; s < toStore; s++) {
        entry.services.push_back(dev.getServiceUUID(s).toString());
      }
    }
    devices.push_back(entry);
  }
  scan->clearResults();
  rebuildVisible();
  scanning = false;
  selectionIndex = 0;
  needsRender = true;
}

void BleScannerActivity::rebuildVisible() {
  visibleIndices.clear();
  for (size_t i = 0; i < devices.size(); i++) {
    if (filterNamed && (devices[i].name.empty() || devices[i].name == "Unknown")) {
      continue;
    }
    visibleIndices.push_back(static_cast<int>(i));
  }
  if (visibleIndices.empty()) {
    selectionIndex = 0;
    showDetail = false;
    return;
  }
  if (selectionIndex >= static_cast<int>(visibleIndices.size())) {
    selectionIndex = 0;
  }
}

void BleScannerActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (showDetail) {
      showDetail = false;
      needsRender = true;
      return;
    }
    if (onExitCb) {
      onExitCb();
    }
    return;
  }

  if (scanning) {
    performScan();
    return;
  }

  if (!showDetail) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      startScan();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
      filterNamed = !filterNamed;
      rebuildVisible();
      needsRender = true;
    }
    if (!visibleIndices.empty()) {
      if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
        selectionIndex = (selectionIndex - 1 + static_cast<int>(visibleIndices.size())) %
                         static_cast<int>(visibleIndices.size());
        needsRender = true;
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
        selectionIndex = (selectionIndex + 1) % static_cast<int>(visibleIndices.size());
        needsRender = true;
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
        showDetail = true;
        needsRender = true;
      }
    }
  } else {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      saveSelected();
      needsRender = true;
    }
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void BleScannerActivity::saveSelected() {
  if (!SdMan.ready()) {
    statusMessage = "SD card not ready";
    return;
  }
  if (devices.empty() || visibleIndices.empty()) {
    statusMessage = "No device selected";
    return;
  }

  SdMan.ensureDirectoryExists(kLogDir);
  FsFile logFile = SdMan.open(kLogFile, O_WRITE | O_APPEND | O_CREAT);
  if (!logFile) {
    statusMessage = "Failed to open log";
    return;
  }

  if (logFile.fileSize() == 0) {
    logFile.println("timestamp,name,address,rssi,mfg_len,services");
  }

  const auto& dev = devices[visibleIndices[selectionIndex]];
  const bool hasTime = TimeUtils::isTimeValid();
  const time_t now = time(nullptr);
  const long ts = hasTime ? static_cast<long>(now) : 0L;

  std::string services = joinServices(dev.services);
  if (services.size() > 120) {
    services.resize(120);
  }

  const std::string safeName = sanitizeCsvField(dev.name);
  const std::string safeAddr = sanitizeCsvField(dev.address);
  const std::string safeServices = sanitizeCsvField(services);

  char line[256];
  snprintf(line, sizeof(line), "%ld,\"%s\",\"%s\",%d,%d,\"%s\"", ts, safeName.c_str(), safeAddr.c_str(), dev.rssi,
           dev.mfgDataLen, safeServices.c_str());
  logFile.println(line);
  logFile.close();
  statusMessage = "Saved to /logs/ble_scan.csv";
}

void BleScannerActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  renderer.drawCenteredText(UI_12_FONT_ID, 16, "BLE Scan");

  if (scanning) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "Scanning...");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Back: Menu");
    renderer.displayBuffer();
    return;
  }

  if (devices.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "No devices found");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Confirm: Rescan   Back: Menu");
    renderer.displayBuffer();
    return;
  }
  if (visibleIndices.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "No devices match filter");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                              "Left: Toggle Filter   Confirm: Rescan   Back: Menu");
    renderer.displayBuffer();
    return;
  }

  if (!showDetail) {
    const int page = selectionIndex / kItemsPerPage;
    const int start = page * kItemsPerPage;
    const int end = std::min(start + kItemsPerPage, static_cast<int>(visibleIndices.size()));
    int y = 70;
    for (int i = start; i < end; i++) {
      const auto& dev = devices[visibleIndices[i]];
      char line[64];
      snprintf(line, sizeof(line), "%s (%ddBm)", dev.name.c_str(), dev.rssi);
      if (i == selectionIndex) {
        renderer.fillRect(30, y - 6, renderer.getScreenWidth() - 60, 22, true);
        renderer.drawText(UI_10_FONT_ID, 40, y, line, false);
      } else {
        renderer.drawText(UI_10_FONT_ID, 40, y, line);
      }
      y += 22;
    }
    const char* filterLabel = filterNamed ? "Filter: Named" : "Filter: All";
    if (!statusMessage.empty()) {
      renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 64, statusMessage.c_str());
    }
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 44, filterLabel);
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                              "Confirm: Rescan   Left: Filter   Right: Details   Back: Menu");
    renderer.displayBuffer();
    return;
  }

  const auto& dev = devices[visibleIndices[selectionIndex]];
  int y = 60;
  renderer.drawText(UI_10_FONT_ID, 40, y, dev.name.c_str());
  y += 20;
  renderer.drawText(UI_10_FONT_ID, 40, y, dev.address.c_str());
  y += 20;
  char rssiLine[32];
  snprintf(rssiLine, sizeof(rssiLine), "RSSI: %d dBm", dev.rssi);
  renderer.drawText(UI_10_FONT_ID, 40, y, rssiLine);
  y += 20;
  char mfgLine[32];
  snprintf(mfgLine, sizeof(mfgLine), "MFG Data: %d bytes", dev.mfgDataLen);
  renderer.drawText(UI_10_FONT_ID, 40, y, mfgLine);
  y += 24;

  if (!dev.services.empty()) {
    renderer.drawText(UI_10_FONT_ID, 40, y, "Services:");
    y += 18;
    for (const auto& svc : dev.services) {
      renderer.drawText(UI_10_FONT_ID, 40, y, svc.c_str());
      y += 18;
      if (y > renderer.getScreenHeight() - 60) break;
    }
  }

  if (!statusMessage.empty()) {
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 44, statusMessage.c_str());
  }
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Confirm: Save   Back: List");
  renderer.displayBuffer();
}
