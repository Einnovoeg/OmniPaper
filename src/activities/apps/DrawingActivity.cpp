#include "DrawingActivity.h"

#ifdef PLATFORM_M5PAPER
#include <M5Unified.h>
#endif

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"

DrawingActivity::DrawingActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                 const std::function<void()>& onExit)
    : Activity("Drawing", renderer, mappedInput), onExit(onExit) {}

void DrawingActivity::onEnter() {
  Activity::onEnter();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  renderer.clearScreen();
  renderOverlay();
  renderer.displayBuffer();
}

void DrawingActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExit) {
      onExit();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    renderer.clearScreen();
    renderOverlay();
    renderer.displayBuffer();
    return;
  }

#ifdef PLATFORM_M5PAPER
  const auto detail = M5.Touch.getDetail();
  if (detail.isPressed()) {
    int x = static_cast<int>(detail.x);
    int y = static_cast<int>(detail.y);
    if (y > 40) {
      for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
          renderer.drawPixel(x + dx, y + dy, true);
        }
      }
      renderer.displayBuffer();
    }
  }
#endif
}

void DrawingActivity::renderOverlay() {
  renderer.drawCenteredText(SMALL_FONT_ID, 8, "Touch to draw  |  Confirm: Clear  Back: Menu");
}
