#pragma once

#include <functional>
#include <string>

#include "activities/ActivityWithSubactivity.h"

class KeyboardHostActivity final : public ActivityWithSubactivity {
 public:
  KeyboardHostActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  enum class Transport : uint8_t { BluetoothHid = 0, UsbSerial = 1 };

  std::function<void()> onExitCb;
  Transport transport = Transport::BluetoothHid;
  int selectionIndex = 0;
  bool needsRender = true;
  std::string statusMessage;
  bool statusError = false;
  unsigned long statusExpiresAtMs = 0;

  void beginBleKeyboard();
  void stopBleKeyboard();
  bool isBleConnected() const;

  void launchCompose();
  bool sendTextToHost(const std::string& text);
  bool sendCtrlShortcut(char letter);
  void sendEnterKey();

  void setStatus(const std::string& text, bool isError = false, unsigned long timeoutMs = 2500);
  void clearExpiredStatus();
  void render();
};
