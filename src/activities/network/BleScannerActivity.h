#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"

class BleScannerActivity final : public Activity {
 public:
  BleScannerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  struct BleEntry {
    std::string name;
    std::string address;
    int rssi = 0;
    std::vector<std::string> services;
    int mfgDataLen = 0;
  };

  std::function<void()> onExitCb;
  std::vector<BleEntry> devices;
  std::vector<int> visibleIndices;
  bool scanning = false;
  bool showDetail = false;
  bool filterNamed = false;
  int selectionIndex = 0;
  std::string statusMessage;
  bool needsRender = true;

  void startScan();
  void performScan();
  void rebuildVisible();
  void saveSelected();
  void render();
};
