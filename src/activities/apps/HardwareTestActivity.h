#pragma once

#include <functional>

#include "../Activity.h"
#include <HalGPIO.h>

class HardwareTestActivity final : public Activity {
 public:
  HardwareTestActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, HalGPIO& gpio,
                       const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  HalGPIO& gpio;
  std::function<void()> onExit;
  bool needsRender = true;
  unsigned long lastUpdate = 0;

  void render();
};
