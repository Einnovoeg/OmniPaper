#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "../ActivityWithSubactivity.h"

class I2CToolsActivity final : public ActivityWithSubactivity {
 public:
  I2CToolsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  enum class Screen {
    Scan,
    Identify,
    Monitor,
  };

  std::function<void()> onExitCb;
  Screen screen = Screen::Scan;
  bool busReady = false;
  std::vector<uint8_t> addresses;
  int selectionIndex = 0;
  bool needsRender = true;
  std::string statusMessage;

  uint8_t monitorBaseReg = 0x00;
  uint8_t monitorLen = 16;
  std::array<uint8_t, 32> lastRegs{};
  std::array<uint8_t, 32> curRegs{};
  std::array<bool, 32> changed{};
  bool hasLast = false;
  uint16_t monitorIntervalMs = 1500;
  unsigned long lastPollMs = 0;

  void scanBus();
  void enterIdentify();
  void enterMonitor();
  void promptMonitorBase();
  void promptMonitorLength();
  void updateMonitor();
  bool readRegisters(uint8_t address, uint8_t reg, uint8_t* data, size_t len);
  const char* identifyHint(uint8_t address) const;

  void render();
  void renderScan();
  void renderIdentify();
  void renderMonitor();
};
