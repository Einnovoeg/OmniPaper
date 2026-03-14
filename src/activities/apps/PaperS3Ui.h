#pragma once

#include <GfxRenderer.h>

#include "fontIds.h"

namespace PaperS3Ui {

// Shared card chrome for the touch-first PaperS3 dashboards and diagnostics.
constexpr int kOuterMargin = 24;
constexpr int kCardGap = 16;
constexpr int kTopY = 92;
constexpr int kFooterOffset = 34;
constexpr int kFooterStatusOffset = 66;
constexpr int kHeaderHeight = 34;
constexpr int kCardInnerPadding = 14;
constexpr int kListTopY = 132;
constexpr int kListRowHeight = 62;
constexpr int kListRowGap = 12;

struct Rect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;

  constexpr Rect() = default;
  constexpr Rect(const int px, const int py, const int w, const int h) : x(px), y(py), width(w), height(h) {}

  bool contains(const int px, const int py) const {
    return px >= x && px < (x + width) && py >= y && py < (y + height);
  }
};

struct FourCardLayout {
  int leftX = 0;
  int rightX = 0;
  int topY = 0;
  int bottomY = 0;
  int cardWidth = 0;
  int cardHeight = 0;
};

inline GfxRenderer::Orientation defaultOrientation() {
#if defined(PLATFORM_M5PAPERS3)
  return GfxRenderer::Portrait;
#else
  return GfxRenderer::LandscapeCounterClockwise;
#endif
}

inline void applyDefaultOrientation(GfxRenderer& renderer) { renderer.setOrientation(defaultOrientation()); }

inline FourCardLayout fourCardLayout(const GfxRenderer& renderer) {
  FourCardLayout layout;
  layout.leftX = kOuterMargin;
  layout.cardWidth = (renderer.getScreenWidth() - (kOuterMargin * 2) - kCardGap) / 2;
  layout.rightX = layout.leftX + layout.cardWidth + kCardGap;
  layout.topY = kTopY;

  // Leave room for touch hints and transient status text so card content does
  // not collide with the PaperS3 footer affordances.
  const int footerReserve = 52;
  layout.cardHeight = (renderer.getScreenHeight() - kTopY - footerReserve - kCardGap) / 2;
  layout.bottomY = layout.topY + layout.cardHeight + kCardGap;
  return layout;
}

inline bool rawTouchToPortrait(const uint16_t rawX, const uint16_t rawY, int& logicalX, int& logicalY) {
  logicalX = HalDisplay::DISPLAY_HEIGHT - 1 - static_cast<int>(rawY);
  logicalY = static_cast<int>(rawX);
  return logicalX >= 0 && logicalX < HalDisplay::DISPLAY_HEIGHT && logicalY >= 0 &&
         logicalY < HalDisplay::DISPLAY_WIDTH;
}

inline Rect backButtonRect(const GfxRenderer& renderer) {
  Rect rect;
  rect.x = renderer.getScreenWidth() - 132;
  rect.y = 28;
  rect.width = 112;
  rect.height = 46;
  return rect;
}

inline Rect primaryActionRect(const GfxRenderer& renderer) {
  Rect rect;
  rect.x = (renderer.getScreenWidth() - 220) / 2;
  rect.y = renderer.getScreenHeight() - 98;
  rect.width = 220;
  rect.height = 54;
  return rect;
}

inline Rect sideActionRect(const GfxRenderer& renderer, const bool rightSide) {
  Rect rect;
  rect.width = 160;
  rect.height = 54;
  rect.x = rightSide ? (renderer.getScreenWidth() - kOuterMargin - rect.width) : kOuterMargin;
  rect.y = renderer.getScreenHeight() - 98;
  return rect;
}

inline Rect listRowRect(const GfxRenderer& renderer, const int index) {
  Rect rect;
  rect.x = kOuterMargin;
  rect.y = kListTopY + index * (kListRowHeight + kListRowGap);
  rect.width = renderer.getScreenWidth() - kOuterMargin * 2;
  rect.height = kListRowHeight;
  return rect;
}

inline void drawScreenHeader(GfxRenderer& renderer, const char* title, const char* subtitle = nullptr) {
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, title, true, EpdFontFamily::BOLD);
  if (subtitle != nullptr && subtitle[0] != '\0') {
    renderer.drawCenteredText(UI_12_FONT_ID, 64, subtitle);
  }
}

inline void drawBackButton(GfxRenderer& renderer, const char* label = "Back") {
  const Rect rect = backButtonRect(renderer);
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
  renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
  const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, label);
  const int textX = rect.x + (rect.width - textWidth) / 2;
  const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
  renderer.drawText(UI_12_FONT_ID, textX, textY, label);
}

inline void drawPrimaryActionButton(GfxRenderer& renderer, const char* label) {
  const Rect rect = primaryActionRect(renderer);
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
  renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
  const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, label);
  const int textX = rect.x + (rect.width - textWidth) / 2;
  const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
  renderer.drawText(UI_12_FONT_ID, textX, textY, label);
}

inline void drawSideActionButton(GfxRenderer& renderer, const bool rightSide, const char* label) {
  const Rect rect = sideActionRect(renderer, rightSide);
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
  renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
  const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, label);
  const int textX = rect.x + (rect.width - textWidth) / 2;
  const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
  renderer.drawText(UI_12_FONT_ID, textX, textY, label);
}

inline void drawListRow(GfxRenderer& renderer, const Rect& rect, const bool selected, const char* title,
                        const char* trailing = nullptr, const char* subtitle = nullptr) {
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
  renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
  if (selected) {
    renderer.fillRect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2, true);
  }

  renderer.drawText(UI_12_FONT_ID, rect.x + 18, rect.y + 16, title, !selected, EpdFontFamily::BOLD);

  if (subtitle != nullptr && subtitle[0] != '\0') {
    renderer.drawText(UI_10_FONT_ID, rect.x + 18, rect.y + 39, subtitle, !selected);
  }

  if (trailing != nullptr && trailing[0] != '\0') {
    const int width = renderer.getTextWidth(UI_10_FONT_ID, trailing);
    renderer.drawText(UI_10_FONT_ID, rect.x + rect.width - width - 18, rect.y + 20, trailing, !selected);
  }
}

inline void drawCard(GfxRenderer& renderer, const int x, const int y, const int width, const int height,
                     const char* title) {
  renderer.drawRect(x, y, width, height);
  renderer.fillRect(x + 1, y + 1, width - 2, kHeaderHeight, true);
  renderer.drawText(UI_12_FONT_ID, x + kCardInnerPadding, y + 8, title, false, EpdFontFamily::BOLD);
}

inline int bodyX(const int cardX) { return cardX + kCardInnerPadding; }

inline int bodyY(const int cardY) { return cardY + kHeaderHeight + 10; }

inline const char* yesNo(const bool value) { return value ? "Yes" : "No"; }

inline const char* readyOff(const bool value) { return value ? "Ready" : "Off"; }

inline const char* openWaiting(const bool value) { return value ? "Open" : "Waiting"; }

inline const char* presentAbsent(const bool value) { return value ? "Present" : "Absent"; }

inline bool usbCablePresent(const int16_t vbusMv) { return vbusMv >= 4000; }

inline void drawFooter(GfxRenderer& renderer, const char* text) {
  renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() - kFooterOffset, text);
}

inline void drawFooterStatus(GfxRenderer& renderer, const char* text) {
  renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() - kFooterStatusOffset, text);
}

}  // namespace PaperS3Ui
