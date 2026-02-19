#pragma once

#include <functional>

#include "../Activity.h"

class SleepTimerActivity final : public Activity {
 public:
  using SleepCallback = std::function<void(uint64_t seconds)>;

  SleepTimerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, SleepCallback onSleep,
                     const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  SleepCallback onSleep;
  std::function<void()> onExit;
  int selectionIndex = 0;
  bool needsRender = true;

  void render();
};
