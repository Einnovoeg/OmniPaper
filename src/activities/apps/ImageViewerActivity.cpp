#include "ImageViewerActivity.h"

#include <SDCardManager.h>

#include <Bitmap.h>
#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "PaperS3Ui.h"
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
#if defined(PLATFORM_M5PAPERS3)
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasTapped() && PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), tapX, tapY)) {
    if (PaperS3Ui::backButtonRect(renderer).contains(tapX, tapY)) {
      if (onExit) {
        onExit();
      }
      return;
    }

    if (!imageFiles.empty()) {
      if (PaperS3Ui::sideActionRect(renderer, false).contains(tapX, tapY)) {
        currentIndex = (currentIndex - 1 + static_cast<int>(imageFiles.size())) % static_cast<int>(imageFiles.size());
        needsRender = true;
        return;
      }
      if (PaperS3Ui::sideActionRect(renderer, true).contains(tapX, tapY)) {
        currentIndex = (currentIndex + 1) % static_cast<int>(imageFiles.size());
        needsRender = true;
        return;
      }
    }
  }
#endif

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
#if defined(PLATFORM_M5PAPERS3)
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  PaperS3Ui::drawScreenHeader(renderer, "Image Viewer");
  PaperS3Ui::drawBackButton(renderer);

  if (imageFiles.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "No /images/*.bmp found");
    PaperS3Ui::drawFooter(renderer, "Copy BMP files to /images on the SD card");
    renderer.displayBuffer();
    return;
  }

  renderer.drawRect(24, 108, 492, 720);
  renderImage(imageFiles[currentIndex]);
  PaperS3Ui::drawSideActionButton(renderer, false, "Prev");
  PaperS3Ui::drawSideActionButton(renderer, true, "Next");

  char footer[48];
  snprintf(footer, sizeof(footer), "%d / %d", currentIndex + 1, static_cast<int>(imageFiles.size()));
  PaperS3Ui::drawFooter(renderer, footer);
  renderer.displayBuffer();
  return;
#else
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
#endif
}

void ImageViewerActivity::renderImage(const String& path) {
  FsFile file;
  if (!SdMan.openFileForRead("IMG", path, file)) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "Failed to open image");
    return;
  }

  Bitmap bitmap(file, true);
  if (bitmap.parseHeaders() != BmpReaderError::Ok) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "Unsupported BMP");
    file.close();
    return;
  }

#if defined(PLATFORM_M5PAPERS3)
  renderer.drawBitmap(bitmap, 36, 120, 468, 696);
#else
  renderer.drawBitmap(bitmap, 0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
#endif
  file.close();
}
