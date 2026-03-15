#include "DrawingActivity.h"

#ifdef PLATFORM_M5PAPER
#include <M5Unified.h>
#endif

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "PaperS3Ui.h"
#include "fontIds.h"

namespace {
constexpr int kPaperS3CanvasTop = 170;
constexpr int kPaperS3CanvasBottom = 830;
constexpr int kBrushRadius = 3;
constexpr unsigned long kFlushIntervalMs = 40;

PaperS3Ui::Rect canvasRect(const GfxRenderer& renderer) {
  return {PaperS3Ui::kOuterMargin, kPaperS3CanvasTop, renderer.getScreenWidth() - PaperS3Ui::kOuterMargin * 2,
          kPaperS3CanvasBottom - kPaperS3CanvasTop};
}
}  // namespace

DrawingActivity::DrawingActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                 const std::function<void()>& onExit)
    : Activity("Drawing", renderer, mappedInput), onExit(onExit) {}

void DrawingActivity::onEnter() {
  Activity::onEnter();
  PaperS3Ui::applyDefaultOrientation(renderer);
  renderer.clearScreen();
  touchStrokeActive = false;
  hasDirtyStroke = false;
  lastFlushMs = millis();
  renderOverlay();
  renderer.displayBuffer();
}

void DrawingActivity::loop() {
#if defined(PLATFORM_M5PAPERS3)
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasTapped() &&
      PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), tapX, tapY)) {
    if (PaperS3Ui::backButtonRect(renderer).contains(tapX, tapY)) {
      if (onExit) {
        onExit();
      }
      return;
    }
    if (PaperS3Ui::primaryActionRect(renderer).contains(tapX, tapY)) {
      renderer.clearScreen();
      renderOverlay();
      renderer.displayBuffer();
      touchStrokeActive = false;
      hasDirtyStroke = false;
      return;
    }
  }
#endif

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
    touchStrokeActive = false;
    hasDirtyStroke = false;
    return;
  }

#if defined(PLATFORM_M5PAPERS3)
  int logicalX = 0;
  int logicalY = 0;
  const bool hasTouch = mappedInput.isTouchPressed() &&
                        PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), logicalX,
                                                      logicalY);

  const auto canvas = canvasRect(renderer);
  if (hasTouch && canvas.contains(logicalX, logicalY)) {
    if (!touchStrokeActive) {
      touchStrokeActive = true;
      lastStrokeX = logicalX;
      lastStrokeY = logicalY;
    }

    // Join successive touch points with a line so quick strokes do not leave
    // dotted gaps on the slower e-paper refresh cadence.
    renderer.drawLine(lastStrokeX, lastStrokeY, logicalX, logicalY, true);
    for (int dy = -kBrushRadius; dy <= kBrushRadius; dy++) {
      for (int dx = -kBrushRadius; dx <= kBrushRadius; dx++) {
        renderer.drawPixel(logicalX + dx, logicalY + dy, true);
      }
    }
    lastStrokeX = logicalX;
    lastStrokeY = logicalY;
    hasDirtyStroke = true;
  } else if (touchStrokeActive) {
    touchStrokeActive = false;
    if (hasDirtyStroke) {
      renderer.displayBuffer();
      hasDirtyStroke = false;
    }
  }

  if (hasDirtyStroke && millis() - lastFlushMs >= kFlushIntervalMs) {
    renderer.displayBuffer();
    hasDirtyStroke = false;
    lastFlushMs = millis();
  }
#elif defined(PLATFORM_M5PAPER)
  const auto detail = M5.Touch.getDetail();
  if (detail.isPressed()) {
    for (int dy = -2; dy <= 2; dy++) {
      for (int dx = -2; dx <= 2; dx++) {
        renderer.drawPixel(static_cast<int>(detail.x) + dx, static_cast<int>(detail.y) + dy, true);
      }
    }
    renderer.displayBuffer();
  }
#endif
}

void DrawingActivity::renderOverlay() {
#if defined(PLATFORM_M5PAPERS3)
  PaperS3Ui::drawScreenHeader(renderer, "Drawing", "Tap and drag inside the canvas");
  PaperS3Ui::drawBackButton(renderer);
  const auto canvas = canvasRect(renderer);
  renderer.drawRect(canvas.x, canvas.y, canvas.width, canvas.height, true);
  PaperS3Ui::drawPrimaryActionButton(renderer, "Clear");
  PaperS3Ui::drawFooter(renderer, "Touch the canvas to sketch");
#else
  renderer.drawCenteredText(SMALL_FONT_ID, 8, "Touch to draw  |  Confirm: Clear  Back: Menu");
#endif
}
