#include "ImageViewerActivity.h"

#include <SDCardManager.h>

#include <Bitmap.h>
#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"

namespace {
const char* kImageDir = "/images";

bool endsWithIgnoreCase(const String& name, const char* ext) {
  String lower = name;
  lower.toLowerCase();
  String target = ext;
  target.toLowerCase();
  return lower.endsWith(target);
}
}

ImageViewerActivity::ImageViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                         const std::function<void()>& onExit)
    : Activity("ImageViewer", renderer, mappedInput), onExit(onExit) {}

ImageViewerActivity::ImageViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                         std::vector<String> files, const int startIndex,
                                         const std::function<void()>& onExit)
    : Activity("ImageViewer", renderer, mappedInput),
      onExit(onExit),
      imageFiles(std::move(files)),
      currentIndex(startIndex),
      useProvidedList(true) {}

void ImageViewerActivity::onEnter() {
  Activity::onEnter();
  if (!useProvidedList) {
    loadImages();
    currentIndex = 0;
  } else if (!imageFiles.empty()) {
    if (currentIndex < 0) currentIndex = 0;
    if (currentIndex >= static_cast<int>(imageFiles.size())) {
      currentIndex = static_cast<int>(imageFiles.size()) - 1;
    }
  }
  needsRender = true;
}

void ImageViewerActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExit) {
      onExit();
    }
    return;
  }

  if (!imageFiles.empty()) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
      currentIndex = (currentIndex - 1 + static_cast<int>(imageFiles.size())) % static_cast<int>(imageFiles.size());
      needsRender = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      currentIndex = (currentIndex + 1) % static_cast<int>(imageFiles.size());
      needsRender = true;
    }
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void ImageViewerActivity::loadImages() {
  imageFiles.clear();
  if (!SdMan.ready()) {
    return;
  }

  auto files = SdMan.listFiles(kImageDir, 200);
  for (const auto& file : files) {
    if (endsWithIgnoreCase(file, ".bmp")) {
      imageFiles.push_back(String(kImageDir) + "/" + file);
    }
  }
}

void ImageViewerActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);

  if (imageFiles.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "No /images/*.bmp found");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Back: Menu");
    renderer.displayBuffer();
    return;
  }

  renderImage(imageFiles[currentIndex]);

  String footer = String(currentIndex + 1) + "/" + String(imageFiles.size());
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, footer.c_str());
  renderer.displayBuffer();
}

void ImageViewerActivity::renderImage(const String& path) {
  FsFile file;
  if (!SdMan.openFileForRead("IMG", path, file)) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "Failed to open image");
    return;
  }

  Bitmap bitmap(file, true);
  renderer.drawBitmap(bitmap, 0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
  file.close();
}
