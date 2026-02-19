#pragma once

#include <functional>
#include <array>

#include "../Activity.h"

class SudokuActivity final : public Activity {
 public:
  SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  std::function<void()> onExit;
  std::array<std::array<uint8_t, 9>, 9> grid{};
  std::array<std::array<uint8_t, 9>, 9> fixed{};
  int selX = 0;
  int selY = 0;
  bool needsRender = true;

  void render();
  void handleInput();
};
