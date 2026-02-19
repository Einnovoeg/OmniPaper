#pragma once

#include <functional>
#include <string>

#include "../ActivityWithSubactivity.h"

class NotesActivity final : public ActivityWithSubactivity {
 public:
  NotesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  std::function<void()> onExit;
  std::string noteText;
  bool needsRender = true;

  void loadNote();
  void saveNote();
  void startEdit();
  void render();
};
