#pragma once

#include <functional>
#include <array>

#include "../Activity.h"

class WifiChannelMonitorActivity final : public Activity {
 public:
  WifiChannelMonitorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                             const std::function<void()>& onExit);

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  std::function<void()> onExitCb;
  bool scanning = false;
  bool needsRender = true;
  std::array<int, 15> counts{};

  void startScan();
  void collectResults();
  void render();
};
