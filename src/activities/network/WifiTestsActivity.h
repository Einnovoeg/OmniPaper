#pragma once

#include <functional>
#include <string>

#include "../ActivityWithSubactivity.h"

class WifiTestsActivity final : public ActivityWithSubactivity {
 public:
  WifiTestsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  enum class PendingAction { None, Http, Dns, Tcp };

  std::function<void()> onExit;
  std::string host = "example.com";
  std::string resultMessage;
  int selectionIndex = 0;
  bool needsRender = true;
  PendingAction pending = PendingAction::None;

  void runHttpTest();
  void runDnsTest();
  void runTcpTest();
  void ensureWifiThenRun(PendingAction action);
  void launchWifiSelection();
  void launchHostEntry();
  void render();
};
