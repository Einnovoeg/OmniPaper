#pragma once

#include <GfxRenderer.h>

#include "fontIds.h"

namespace PaperS3Ui {

// Shared card chrome for the touch-first PaperS3 dashboards and diagnostics.
constexpr int kOuterMargin = 24;
constexpr int kCardGap = 16;
constexpr int kTopY = 42;
constexpr int kFooterOffset = 24;
constexpr int kFooterStatusOffset = 46;
constexpr int kHeaderHeight = 28;
constexpr int kCardInnerPadding = 12;

struct FourCardLayout {
  int leftX = 0;
  int rightX = 0;
  int topY = 0;
  int bottomY = 0;
  int cardWidth = 0;
  int cardHeight = 0;
};

inline FourCardLayout fourCardLayout(const GfxRenderer& renderer) {
  FourCardLayout layout;
  layout.leftX = kOuterMargin;
  layout.cardWidth = (renderer.getScreenWidth() - (kOuterMargin * 2) - kCardGap) / 2;
  layout.rightX = layout.leftX + layout.cardWidth + kCardGap;
  layout.topY = kTopY;

  const int footerReserve = 52;
  layout.cardHeight = (renderer.getScreenHeight() - kTopY - footerReserve - kCardGap) / 2;
  layout.bottomY = layout.topY + layout.cardHeight + kCardGap;
  return layout;
}

inline void drawCard(GfxRenderer& renderer, const int x, const int y, const int width, const int height,
                     const char* title) {
  renderer.drawRect(x, y, width, height);
  renderer.fillRect(x + 1, y + 1, width - 2, kHeaderHeight, true);
  renderer.drawText(UI_10_FONT_ID, x + kCardInnerPadding, y + 6, title, false, EpdFontFamily::BOLD);
}

inline int bodyX(const int cardX) { return cardX + kCardInnerPadding; }

inline int bodyY(const int cardY) { return cardY + kHeaderHeight + 10; }

inline const char* yesNo(const bool value) { return value ? "Yes" : "No"; }

inline const char* readyOff(const bool value) { return value ? "Ready" : "Off"; }

inline const char* openWaiting(const bool value) { return value ? "Open" : "Waiting"; }

inline void drawFooter(GfxRenderer& renderer, const char* text) {
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - kFooterOffset, text);
}

inline void drawFooterStatus(GfxRenderer& renderer, const char* text) {
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - kFooterStatusOffset, text);
}

}  // namespace PaperS3Ui
