#pragma once

#include <Wire.h>

#include <functional>
#include <string>
#include <vector>

#include "../ActivityWithSubactivity.h"

class I2cEepromDumpActivity final : public ActivityWithSubactivity {
 public:
  I2cEepromDumpActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  enum class Screen {
    SelectDevice,
    DumpView
  };

  std::function<void()> onExitCb;
  TwoWire i2c;
  Screen screen = Screen::SelectDevice;
  std::vector<uint8_t> devices;
  int selectionIndex = 0;
  bool use16bitAddress = true;
  uint16_t startAddr = 0x0000;
  uint16_t dumpLen = 128;
  std::vector<uint8_t> dumpBytes;
  int viewOffset = 0;
  std::string statusMessage;
  bool needsRender = true;

  void scanDevices();
  bool readByte(uint8_t deviceAddr, uint16_t memAddr, uint8_t& out);
  void dumpSelectedDevice();
  void promptStartAddress();
  void promptLength();
  void render();
  void renderDeviceSelect();
  void renderDumpView();
};
