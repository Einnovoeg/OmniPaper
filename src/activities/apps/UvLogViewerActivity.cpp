#include "UvLogViewerActivity.h"

#include <SDCardManager.h>

#include <algorithm>
#include <cstdlib>
#include <ctime>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"

namespace {
constexpr const char* kLogFile = "/logs/uv.csv";
constexpr int kMaxEntries = 200;
constexpr int kItemsPerPage = 8;
}

UvLogViewerActivity::UvLogViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                         const std::function<void()>& onExit)
    : Activity("UvLogViewer", renderer, mappedInput), onExitCb(onExit) {}

void UvLogViewerActivity::onEnter() {
  Activity::onEnter();
  loadLogs();
  needsRender = true;
}

void UvLogViewerActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExitCb) {
      onExitCb();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    loadLogs();
    needsRender = true;
    return;
  }

  if (!entries.empty()) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
      selectionIndex = (selectionIndex - 1 + static_cast<int>(entries.size())) % static_cast<int>(entries.size());
      needsRender = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      selectionIndex = (selectionIndex + 1) % static_cast<int>(entries.size());
      needsRender = true;
    }
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void UvLogViewerActivity::loadLogs() {
  entries.clear();
  selectionIndex = 0;
  statusMessage.clear();

  if (!SdMan.ready()) {
    statusMessage = "SD card not ready";
    return;
  }

  FsFile file = SdMan.open(kLogFile, O_RDONLY);
  if (!file) {
    statusMessage = "No UV log file";
    return;
  }

  std::string line;
  line.reserve(128);
  while (file.available()) {
    char c = 0;
    if (file.read(&c, 1) != 1) {
      break;
    }
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      if (!line.empty()) {
        UvLogEntry entry;
        if (parseLine(line, entry)) {
          entries.push_back(entry);
          if (static_cast<int>(entries.size()) > kMaxEntries) {
            entries.erase(entries.begin());
          }
        }
      }
      line.clear();
      continue;
    }
    if (line.size() < 180) {
      line.push_back(c);
    }
  }
  if (!line.empty()) {
    UvLogEntry entry;
    if (parseLine(line, entry)) {
      entries.push_back(entry);
      if (static_cast<int>(entries.size()) > kMaxEntries) {
        entries.erase(entries.begin());
      }
    }
  }
  file.close();

  if (entries.empty()) {
    statusMessage = "No UV samples";
    return;
  }
  selectionIndex = static_cast<int>(entries.size()) - 1;
}

bool UvLogViewerActivity::parseLine(const std::string& line, UvLogEntry& out) const {
  if (line.empty() || line.rfind("timestamp,", 0) == 0) {
    return false;
  }

  std::vector<std::string> parts;
  std::string current;
  current.reserve(24);
  for (char c : line) {
    if (c == ',') {
      parts.push_back(current);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  parts.push_back(current);
  if (parts.size() < 6) {
    return false;
  }

  char* end = nullptr;
  out.timestamp = strtol(parts[0].c_str(), &end, 10);
  if (!end || *end != '\0') {
    return false;
  }
  out.addr = parts[1];
  out.uva = strtof(parts[2].c_str(), nullptr);
  out.uvb = strtof(parts[3].c_str(), nullptr);
  out.uvc = strtof(parts[4].c_str(), nullptr);
  out.tempC = strtof(parts[5].c_str(), nullptr);
  return true;
}

void UvLogViewerActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  renderer.drawCenteredText(UI_12_FONT_ID, 16, "UV Log Viewer");

  if (entries.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, statusMessage.c_str());
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                              "Confirm: Refresh   Back: Menu");
    renderer.displayBuffer();
    return;
  }

  const auto& selected = entries[selectionIndex];
  char details[128];
  snprintf(details, sizeof(details), "[%d/%d] %s UVA %.1f UVB %.1f UVC %.1f T %.1fC", selectionIndex + 1,
           static_cast<int>(entries.size()), selected.addr.c_str(), selected.uva, selected.uvb, selected.uvc,
           selected.tempC);
  renderer.drawText(SMALL_FONT_ID, 20, 40, details);

  char tsLine[48];
  if (selected.timestamp > 0) {
    std::tm tm {};
    const time_t ts = static_cast<time_t>(selected.timestamp);
    gmtime_r(&ts, &tm);
    snprintf(tsLine, sizeof(tsLine), "UTC %04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
  } else {
    snprintf(tsLine, sizeof(tsLine), "UTC unknown");
  }
  renderer.drawText(SMALL_FONT_ID, 20, 58, tsLine);

  const int page = selectionIndex / kItemsPerPage;
  const int start = page * kItemsPerPage;
  const int end = std::min(start + kItemsPerPage, static_cast<int>(entries.size()));
  int y = 86;
  for (int i = start; i < end; i++) {
    const auto& e = entries[i];
    char line[80];
    snprintf(line, sizeof(line), "%03d %s U%.1f B%.1f C%.1f", i + 1, e.addr.c_str(), e.uva, e.uvb, e.uvc);
    if (i == selectionIndex) {
      renderer.fillRect(20, y - 4, renderer.getScreenWidth() - 40, 20, true);
      renderer.drawText(UI_10_FONT_ID, 28, y, line, false);
    } else {
      renderer.drawText(UI_10_FONT_ID, 28, y, line);
    }
    y += 20;
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                            "Up/Down: Select   Confirm: Refresh   Back: Menu");
  renderer.displayBuffer();
}
