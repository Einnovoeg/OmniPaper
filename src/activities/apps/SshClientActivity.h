#pragma once

#include <functional>
#include <string>

#include "activities/ActivityWithSubactivity.h"

class SshClientActivity final : public ActivityWithSubactivity {
 public:
  SshClientActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  struct Profile {
    std::string host;
    std::string user;
    std::string password;
    std::string command;
    uint16_t port = 22;
  };

  std::function<void()> onExitCb;
  Profile profile;

  int selectionIndex = 0;
  bool needsRender = true;

  std::string statusMessage;
  bool statusError = false;
  unsigned long statusExpiresAtMs = 0;
  std::string lastOutput;

  bool libsshInitialized = false;

  void launchWifiSelection();
  void editTextField(const char* title, std::string* field, size_t maxLen, bool passwordField);
  void editPortField();

  void loadProfile();
  void saveProfile();

  bool runCommand();
  void appendOutput(const char* chunk, size_t len);

  void setStatus(const std::string& text, bool isError = false, unsigned long timeoutMs = 3000);
  void clearExpiredStatus();

  void render();
};
