#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../ActivityWithSubactivity.h"

class GpioMonitorActivity final : public ActivityWithSubactivity {
 public:
  GpioMonitorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  enum class Profile {
    PortB,
    PortC,
    ExternalI2C,
    Custom
  };

  std::function<void()> onExitCb;
  Profile profile = Profile::PortB;
  bool pullupEnabled = false;
  std::vector<int> pins;
  std::vector<int> values;
  std::vector<int> lastValues;
  std::string statusMessage;
  bool needsRender = true;
  unsigned long lastPollMs = 0;
  unsigned long pollIntervalMs = 1000;

  void setProfile(Profile next);
  void applyPins();
  void updateValues();
  void promptCustomPins();
  bool parsePins(const std::string& text, std::vector<int>& out) const;

  const char* profileLabel(Profile p) const;
  void render();
};
