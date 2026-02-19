#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"

class WifiScannerActivity final : public Activity {
 public:
  WifiScannerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  struct NetworkEntry {
    std::string ssid;
    int rssi = 0;
    int channel = 0;
    bool open = false;
  };

  std::function<void()> onExitCb;
  std::vector<NetworkEntry> networks;
  bool scanning = false;
  bool needsRender = true;
  int selectionIndex = 0;
  std::string statusMessage;

  void startScan();
  void collectResults();
  void saveResults();
  void render();
};
