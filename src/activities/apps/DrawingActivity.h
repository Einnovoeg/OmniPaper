#pragma once

#include <functional>

#include "../Activity.h"

class DrawingActivity final : public Activity {
 public:
  DrawingActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  std::function<void()> onExit;
  bool needsRender = true;
  bool touchStrokeActive = false;
  bool hasDirtyStroke = false;
  int lastStrokeX = 0;
  int lastStrokeY = 0;
  unsigned long lastFlushMs = 0;

  void renderOverlay();
};
