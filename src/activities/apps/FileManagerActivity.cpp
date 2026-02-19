#include "FileManagerActivity.h"

#include <JpegToBmpConverter.h>
#include <SDCardManager.h>

#include <algorithm>
#include <cstring>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "activities/apps/ImageViewerActivity.h"
#include "fontIds.h"

namespace {
constexpr int kItemsPerPage = 12;
constexpr const char* kTempDir = "/.crosspoint/tmp";
constexpr const char* kTempJpegBmp = "/.crosspoint/tmp/jpg_view.bmp";

bool endsWithIgnoreCase(const std::string& name, const char* ext) {
  const size_t extLen = strlen(ext);
  if (name.size() < extLen) return false;
  std::string tail = name.substr(name.size() - extLen);
  std::string target = ext;
  std::transform(tail.begin(), tail.end(), tail.begin(), ::tolower);
  std::transform(target.begin(), target.end(), target.begin(), ::tolower);
  return tail == target;
}

bool convertJpegToBmp(const std::string& srcPath, const int targetWidth, const int targetHeight,
                      std::string& errorOut) {
  if (!SdMan.ready()) {
    errorOut = "SD card not ready";
    return false;
  }
  SdMan.ensureDirectoryExists(kTempDir);

  FsFile jpegFile;
  if (!SdMan.openFileForRead("IMG", srcPath, jpegFile)) {
    errorOut = "Failed to open JPG";
    return false;
  }

  FsFile bmpFile;
  if (!SdMan.openFileForWrite("IMG", kTempJpegBmp, bmpFile)) {
    jpegFile.close();
    errorOut = "Failed to create BMP";
    return false;
  }

  const bool ok =
      JpegToBmpConverter::jpegFileToBmpStreamWithSize(jpegFile, bmpFile, targetWidth, targetHeight);
  jpegFile.close();
  bmpFile.close();

  if (!ok) {
    errorOut = "JPEG convert failed";
  }
  return ok;
}
}  // namespace

FileManagerActivity::FileManagerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                         const std::function<void()>& onExit, OpenReaderCallback onOpenReader)
    : ActivityWithSubactivity("FileManager", renderer, mappedInput),
      onExit(onExit),
      onOpenReader(std::move(onOpenReader)) {}

void FileManagerActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
  currentPath = "/";
  selectionIndex = 0;
  loadDirectory();
  needsRender = true;
}

void FileManagerActivity::loop() {
  if (subActivity) {
    ActivityWithSubactivity::loop();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (currentPath == "/") {
      if (onExit) {
        onExit();
      }
    } else {
      goUp();
      loadDirectory();
      needsRender = true;
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    if (!entries.empty()) {
      selectionIndex = (selectionIndex - 1 + static_cast<int>(entries.size())) % static_cast<int>(entries.size());
      needsRender = true;
    }
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    if (!entries.empty()) {
      selectionIndex = (selectionIndex + 1) % static_cast<int>(entries.size());
      needsRender = true;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    openSelected();
    return;
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void FileManagerActivity::loadDirectory() {
  entries.clear();
  statusMessage.clear();

  if (!SdMan.ready()) {
    statusMessage = "SD card not ready";
    return;
  }

  FsFile dir = SdMan.open(currentPath.c_str());
  if (!dir || !dir.isDirectory()) {
    statusMessage = "Invalid directory";
    return;
  }

  for (FsFile entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    char name[128];
    entry.getName(name, sizeof(name));
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      entry.close();
      continue;
    }
    Entry item;
    item.name = name;
    item.isDir = entry.isDirectory();
    item.size = entry.fileSize();
    entries.push_back(item);
    entry.close();
  }
  dir.close();

  std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
    if (a.isDir != b.isDir) return a.isDir > b.isDir;
    return a.name < b.name;
  });

  if (selectionIndex >= static_cast<int>(entries.size())) {
    selectionIndex = 0;
  }
}

void FileManagerActivity::openSelected() {
  if (entries.empty()) {
    return;
  }
  const auto& entry = entries[selectionIndex];
  const std::string path = joinPath(currentPath, entry.name);

  if (entry.isDir) {
    currentPath = path;
    selectionIndex = 0;
    loadDirectory();
    needsRender = true;
    return;
  }

  if (endsWithIgnoreCase(entry.name, ".bmp")) {
    std::vector<String> files;
    files.emplace_back(path.c_str());
    enterNewActivity(new ImageViewerActivity(
        renderer, mappedInput, std::move(files), 0,
        [this]() {
          exitActivity();
          needsRender = true;
        }));
    return;
  }

  if (endsWithIgnoreCase(entry.name, ".jpg") || endsWithIgnoreCase(entry.name, ".jpeg")) {
    statusMessage = "Converting JPEG...";
    render();
    std::string error;
    const bool ok = convertJpegToBmp(path, renderer.getScreenWidth(), renderer.getScreenHeight(), error);
    if (!ok) {
      statusMessage = error;
      needsRender = true;
      return;
    }
    std::vector<String> files;
    files.emplace_back(kTempJpegBmp);
    enterNewActivity(new ImageViewerActivity(
        renderer, mappedInput, std::move(files), 0,
        [this]() {
          exitActivity();
          needsRender = true;
        }));
    return;
  }

  if (endsWithIgnoreCase(entry.name, ".epub") || endsWithIgnoreCase(entry.name, ".txt") ||
      endsWithIgnoreCase(entry.name, ".xtc") || endsWithIgnoreCase(entry.name, ".xtch")) {
    if (onOpenReader) {
      onOpenReader(path);
    }
    return;
  }

  statusMessage = "No viewer for this file";
  needsRender = true;
}

void FileManagerActivity::goUp() {
  if (currentPath == "/") {
    return;
  }
  const auto pos = currentPath.find_last_of('/');
  if (pos == std::string::npos || pos == 0) {
    currentPath = "/";
  } else {
    currentPath = currentPath.substr(0, pos);
  }
  if (currentPath.empty()) {
    currentPath = "/";
  }
}

std::string FileManagerActivity::joinPath(const std::string& base, const std::string& name) const {
  if (base == "/") {
    return base + name;
  }
  return base + "/" + name;
}

void FileManagerActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "File Manager");
  renderer.drawText(SMALL_FONT_ID, 20, 40, currentPath.c_str());

  if (!statusMessage.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2, statusMessage.c_str());
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Back: Up/Exit");
    renderer.displayBuffer();
    return;
  }

  if (entries.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2, "No files");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Back: Up/Exit");
    renderer.displayBuffer();
    return;
  }

  const int page = selectionIndex / kItemsPerPage;
  const int start = page * kItemsPerPage;
  const int end = std::min(start + kItemsPerPage, static_cast<int>(entries.size()));
  int y = 70;
  for (int i = start; i < end; i++) {
    const bool selected = (i == selectionIndex);
    const auto& entry = entries[i];
    std::string label = entry.isDir ? "[D] " + entry.name : entry.name;
    if (selected) {
      renderer.fillRect(30, y - 6, renderer.getScreenWidth() - 60, 22, true);
      renderer.drawText(UI_10_FONT_ID, 40, y, label.c_str(), false);
    } else {
      renderer.drawText(UI_10_FONT_ID, 40, y, label.c_str());
    }
    y += 22;
  }

  char footer[48];
  snprintf(footer, sizeof(footer), "%d/%d", selectionIndex + 1, static_cast<int>(entries.size()));
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, footer);
  renderer.displayBuffer();
}
