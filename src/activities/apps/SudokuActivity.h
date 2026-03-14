#pragma once

#include <array>
#include <functional>
#include <string>

#include "../Activity.h"

class SudokuActivity final : public Activity {
 public:
  SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  using Board = std::array<std::array<uint8_t, 9>, 9>;
  using FixedMask = std::array<std::array<bool, 9>, 9>;
  using NoteMask = std::array<std::array<uint16_t, 9>, 9>;

  std::function<void()> onExit;

  Board puzzle_{};
  Board grid_{};
  Board solution_{};
  FixedMask fixed_{};
  NoteMask notes_{};

  int selectedCol_ = 0;
  int selectedRow_ = 0;
  bool pencilMode_ = false;
  bool solved_ = false;
  bool needsRender_ = true;
  std::string statusMessage_;

  void render();
  void handleInput();
  void resetBoardState();
  void selectCell(int row, int col);
  void applyDigit(uint8_t value);
  void clearSelectedCell();
  void applyHint();
  void updateStatusMessage();

  bool solveBoard(Board& board) const;
  bool isPlacementAllowed(const Board& board, int row, int col, uint8_t value) const;
  bool isCurrentValueValid(int row, int col) const;
  bool boardSolved() const;
};
