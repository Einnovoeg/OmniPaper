#pragma once

#include <functional>
#include <vector>

#include "../Activity.h"

class ImageViewerActivity final : public Activity {
 public:
 ImageViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);
 ImageViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::vector<String> files, int startIndex,
                     const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
 std::function<void()> onExit;
 std::vector<String> imageFiles;
 int currentIndex = 0;
 bool needsRender = true;
  bool useProvidedList = false;

  void loadImages();
  void render();
  void renderImage(const String& path);
};
