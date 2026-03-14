#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>

#include "../Activity.h"

class MinesweeperActivity final : public Activity {
 public:
  MinesweeperActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  static constexpr int kBoardWidth = 10;
  static constexpr int kBoardHeight = 12;
  static constexpr int kMineCount = 18;

  enum class ToolMode : uint8_t { Reveal = 0, Flag };

  using BoolBoard = std::array<std::array<bool, kBoardWidth>, kBoardHeight>;
  using CountBoard = std::array<std::array<uint8_t, kBoardWidth>, kBoardHeight>;

  std::function<void()> onExit;

  BoolBoard mines_{};
  BoolBoard revealed_{};
  BoolBoard flagged_{};
  CountBoard adjacentCounts_{};

  ToolMode mode_ = ToolMode::Reveal;
  bool boardSeeded_ = false;
  bool gameOver_ = false;
  bool win_ = false;
  bool needsRender_ = true;
  int cursorRow_ = 0;
  int cursorCol_ = 0;
  int explodedRow_ = -1;
  int explodedCol_ = -1;
  int flagsPlaced_ = 0;
  int safeCellsRevealed_ = 0;
  std::string statusMessage_;

  void resetGame();
  void placeMinesAvoiding(int safeRow, int safeCol);
  void rebuildAdjacencyCounts();
  void revealAt(int row, int col);
  void revealFloodFrom(int row, int col);
  void toggleFlagAt(int row, int col);
  void chordAt(int row, int col);
  void useHint();
  void revealAllMines();
  void checkWin();
  void toggleMode();
  void selectCell(int row, int col);
  void updateStatusMessage();

  bool inBounds(int row, int col) const;
  int countFlaggedNeighbors(int row, int col) const;
  int countHiddenSafeCells() const;

  void render();
  void handleInput();
  void drawCell(int row, int col) const;
};
