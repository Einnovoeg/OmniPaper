#include "ToolsOtaUpdateActivity.h"

#include <Arduino.h>
#include <SDCardManager.h>
#include <Update.h>
#include <WiFi.h>

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"

namespace {
constexpr const char* kUpdateDir = "/update";

bool hasPrefix(const std::string& value, const char* prefix) {
  return value.rfind(prefix, 0) == 0;
}
}  // namespace

ToolsOtaUpdateActivity::ToolsOtaUpdateActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                               const std::function<void()>& onExit)
    : ActivityWithSubactivity("OtaUpdate", renderer, mappedInput), onExit(onExit) {}

void ToolsOtaUpdateActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
  state = State::Menu;
  menuIndex = 0;
  fileIndex = 0;
  sdFiles.clear();
  statusMessage.clear();
  pendingUrl.clear();
  updateOk = false;
  needsRender = true;
}

void ToolsOtaUpdateActivity::loop() {
  if (subActivity) {
    ActivityWithSubactivity::loop();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (state == State::Menu) {
      if (onExit) onExit();
    } else {
      state = State::Menu;
      needsRender = true;
    }
    return;
  }

  if (state == State::Menu) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
      menuIndex = (menuIndex - 1 + 2) % 2;
      needsRender = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      menuIndex = (menuIndex + 1) % 2;
      needsRender = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      if (menuIndex == 0) {
        loadSdFiles();
        state = State::SdList;
        needsRender = true;
      } else {
        startUrlUpdate();
      }
    }
  } else if (state == State::SdList) {
    if (sdFiles.empty()) {
      if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
        state = State::Menu;
        needsRender = true;
      }
    } else {
      if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
        fileIndex = (fileIndex - 1 + static_cast<int>(sdFiles.size())) % static_cast<int>(sdFiles.size());
        needsRender = true;
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
        fileIndex = (fileIndex + 1) % static_cast<int>(sdFiles.size());
        needsRender = true;
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
        startSdUpdate();
      }
    }
  } else if (state == State::Result) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      state = State::Menu;
      needsRender = true;
    }
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void ToolsOtaUpdateActivity::loadSdFiles() {
  sdFiles.clear();
  if (!SdMan.ready()) {
    statusMessage = "SD card not ready";
    return;
  }
  FsFile dir = SdMan.open(kUpdateDir);
  if (!dir || !dir.isDirectory()) {
    statusMessage = "No /update directory";
    return;
  }
  for (FsFile entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    char name[128];
    entry.getName(name, sizeof(name));
    std::string n = name;
    if (!entry.isDirectory() && n.size() >= 4 && n.substr(n.size() - 4) == ".bin") {
      sdFiles.emplace_back(std::string(kUpdateDir) + "/" + n);
    }
    entry.close();
  }
  dir.close();
  fileIndex = 0;
  if (sdFiles.empty()) {
    statusMessage = "No .bin found in /update";
  } else {
    statusMessage.clear();
  }
}

void ToolsOtaUpdateActivity::startSdUpdate() {
  if (sdFiles.empty()) return;
  state = State::Updating;
  statusMessage = "Updating from SD...";
  needsRender = true;

  std::string error;
  updateOk = updateFromFile(sdFiles[fileIndex], error);
  statusMessage = updateOk ? "Update OK. Rebooting..." : error;
  state = State::Result;
  needsRender = true;
  if (updateOk) {
    delay(1500);
    ESP.restart();
  }
}

void ToolsOtaUpdateActivity::startUrlUpdate() {
  if (WiFi.status() == WL_CONNECTED) {
    launchUrlEntry();
  } else {
    launchWifiSelection();
  }
}

void ToolsOtaUpdateActivity::launchWifiSelection() {
  enterNewActivity(new WifiSelectionActivity(renderer, mappedInput,
                                             [this](const bool connected) {
                                               exitActivity();
                                               if (connected) {
                                                 launchUrlEntry();
                                               } else {
                                                 statusMessage = "WiFi connection failed";
                                                 state = State::Result;
                                                 needsRender = true;
                                               }
                                             }));
}

void ToolsOtaUpdateActivity::launchUrlEntry() {
  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, "OTA URL", pendingUrl, 12, 0, false,
      [this](const std::string& url) {
        pendingUrl = url;
        exitActivity();
        state = State::Updating;
        statusMessage = "Updating from URL...";
        needsRender = true;

        std::string error;
        updateOk = updateFromUrl(pendingUrl, error);
        statusMessage = updateOk ? "Update OK. Rebooting..." : error;
        state = State::Result;
        needsRender = true;
        if (updateOk) {
          delay(1500);
          ESP.restart();
        }
      },
      [this]() {
        exitActivity();
        state = State::Menu;
        needsRender = true;
      }));
}

bool ToolsOtaUpdateActivity::updateFromFile(const std::string& path, std::string& error) {
  if (!SdMan.ready()) {
    error = "SD card not ready";
    return false;
  }
  FsFile file;
  if (!SdMan.openFileForRead("OTA", path, file)) {
    error = "Failed to open file";
    return false;
  }

  const size_t size = file.fileSize();
  if (!Update.begin(size)) {
    error = "Update begin failed";
    file.close();
    return false;
  }

  const size_t written = Update.writeStream(file);
  file.close();
  if (written != size) {
    error = "Write failed";
    Update.abort();
    return false;
  }
  if (!Update.end()) {
    error = "Finalize failed";
    return false;
  }
  if (!Update.isFinished()) {
    error = "Incomplete update";
    return false;
  }
  return true;
}

bool ToolsOtaUpdateActivity::updateFromUrl(const std::string& url, std::string& error) {
  if (url.empty()) {
    error = "URL empty";
    return false;
  }

  HTTPClient http;
  WiFiClientSecure secure;

  if (hasPrefix(url, "https://")) {
    secure.setInsecure();
    if (!http.begin(secure, url.c_str())) {
      error = "HTTP begin failed";
      return false;
    }
  } else {
    if (!http.begin(url.c_str())) {
      error = "HTTP begin failed";
      return false;
    }
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    error = "HTTP error";
    return false;
  }

  const int len = http.getSize();
  if (!Update.begin(len > 0 ? static_cast<size_t>(len) : UPDATE_SIZE_UNKNOWN)) {
    http.end();
    error = "Update begin failed";
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  const size_t written = Update.writeStream(*stream);
  http.end();

  if (len > 0 && written != static_cast<size_t>(len)) {
    error = "Write failed";
    Update.abort();
    return false;
  }
  if (!Update.end()) {
    error = "Finalize failed";
    return false;
  }
  if (!Update.isFinished()) {
    error = "Incomplete update";
    return false;
  }
  return true;
}

void ToolsOtaUpdateActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "OTA Update");

  if (state == State::Menu) {
    const char* items[2] = {"From SD", "From URL"};
    int y = 80;
    for (int i = 0; i < 2; i++) {
      if (i == menuIndex) {
        renderer.fillRect(40, y - 6, renderer.getScreenWidth() - 80, 24, true);
        renderer.drawText(UI_12_FONT_ID, 50, y, items[i], false);
      } else {
        renderer.drawText(UI_12_FONT_ID, 50, y, items[i]);
      }
      y += 28;
    }
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Confirm: Select   Back: Menu");
    renderer.displayBuffer();
    return;
  }

  if (state == State::SdList) {
    if (!statusMessage.empty() && sdFiles.empty()) {
      renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2, statusMessage.c_str());
      renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Confirm: Back");
      renderer.displayBuffer();
      return;
    }
    int y = 70;
    for (size_t i = 0; i < sdFiles.size(); i++) {
      const bool selected = static_cast<int>(i) == fileIndex;
      const std::string name = sdFiles[i].substr(sdFiles[i].find_last_of('/') + 1);
      if (selected) {
        renderer.fillRect(30, y - 6, renderer.getScreenWidth() - 60, 24, true);
        renderer.drawText(UI_10_FONT_ID, 40, y, name.c_str(), false);
      } else {
        renderer.drawText(UI_10_FONT_ID, 40, y, name.c_str());
      }
      y += 24;
    }
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Confirm: Update   Back: Menu");
    renderer.displayBuffer();
    return;
  }

  if (state == State::Updating) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, statusMessage.c_str());
    renderer.displayBuffer();
    return;
  }

  if (state == State::Result) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, statusMessage.c_str());
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Confirm: Back");
    renderer.displayBuffer();
    return;
  }
}
