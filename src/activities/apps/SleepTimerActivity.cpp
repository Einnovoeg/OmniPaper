#include "SleepTimerActivity.h"

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"

namespace {
struct SleepOption {
  const char* label;
  uint64_t seconds;
};

const SleepOption kOptions[] = {
    {"5 minutes", 5ULL * 60ULL},
    {"15 minutes", 15ULL * 60ULL},
    {"30 minutes", 30ULL * 60ULL},
    {"1 hour", 60ULL * 60ULL},
    {"2 hours", 120ULL * 60ULL},
};
constexpr int kOptionCount = sizeof(kOptions) / sizeof(kOptions[0]);
}  // namespace

SleepTimerActivity::SleepTimerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, SleepCallback onSleep,
                                       const std::function<void()>& onExit)
    : Activity("SleepTimer", renderer, mappedInput), onSleep(std::move(onSleep)), onExit(onExit) {}

void SleepTimerActivity::onEnter() {
  Activity::onEnter();
  selectionIndex = 0;
  needsRender = true;
}

void SleepTimerActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExit) {
      onExit();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selectionIndex = (selectionIndex - 1 + kOptionCount) % kOptionCount;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selectionIndex = (selectionIndex + 1) % kOptionCount;
    needsRender = true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (onSleep) {
      onSleep(kOptions[selectionIndex].seconds);
    }
    return;
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void SleepTimerActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "Sleep Timer");

  const int startY = 70;
  for (int i = 0; i < kOptionCount; i++) {
    const int y = startY + i * 28;
    if (i == selectionIndex) {
      renderer.fillRect(40, y - 6, renderer.getScreenWidth() - 80, 24, true);
      renderer.drawText(UI_12_FONT_ID, 50, y, kOptions[i].label, false);
    } else {
      renderer.drawText(UI_12_FONT_ID, 50, y, kOptions[i].label);
    }
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Confirm: Sleep   Back: Menu");
  renderer.displayBuffer();
}
