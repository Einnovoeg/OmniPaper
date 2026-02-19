#pragma once

#include <functional>

#include "../ActivityWithSubactivity.h"

class TimeSettingsActivity final : public ActivityWithSubactivity {
 public:
  TimeSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  enum class Mode { Idle, Syncing, Result };

  std::function<void()> onExit;
  int selectionIndex = 0;
  Mode mode = Mode::Idle;
  bool needsRender = true;
  std::string statusMessage;

  void launchWifiSelection();
  void performSync();
  void render();
};
