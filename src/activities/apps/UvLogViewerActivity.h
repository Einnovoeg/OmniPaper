#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"

class UvLogViewerActivity final : public Activity {
 public:
  UvLogViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  struct UvLogEntry {
    long timestamp = 0;
    std::string addr;
    float uva = 0.0f;
    float uvb = 0.0f;
    float uvc = 0.0f;
    float tempC = 0.0f;
  };

  std::function<void()> onExitCb;
  std::vector<UvLogEntry> entries;
  int selectionIndex = 0;
  std::string statusMessage;
  bool needsRender = true;

  void loadLogs();
  bool parseLine(const std::string& line, UvLogEntry& out) const;
  void render();
};
