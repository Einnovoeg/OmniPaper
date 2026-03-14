#pragma once

#include <HalGPIO.h>

#include <functional>
#include <string>

#include "../Activity.h"

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
  std::string statusMessage;
  unsigned long statusMessageExpiresAt = 0;

  void render();
#if defined(PLATFORM_M5PAPERS3)
  void runPaperS3SpeakerTest();
  void renderPaperS3();
#endif
};
