#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"

class OptionalDevicesActivity final : public Activity {
 public:
  enum class DeviceAction { None = 0, ExternalSensors, UvSensor, UvLogs };

  using ExitCallback = std::function<void()>;
  using ActionCallback = std::function<void(DeviceAction)>;

  OptionalDevicesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, ExitCallback onExit,
                          ActionCallback onAction)
      : Activity("OptionalDevices", renderer, mappedInput), onExit(std::move(onExit)), onAction(std::move(onAction)) {}

  void onEnter() override;
  void loop() override;

 private:
  struct Row {
    std::string title;
    std::string subtitle;
    DeviceAction action = DeviceAction::None;

    Row() = default;
    Row(std::string rowTitle, std::string rowSubtitle, const DeviceAction rowAction)
        : title(std::move(rowTitle)), subtitle(std::move(rowSubtitle)), action(rowAction) {}
  };

  ExitCallback onExit;
  ActionCallback onAction;
  std::vector<Row> rows;
  bool needsRender = true;
  int selectedIndex = 0;

  void refreshRows();
  void render();
};
