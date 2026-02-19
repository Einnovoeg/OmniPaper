#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../ActivityWithSubactivity.h"

class ToolsOtaUpdateActivity final : public ActivityWithSubactivity {
 public:
  ToolsOtaUpdateActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  enum class State { Menu, SdList, Updating, Result, WifiSelection, UrlEntry };

  std::function<void()> onExit;
  State state = State::Menu;
  int menuIndex = 0;
  int fileIndex = 0;
  std::vector<std::string> sdFiles;
  std::string statusMessage;
  std::string pendingUrl;
  bool updateOk = false;
  bool needsRender = true;

  void loadSdFiles();
  void startSdUpdate();
  void startUrlUpdate();
  void launchWifiSelection();
  void launchUrlEntry();
  void render();
  bool updateFromFile(const std::string& path, std::string& error);
  bool updateFromUrl(const std::string& url, std::string& error);
};
