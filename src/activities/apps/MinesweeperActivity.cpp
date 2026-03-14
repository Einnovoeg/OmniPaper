#include "MinesweeperActivity.h"

// Source reference:
// - https://github.com/lualiliu/esp32-gameboy
// - That repository was reviewed only as a games-section reference. It is a
//   GPLv3 Game Boy emulator targeting a 320x240 SPI TFT and physical buttons.
//   OmniPaper does not copy code from it. This native Minesweeper activity is a
//   clean local implementation chosen because low-refresh, touch-first gameplay
//   fits the PaperS3 substantially better than real-time emulation.

#include <GfxRenderer.h>
#include <esp_system.h>

#include <array>

#include "MappedInputManager.h"
#include "PaperS3Ui.h"
#include "fontIds.h"

namespace {
constexpr int kBoardWidth = 10;
constexpr int kBoardHeight = 12;
constexpr int kCellSize = 38;
constexpr int kBoardX = 24;
constexpr int kBoardY = 168;
constexpr int kInfoX = 426;

PaperS3Ui::Rect boardRect() { return {kBoardX, kBoardY, kBoardWidth * kCellSize, kBoardHeight * kCellSize}; }

PaperS3Ui::Rect cellRect(const int row, const int col) {
  return {kBoardX + col * kCellSize, kBoardY + row * kCellSize, kCellSize, kCellSize};
}
}  // namespace

MinesweeperActivity::MinesweeperActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                         const std::function<void()>& onExit)
    : Activity("Minesweeper", renderer, mappedInput), onExit(onExit) {}

void MinesweeperActivity::onEnter() {
  Activity::onEnter();
  resetGame();
}

void MinesweeperActivity::loop() {
  handleInput();
  if (needsRender_) {
    render();
    needsRender_ = false;
  }
}

void MinesweeperActivity::resetGame() {
  for (auto& row : mines_) {
    row.fill(false);
  }
  for (auto& row : revealed_) {
    row.fill(false);
  }
  for (auto& row : flagged_) {
    row.fill(false);
  }
  for (auto& row : adjacentCounts_) {
    row.fill(0);
  }

  mode_ = ToolMode::Reveal;
  boardSeeded_ = false;
  gameOver_ = false;
  win_ = false;
  cursorRow_ = 0;
  cursorCol_ = 0;
  explodedRow_ = -1;
  explodedCol_ = -1;
  flagsPlaced_ = 0;
  safeCellsRevealed_ = 0;
  updateStatusMessage();
  needsRender_ = true;
}

bool MinesweeperActivity::inBounds(const int row, const int col) const {
  return row >= 0 && row < kBoardHeight && col >= 0 && col < kBoardWidth;
}

void MinesweeperActivity::placeMinesAvoiding(const int safeRow, const int safeCol) {
  std::array<int, kBoardWidth * kBoardHeight> candidates{};
  int candidateCount = 0;

  // Keep the first reveal pleasant by reserving the tapped cell and its
  // neighbors so the opening move does not immediately punish the user.
  for (int row = 0; row < kBoardHeight; row++) {
    for (int col = 0; col < kBoardWidth; col++) {
      if (row >= safeRow - 1 && row <= safeRow + 1 && col >= safeCol - 1 && col <= safeCol + 1) {
        continue;
      }
      candidates[candidateCount++] = row * kBoardWidth + col;
    }
  }

  for (int i = 0; i < kMineCount && i < candidateCount; i++) {
    const int pick = i + static_cast<int>(esp_random() % static_cast<uint32_t>(candidateCount - i));
    const int selected = candidates[pick];
    candidates[pick] = candidates[i];
    candidates[i] = selected;

    const int row = selected / kBoardWidth;
    const int col = selected % kBoardWidth;
    mines_[row][col] = true;
  }

  rebuildAdjacencyCounts();
  boardSeeded_ = true;
}

void MinesweeperActivity::rebuildAdjacencyCounts() {
  for (auto& row : adjacentCounts_) {
    row.fill(0);
  }

  for (int row = 0; row < kBoardHeight; row++) {
    for (int col = 0; col < kBoardWidth; col++) {
      if (mines_[row][col]) {
        continue;
      }

      uint8_t count = 0;
      for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
          if (dx == 0 && dy == 0) {
            continue;
          }
          const int neighborRow = row + dy;
          const int neighborCol = col + dx;
          if (inBounds(neighborRow, neighborCol) && mines_[neighborRow][neighborCol]) {
            count++;
          }
        }
      }
      adjacentCounts_[row][col] = count;
    }
  }
}

void MinesweeperActivity::revealFloodFrom(const int row, const int col) {
  struct Cell {
    uint8_t row;
    uint8_t col;
  };

  std::array<Cell, kBoardWidth * kBoardHeight> stack{};
  BoolBoard queued{};
  int stackSize = 0;

  stack[stackSize++] = {static_cast<uint8_t>(row), static_cast<uint8_t>(col)};
  queued[row][col] = true;

  while (stackSize > 0) {
    const Cell current = stack[--stackSize];
    const int currentRow = current.row;
    const int currentCol = current.col;

    if (revealed_[currentRow][currentCol] || flagged_[currentRow][currentCol] || mines_[currentRow][currentCol]) {
      continue;
    }

    revealed_[currentRow][currentCol] = true;
    safeCellsRevealed_++;

    if (adjacentCounts_[currentRow][currentCol] != 0) {
      continue;
    }

    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        if (dx == 0 && dy == 0) {
          continue;
        }

        const int neighborRow = currentRow + dy;
        const int neighborCol = currentCol + dx;
        if (!inBounds(neighborRow, neighborCol) || queued[neighborRow][neighborCol] ||
            flagged_[neighborRow][neighborCol] || mines_[neighborRow][neighborCol]) {
          continue;
        }

        queued[neighborRow][neighborCol] = true;
        stack[stackSize++] = {static_cast<uint8_t>(neighborRow), static_cast<uint8_t>(neighborCol)};
      }
    }
  }
}

void MinesweeperActivity::revealAllMines() {
  for (int row = 0; row < kBoardHeight; row++) {
    for (int col = 0; col < kBoardWidth; col++) {
      if (mines_[row][col]) {
        revealed_[row][col] = true;
      }
    }
  }
}

void MinesweeperActivity::checkWin() {
  if (safeCellsRevealed_ >= (kBoardWidth * kBoardHeight - kMineCount)) {
    win_ = true;
    revealAllMines();
  }
}

void MinesweeperActivity::revealAt(const int row, const int col) {
  if (!inBounds(row, col) || flagged_[row][col] || gameOver_ || win_) {
    return;
  }

  if (revealed_[row][col]) {
    chordAt(row, col);
    return;
  }

  if (!boardSeeded_) {
    placeMinesAvoiding(row, col);
  }

  if (mines_[row][col]) {
    revealed_[row][col] = true;
    explodedRow_ = row;
    explodedCol_ = col;
    gameOver_ = true;
    revealAllMines();
    updateStatusMessage();
    needsRender_ = true;
    return;
  }

  revealFloodFrom(row, col);
  checkWin();
  updateStatusMessage();
  needsRender_ = true;
}

void MinesweeperActivity::toggleFlagAt(const int row, const int col) {
  if (!inBounds(row, col) || revealed_[row][col] || gameOver_ || win_) {
    return;
  }

  flagged_[row][col] = !flagged_[row][col];
  flagsPlaced_ += flagged_[row][col] ? 1 : -1;
  updateStatusMessage();
  needsRender_ = true;
}

int MinesweeperActivity::countFlaggedNeighbors(const int row, const int col) const {
  int count = 0;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      const int neighborRow = row + dy;
      const int neighborCol = col + dx;
      if (inBounds(neighborRow, neighborCol) && flagged_[neighborRow][neighborCol]) {
        count++;
      }
    }
  }
  return count;
}

void MinesweeperActivity::chordAt(const int row, const int col) {
  if (!revealed_[row][col] || adjacentCounts_[row][col] == 0 ||
      countFlaggedNeighbors(row, col) != adjacentCounts_[row][col]) {
    return;
  }

  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      const int neighborRow = row + dy;
      const int neighborCol = col + dx;
      if (inBounds(neighborRow, neighborCol) && !flagged_[neighborRow][neighborCol]) {
        revealAt(neighborRow, neighborCol);
      }
    }
  }
}

int MinesweeperActivity::countHiddenSafeCells() const {
  int count = 0;
  for (int row = 0; row < kBoardHeight; row++) {
    for (int col = 0; col < kBoardWidth; col++) {
      if (!mines_[row][col] && !revealed_[row][col]) {
        count++;
      }
    }
  }
  return count;
}

void MinesweeperActivity::useHint() {
  if (gameOver_ || win_) {
    return;
  }

  if (!boardSeeded_) {
    revealAt(kBoardHeight / 2, kBoardWidth / 2);
    return;
  }

  const int hiddenSafe = countHiddenSafeCells();
  if (hiddenSafe <= 0) {
    updateStatusMessage();
    needsRender_ = true;
    return;
  }

  const int start = static_cast<int>(esp_random() % static_cast<uint32_t>(kBoardWidth * kBoardHeight));
  for (int offset = 0; offset < kBoardWidth * kBoardHeight; offset++) {
    const int index = (start + offset) % (kBoardWidth * kBoardHeight);
    const int row = index / kBoardWidth;
    const int col = index % kBoardWidth;
    if (!mines_[row][col] && !revealed_[row][col] && !flagged_[row][col]) {
      revealAt(row, col);
      statusMessage_ = "Hint revealed a safe cell";
      needsRender_ = true;
      return;
    }
  }
}

void MinesweeperActivity::toggleMode() {
  mode_ = mode_ == ToolMode::Reveal ? ToolMode::Flag : ToolMode::Reveal;
  updateStatusMessage();
  needsRender_ = true;
}

void MinesweeperActivity::selectCell(const int row, const int col) {
  cursorRow_ = row < 0 ? 0 : (row >= kBoardHeight ? kBoardHeight - 1 : row);
  cursorCol_ = col < 0 ? 0 : (col >= kBoardWidth ? kBoardWidth - 1 : col);
  needsRender_ = true;
}

void MinesweeperActivity::updateStatusMessage() {
  if (win_) {
    statusMessage_ = "Board cleared";
    return;
  }
  if (gameOver_) {
    statusMessage_ = "Mine triggered";
    return;
  }
  if (mode_ == ToolMode::Flag) {
    statusMessage_ = "Flag mode: tap cells to mark mines";
    return;
  }
  if (!boardSeeded_) {
    statusMessage_ = "First reveal is always safe";
    return;
  }
  statusMessage_ = "Reveal mode: tap hidden cells or chord numbers";
}

void MinesweeperActivity::handleInput() {
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

    if (PaperS3Ui::sideActionRect(renderer, false).contains(tapX, tapY)) {
      toggleMode();
      return;
    }

    if (PaperS3Ui::primaryActionRect(renderer).contains(tapX, tapY)) {
      resetGame();
      return;
    }

    if (PaperS3Ui::sideActionRect(renderer, true).contains(tapX, tapY)) {
      useHint();
      return;
    }

    if (boardRect().contains(tapX, tapY)) {
      const int col = (tapX - kBoardX) / kCellSize;
      const int row = (tapY - kBoardY) / kCellSize;
      selectCell(row, col);
      if (mode_ == ToolMode::Flag) {
        toggleFlagAt(row, col);
      } else {
        revealAt(row, col);
      }
      return;
    }
  }
#endif

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExit) {
      onExit();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    selectCell(cursorRow_, cursorCol_ - 1);
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    selectCell(cursorRow_, cursorCol_ + 1);
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selectCell(cursorRow_ - 1, cursorCol_);
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selectCell(cursorRow_ + 1, cursorCol_);
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (mode_ == ToolMode::Flag) {
      toggleFlagAt(cursorRow_, cursorCol_);
    } else {
      revealAt(cursorRow_, cursorCol_);
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Power) ||
      mappedInput.wasPressed(MappedInputManager::Button::PageBack)) {
    toggleMode();
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::PageForward)) {
    useHint();
  }
}

void MinesweeperActivity::drawCell(const int row, const int col) const {
  const auto rect = cellRect(row, col);
  const bool selected = row == cursorRow_ && col == cursorCol_;

  if (!revealed_[row][col]) {
    renderer.fillRect(rect.x, rect.y, rect.width, rect.height, true);
    renderer.drawRect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2, false);

    if (flagged_[row][col]) {
      const char* marker = "F";
      const int textX = rect.x + (rect.width - renderer.getTextWidth(UI_12_FONT_ID, marker, EpdFontFamily::BOLD)) / 2;
      const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
      renderer.drawText(UI_12_FONT_ID, textX, textY, marker, false, EpdFontFamily::BOLD);
    }
  } else {
    renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
    renderer.drawRect(rect.x, rect.y, rect.width, rect.height, true);

    if (mines_[row][col]) {
      renderer.fillRect(rect.x + 6, rect.y + 6, rect.width - 12, rect.height - 12, true);
      const char* marker = "M";
      const int textX = rect.x + (rect.width - renderer.getTextWidth(UI_12_FONT_ID, marker, EpdFontFamily::BOLD)) / 2;
      const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
      renderer.drawText(UI_12_FONT_ID, textX, textY, marker, false, EpdFontFamily::BOLD);
    } else if (adjacentCounts_[row][col] > 0) {
      char digit[2] = {static_cast<char>('0' + adjacentCounts_[row][col]), '\0'};
      const int textX = rect.x + (rect.width - renderer.getTextWidth(UI_12_FONT_ID, digit, EpdFontFamily::BOLD)) / 2;
      const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
      renderer.drawText(UI_12_FONT_ID, textX, textY, digit, true, EpdFontFamily::BOLD);
    }
  }

  if (selected) {
    renderer.drawRect(rect.x - 2, rect.y - 2, rect.width + 4, rect.height + 4, true);
    renderer.drawRect(rect.x - 1, rect.y - 1, rect.width + 2, rect.height + 2, true);
  }

  if (row == explodedRow_ && col == explodedCol_) {
    renderer.drawRect(rect.x + 4, rect.y + 4, rect.width - 8, rect.height - 8, false);
  }
}

void MinesweeperActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  const char* subtitle = win_ ? "Board cleared" : (gameOver_ ? "Mine triggered" : "Touch-first puzzle");
  PaperS3Ui::drawScreenHeader(renderer, "Minesweeper", subtitle);
  PaperS3Ui::drawBackButton(renderer);

  const auto bounds = boardRect();
  renderer.drawRect(bounds.x - 2, bounds.y - 2, bounds.width + 4, bounds.height + 4);

  for (int row = 0; row < kBoardHeight; row++) {
    for (int col = 0; col < kBoardWidth; col++) {
      drawCell(row, col);
    }
  }

  int infoY = 180;
  char line[48];
  snprintf(line, sizeof(line), "Mode: %s", mode_ == ToolMode::Reveal ? "Reveal" : "Flag");
  renderer.drawText(UI_12_FONT_ID, kInfoX, infoY, line, true, EpdFontFamily::BOLD);
  infoY += 34;
  snprintf(line, sizeof(line), "Mines: %d", kMineCount);
  renderer.drawText(UI_10_FONT_ID, kInfoX, infoY, line);
  infoY += 26;
  snprintf(line, sizeof(line), "Flags: %d", flagsPlaced_);
  renderer.drawText(UI_10_FONT_ID, kInfoX, infoY, line);
  infoY += 26;
  snprintf(line, sizeof(line), "Open: %d", safeCellsRevealed_);
  renderer.drawText(UI_10_FONT_ID, kInfoX, infoY, line);
  infoY += 26;
  snprintf(line, sizeof(line), "Left: %d", kBoardWidth * kBoardHeight - kMineCount - safeCellsRevealed_);
  renderer.drawText(UI_10_FONT_ID, kInfoX, infoY, line);
  infoY += 38;
  renderer.drawText(UI_10_FONT_ID, kInfoX, infoY, "Tap hidden cells");
  infoY += 22;
  renderer.drawText(UI_10_FONT_ID, kInfoX, infoY, "Tap numbers to chord");
  infoY += 22;
  renderer.drawText(UI_10_FONT_ID, kInfoX, infoY, "Hint reveals safe");

  PaperS3Ui::drawSideActionButton(renderer, false, mode_ == ToolMode::Reveal ? "Flag Mode" : "Reveal Mode");
  PaperS3Ui::drawPrimaryActionButton(renderer, (gameOver_ || win_) ? "Restart" : "New Game");
  PaperS3Ui::drawSideActionButton(renderer, true, "Hint");
  PaperS3Ui::drawFooter(renderer, (gameOver_ || win_) ? "Tap Restart to play again" : "Tap cells directly");
  PaperS3Ui::drawFooterStatus(renderer, statusMessage_.c_str());
  renderer.displayBuffer();
}
