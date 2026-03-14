#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "../Activity.h"

class TetrisActivity final : public Activity {
 public:
  TetrisActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  static constexpr int kBoardWidth = 10;
  static constexpr int kBoardHeight = 20;

  using Board = std::array<std::array<uint8_t, kBoardWidth>, kBoardHeight>;

  std::function<void()> onExit;
  Board board{};
  uint8_t pieceType = 0;
  uint8_t rotation = 0;
  int pieceX = 0;
  int pieceY = 0;
  int score = 0;
  int clearedLines = 0;
  unsigned long lastDropMs = 0;
  bool gameOver = false;
  bool needsRender = true;

  void resetGame();
  void spawnPiece();
  bool canPlace(uint8_t type, uint8_t rot, int x, int y) const;
  void lockPiece();
  void clearCompletedLines();
  bool movePiece(int dx, int dy);
  void rotatePiece();
  void stepGame();
  void render();
  void drawCell(int x, int y, bool filled) const;
};
