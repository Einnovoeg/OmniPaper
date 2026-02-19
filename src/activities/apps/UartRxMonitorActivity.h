#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../ActivityWithSubactivity.h"

class UartRxMonitorActivity final : public ActivityWithSubactivity {
 public:
  UartRxMonitorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool preventAutoSleep() override { return true; }

 private:
  std::function<void()> onExitCb;
  HardwareSerial uart;
  bool listening = false;
  int baudRate = 9600;
  int rxPin = 33;
  std::string statusMessage;
  std::vector<std::string> lines;
  std::string currentLine;
  bool needsRender = true;

  void startListening();
  void stopListening();
  void pollUart();
  void appendLine(const std::string& line);
  void promptBaud();
  void promptRxPin();
  void render();
};
