#include "TetrisActivity.h"

#include <GfxRenderer.h>
#include <esp_system.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "PaperS3Ui.h"
#include "fontIds.h"

namespace {
constexpr unsigned long kBaseDropMs = 700;
constexpr int kCellSize = 22;
constexpr int kBoardX = 52;
constexpr int kBoardY = 118;

const int8_t kShapes[7][4][4][2] = {
    {{{0, 1}, {1, 1}, {2, 1}, {3, 1}},
     {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
     {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
     {{1, 0}, {1, 1}, {1, 2}, {1, 3}}},
    {{{1, 0}, {2, 0}, {1, 1}, {2, 1}},
     {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
     {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
     {{1, 0}, {2, 0}, {1, 1}, {2, 1}}},
    {{{1, 0}, {0, 1}, {1, 1}, {2, 1}},
     {{1, 0}, {1, 1}, {2, 1}, {1, 2}},
     {{0, 1}, {1, 1}, {2, 1}, {1, 2}},
     {{1, 0}, {0, 1}, {1, 1}, {1, 2}}},
    {{{1, 0}, {2, 0}, {0, 1}, {1, 1}},
     {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
     {{1, 1}, {2, 1}, {0, 2}, {1, 2}},
     {{0, 0}, {0, 1}, {1, 1}, {1, 2}}},
    {{{0, 0}, {1, 0}, {1, 1}, {2, 1}},
     {{2, 0}, {1, 1}, {2, 1}, {1, 2}},
     {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
     {{1, 0}, {0, 1}, {1, 1}, {0, 2}}},
    {{{0, 0}, {0, 1}, {1, 1}, {2, 1}},
     {{1, 0}, {2, 0}, {1, 1}, {1, 2}},
     {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
     {{1, 0}, {1, 1}, {0, 2}, {1, 2}}},
    {{{2, 0}, {0, 1}, {1, 1}, {2, 1}},
     {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
     {{0, 1}, {1, 1}, {2, 1}, {0, 2}},
     {{0, 0}, {1, 0}, {1, 1}, {1, 2}}},
};
}  // namespace

TetrisActivity::TetrisActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                               const std::function<void()>& onExit)
    : Activity("Tetris", renderer, mappedInput), onExit(onExit) {}

void TetrisActivity::onEnter() {
  Activity::onEnter();
  resetGame();
}

void TetrisActivity::resetGame() {
  for (auto& row : board) {
    row.fill(0);
  }

  score = 0;
  clearedLines = 0;
  gameOver = false;
  lastDropMs = millis();
  spawnPiece();
  needsRender = true;
}

void TetrisActivity::spawnPiece() {
  pieceType = static_cast<uint8_t>(esp_random() % 7);
  rotation = 0;
  pieceX = 3;
  pieceY = 0;
  if (!canPlace(pieceType, rotation, pieceX, pieceY)) {
    gameOver = true;
  }
}

bool TetrisActivity::canPlace(const uint8_t type, const uint8_t rot, const int x, const int y) const {
  for (int i = 0; i < 4; i++) {
    const int px = x + kShapes[type][rot][i][0];
    const int py = y + kShapes[type][rot][i][1];
    if (px < 0 || px >= kBoardWidth || py < 0 || py >= kBoardHeight) {
      return false;
    }
    if (board[py][px] != 0) {
      return false;
    }
  }
  return true;
}

void TetrisActivity::lockPiece() {
  for (int i = 0; i < 4; i++) {
    const int px = pieceX + kShapes[pieceType][rotation][i][0];
    const int py = pieceY + kShapes[pieceType][rotation][i][1];
    if (px >= 0 && px < kBoardWidth && py >= 0 && py < kBoardHeight) {
      board[py][px] = static_cast<uint8_t>(pieceType + 1);
    }
  }

  clearCompletedLines();
  spawnPiece();
}

void TetrisActivity::clearCompletedLines() {
  int linesThisMove = 0;
  for (int y = kBoardHeight - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < kBoardWidth; x++) {
      if (board[y][x] == 0) {
        full = false;
        break;
      }
    }

    if (!full) {
      continue;
    }

    for (int row = y; row > 0; row--) {
      board[row] = board[row - 1];
    }
    board[0].fill(0);
    linesThisMove++;
    y++;
  }

  if (linesThisMove > 0) {
    clearedLines += linesThisMove;
    score += linesThisMove * 100;
  }
}

bool TetrisActivity::movePiece(const int dx, const int dy) {
  const int nextX = pieceX + dx;
  const int nextY = pieceY + dy;
  if (!canPlace(pieceType, rotation, nextX, nextY)) {
    return false;
  }
  pieceX = nextX;
  pieceY = nextY;
  return true;
}

void TetrisActivity::rotatePiece() {
  const uint8_t nextRotation = static_cast<uint8_t>((rotation + 1) % 4);
  if (canPlace(pieceType, nextRotation, pieceX, pieceY)) {
    rotation = nextRotation;
    return;
  }

  if (canPlace(pieceType, nextRotation, pieceX - 1, pieceY)) {
    pieceX -= 1;
    rotation = nextRotation;
    return;
  }

  if (canPlace(pieceType, nextRotation, pieceX + 1, pieceY)) {
    pieceX += 1;
    rotation = nextRotation;
  }
}

void TetrisActivity::stepGame() {
  if (gameOver) {
    return;
  }

  const unsigned long interval = std::max<unsigned long>(220, kBaseDropMs - clearedLines * 15);
  if (millis() - lastDropMs < interval) {
    return;
  }

  lastDropMs = millis();
  if (!movePiece(0, 1)) {
    lockPiece();
  }
  needsRender = true;
}

void TetrisActivity::loop() {
#if defined(PLATFORM_M5PAPERS3)
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasTapped() &&
      PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), tapX, tapY)) {
    if (PaperS3Ui::backButtonRect(renderer).contains(tapX, tapY)) {
      if (onExit) {
        onExit();
      }
      return;
    }

    if (PaperS3Ui::sideActionRect(renderer, false).contains(tapX, tapY) && !gameOver) {
      movePiece(-1, 0);
      needsRender = true;
    } else if (PaperS3Ui::primaryActionRect(renderer).contains(tapX, tapY)) {
      if (gameOver) {
        resetGame();
      } else {
        rotatePiece();
        needsRender = true;
      }
    } else if (PaperS3Ui::sideActionRect(renderer, true).contains(tapX, tapY) && !gameOver) {
      movePiece(1, 0);
      needsRender = true;
    } else if (tapX >= kBoardX && tapX < kBoardX + kBoardWidth * kCellSize && tapY >= kBoardY &&
               tapY < kBoardY + kBoardHeight * kCellSize && !gameOver) {
      if (!movePiece(0, 1)) {
        lockPiece();
      }
      lastDropMs = millis();
      needsRender = true;
    }
  }
#endif

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExit) {
      onExit();
    }
    return;
  }

  if (!gameOver) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
      movePiece(-1, 0);
      needsRender = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      movePiece(1, 0);
      needsRender = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Up) ||
        mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      rotatePiece();
      needsRender = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      if (!movePiece(0, 1)) {
        lockPiece();
      }
      lastDropMs = millis();
      needsRender = true;
    }
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    resetGame();
  }

  stepGame();

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void TetrisActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  PaperS3Ui::drawScreenHeader(renderer, "Tetris", gameOver ? "Game over" : "Touch board to drop");
  PaperS3Ui::drawBackButton(renderer);

  renderer.drawRect(kBoardX - 2, kBoardY - 2, kBoardWidth * kCellSize + 4, kBoardHeight * kCellSize + 4);

  for (int y = 0; y < kBoardHeight; y++) {
    for (int x = 0; x < kBoardWidth; x++) {
      drawCell(kBoardX + x * kCellSize, kBoardY + y * kCellSize, board[y][x] != 0);
    }
  }

  if (!gameOver) {
    for (int i = 0; i < 4; i++) {
      const int x = pieceX + kShapes[pieceType][rotation][i][0];
      const int y = pieceY + kShapes[pieceType][rotation][i][1];
      if (x >= 0 && x < kBoardWidth && y >= 0 && y < kBoardHeight) {
        drawCell(kBoardX + x * kCellSize, kBoardY + y * kCellSize, true);
      }
    }
  }

  int infoX = 312;
  int infoY = 146;
  char line[48];
  snprintf(line, sizeof(line), "Score: %d", score);
  renderer.drawText(UI_12_FONT_ID, infoX, infoY, line, true, EpdFontFamily::BOLD);
  infoY += 34;
  snprintf(line, sizeof(line), "Lines: %d", clearedLines);
  renderer.drawText(UI_10_FONT_ID, infoX, infoY, line);
  infoY += 28;
  snprintf(line, sizeof(line), "Piece: %d", static_cast<int>(pieceType + 1));
  renderer.drawText(UI_10_FONT_ID, infoX, infoY, line);
  infoY += 36;
  renderer.drawText(UI_10_FONT_ID, infoX, infoY, "Left/Right: move");
  infoY += 24;
  renderer.drawText(UI_10_FONT_ID, infoX, infoY, "Rotate: turn");
  infoY += 24;
  renderer.drawText(UI_10_FONT_ID, infoX, infoY, "Board tap: drop");

  PaperS3Ui::drawSideActionButton(renderer, false, "Left");
  PaperS3Ui::drawPrimaryActionButton(renderer, gameOver ? "Restart" : "Rotate");
  PaperS3Ui::drawSideActionButton(renderer, true, "Right");
  PaperS3Ui::drawFooter(renderer, gameOver ? "Tap Restart to play again" : "Auto drop is active");
  renderer.displayBuffer();
}

void TetrisActivity::drawCell(const int x, const int y, const bool filled) const {
  if (filled) {
    renderer.fillRect(x + 1, y + 1, kCellSize - 2, kCellSize - 2, true);
  } else {
    renderer.fillRect(x + 1, y + 1, kCellSize - 2, kCellSize - 2, false);
  }
  renderer.drawRect(x, y, kCellSize, kCellSize, true);
}
