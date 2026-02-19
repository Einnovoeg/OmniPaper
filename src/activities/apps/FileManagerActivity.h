#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../ActivityWithSubactivity.h"

class FileManagerActivity final : public ActivityWithSubactivity {
 public:
  using OpenReaderCallback = std::function<void(const std::string&)>;

  FileManagerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit,
                      OpenReaderCallback onOpenReader);

  void onEnter() override;
  void loop() override;

 private:
  struct Entry {
    std::string name;
    bool isDir = false;
    uint32_t size = 0;
  };

  std::function<void()> onExit;
  OpenReaderCallback onOpenReader;
  std::string currentPath = "/";
  std::vector<Entry> entries;
  int selectionIndex = 0;
  bool needsRender = true;
  std::string statusMessage;

  void loadDirectory();
  void openSelected();
  void goUp();
  std::string joinPath(const std::string& base, const std::string& name) const;
  void render();
};
