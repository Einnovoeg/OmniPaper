#pragma once

#include <functional>

#include "../ActivityWithSubactivity.h"

class WifiStatusActivity final : public ActivityWithSubactivity {
 public:
  WifiStatusActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  std::function<void()> onExit;
  bool needsRender = true;

  void launchWifiSelection();
  void render();
};
