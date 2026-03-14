#include "SudokuActivity.h"

// Source reference:
// - Gameplay reference: https://gist.github.com/palaniraja/d5a55f9bd1f990410c8a0099844cec91
// - OmniPaper keeps the touch-first board layout idea, but uses a solver-backed
//   implementation so hints, validation, and reset logic are generated from the
//   local puzzle instead of depending on hard-coded solved grids.

#include <GfxRenderer.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "PaperS3Ui.h"
#include "fontIds.h"

namespace {
constexpr uint8_t kPuzzle[9][9] = {
    {8, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 3, 6, 0, 0, 0, 0, 0}, {0, 7, 0, 0, 9, 0, 2, 0, 0},
    {0, 5, 0, 0, 0, 7, 0, 0, 0}, {0, 0, 0, 0, 4, 5, 7, 0, 0}, {0, 0, 0, 1, 0, 0, 0, 3, 0},
    {0, 0, 1, 0, 0, 0, 0, 6, 8}, {0, 0, 8, 5, 0, 0, 0, 1, 0}, {0, 9, 0, 0, 0, 0, 4, 0, 0},
};

constexpr int kGridCellSize = 42;
constexpr int kGridSize = kGridCellSize * 9;
constexpr int kGridY = 108;

constexpr int kDigitsTopY = 514;
constexpr int kDigitsRowGap = 10;
constexpr int kDigitsButtonHeight = 54;
constexpr int kDigitsColumns = 3;

constexpr int kActionsTopY = 716;
constexpr int kActionColumns = 2;
constexpr int kActionButtonHeight = 48;
constexpr int kActionRowGap = 10;

PaperS3Ui::Rect makeGridRect(const GfxRenderer& renderer) {
  return {(renderer.getScreenWidth() - kGridSize) / 2, kGridY, kGridSize, kGridSize};
}

PaperS3Ui::Rect makeDigitButtonRect(const GfxRenderer& renderer, const int index) {
  constexpr int outerMargin = 24;
  constexpr int gap = 12;
  const int buttonWidth = (renderer.getScreenWidth() - outerMargin * 2 - gap * (kDigitsColumns - 1)) / kDigitsColumns;
  const int row = index / kDigitsColumns;
  const int col = index % kDigitsColumns;
  return {outerMargin + col * (buttonWidth + gap), kDigitsTopY + row * (kDigitsButtonHeight + kDigitsRowGap),
          buttonWidth, kDigitsButtonHeight};
}

PaperS3Ui::Rect makeActionButtonRect(const GfxRenderer& renderer, const int index) {
  constexpr int outerMargin = 24;
  constexpr int gap = 12;
  const int buttonWidth = (renderer.getScreenWidth() - outerMargin * 2 - gap) / kActionColumns;
  const int row = index / kActionColumns;
  const int col = index % kActionColumns;
  return {outerMargin + col * (buttonWidth + gap), kActionsTopY + row * (kActionButtonHeight + kActionRowGap),
          buttonWidth, kActionButtonHeight};
}

void drawButton(GfxRenderer& renderer, const PaperS3Ui::Rect& rect, const char* title, const char* subtitle = nullptr,
                const bool inverted = false) {
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
  renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
  if (inverted) {
    renderer.fillRect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2, true);
  }

  const bool textBlack = !inverted;
  const int titleWidth = renderer.getTextWidth(UI_12_FONT_ID, title, EpdFontFamily::BOLD);
  const int titleX = rect.x + (rect.width - titleWidth) / 2;
  const int titleY = rect.y + (subtitle == nullptr ? 14 : 6);
  renderer.drawText(UI_12_FONT_ID, titleX, titleY, title, textBlack, EpdFontFamily::BOLD);

  if (subtitle != nullptr && subtitle[0] != '\0') {
    const int subtitleWidth = renderer.getTextWidth(SMALL_FONT_ID, subtitle);
    const int subtitleX = rect.x + (rect.width - subtitleWidth) / 2;
    renderer.drawText(SMALL_FONT_ID, subtitleX, rect.y + 27, subtitle, textBlack);
  }
}

const char* actionTitle(const int index, const bool pencilMode) {
  switch (index) {
    case 0:
      return pencilMode ? "Notes On" : "Notes Off";
    case 1:
      return "Clear";
    case 2:
      return "Hint";
    case 3:
      return "Reset";
    default:
      return "";
  }
}

const char* actionSubtitle(const int index, const bool pencilMode) {
  switch (index) {
    case 0:
      return pencilMode ? "Digits add notes" : "Digits write values";
    case 1:
      return "Erase selected cell";
    case 2:
      return "Fill one correct value";
    case 3:
      return "Restart the puzzle";
    default:
      return "";
  }
}
}  // namespace

SudokuActivity::SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                               const std::function<void()>& onExit)
    : Activity("Sudoku", renderer, mappedInput), onExit(onExit) {}

void SudokuActivity::onEnter() {
  Activity::onEnter();

  for (int row = 0; row < 9; row++) {
    for (int col = 0; col < 9; col++) {
      puzzle_[row][col] = kPuzzle[row][col];
      fixed_[row][col] = (kPuzzle[row][col] != 0);
    }
  }

  solution_ = puzzle_;
  if (!solveBoard(solution_)) {
    statusMessage_ = "Puzzle setup failed";
  }

  resetBoardState();
}

void SudokuActivity::loop() {
  handleInput();
  if (needsRender_) {
    render();
    needsRender_ = false;
  }
}

void SudokuActivity::resetBoardState() {
  grid_ = puzzle_;
  for (auto& row : notes_) {
    row.fill(0);
  }

  selectedCol_ = 0;
  selectedRow_ = 0;
  pencilMode_ = false;
  solved_ = false;
  updateStatusMessage();
  needsRender_ = true;
}

void SudokuActivity::selectCell(const int row, const int col) {
  selectedRow_ = row < 0 ? 0 : (row > 8 ? 8 : row);
  selectedCol_ = col < 0 ? 0 : (col > 8 ? 8 : col);
  updateStatusMessage();
  needsRender_ = true;
}

void SudokuActivity::applyDigit(const uint8_t value) {
  if (value < 1 || value > 9 || fixed_[selectedRow_][selectedCol_] || solved_) {
    return;
  }

  if (pencilMode_) {
    if (grid_[selectedRow_][selectedCol_] != 0) {
      statusMessage_ = "Clear the cell before adding notes";
      needsRender_ = true;
      return;
    }

    const uint16_t bit = static_cast<uint16_t>(1U << (value - 1));
    notes_[selectedRow_][selectedCol_] ^= bit;
  } else {
    grid_[selectedRow_][selectedCol_] = value;
    notes_[selectedRow_][selectedCol_] = 0;
  }

  updateStatusMessage();
  needsRender_ = true;
}

void SudokuActivity::clearSelectedCell() {
  if (fixed_[selectedRow_][selectedCol_] || solved_) {
    return;
  }

  grid_[selectedRow_][selectedCol_] = 0;
  notes_[selectedRow_][selectedCol_] = 0;
  updateStatusMessage();
  needsRender_ = true;
}

void SudokuActivity::applyHint() {
  if (fixed_[selectedRow_][selectedCol_] || solved_) {
    return;
  }

  const uint8_t solutionValue = solution_[selectedRow_][selectedCol_];
  if (solutionValue == 0) {
    statusMessage_ = "No hint available";
    needsRender_ = true;
    return;
  }

  grid_[selectedRow_][selectedCol_] = solutionValue;
  notes_[selectedRow_][selectedCol_] = 0;
  updateStatusMessage();
  needsRender_ = true;
}

void SudokuActivity::updateStatusMessage() {
  if (solved_ || boardSolved()) {
    solved_ = true;
    statusMessage_ = "Solved";
    return;
  }

  const uint8_t selectedValue = grid_[selectedRow_][selectedCol_];
  if (selectedValue != 0 && !isCurrentValueValid(selectedRow_, selectedCol_)) {
    statusMessage_ = "Conflict in row, column, or box";
    return;
  }

  if (pencilMode_) {
    statusMessage_ = "Notes mode: tap digits to toggle candidates";
    return;
  }

  if (fixed_[selectedRow_][selectedCol_]) {
    statusMessage_ = "Fixed clue selected";
    return;
  }

  statusMessage_ = "Tap a digit to fill the selected cell";
}

bool SudokuActivity::isPlacementAllowed(const Board& board, const int row, const int col, const uint8_t value) const {
  for (int i = 0; i < 9; i++) {
    if (board[row][i] == value || board[i][col] == value) {
      return false;
    }
  }

  const int boxRow = (row / 3) * 3;
  const int boxCol = (col / 3) * 3;
  for (int y = boxRow; y < boxRow + 3; y++) {
    for (int x = boxCol; x < boxCol + 3; x++) {
      if (board[y][x] == value) {
        return false;
      }
    }
  }

  return true;
}

bool SudokuActivity::solveBoard(Board& board) const {
  int bestRow = -1;
  int bestCol = -1;
  int bestCandidateCount = 10;

  // Choose the empty cell with the fewest legal moves to keep the solver fast
  // enough for on-device startup and hint generation.
  for (int row = 0; row < 9; row++) {
    for (int col = 0; col < 9; col++) {
      if (board[row][col] != 0) {
        continue;
      }

      int candidates = 0;
      for (uint8_t value = 1; value <= 9; value++) {
        if (isPlacementAllowed(board, row, col, value)) {
          candidates++;
        }
      }

      if (candidates == 0) {
        return false;
      }

      if (candidates < bestCandidateCount) {
        bestCandidateCount = candidates;
        bestRow = row;
        bestCol = col;
      }
    }
  }

  if (bestRow == -1) {
    return true;
  }

  for (uint8_t value = 1; value <= 9; value++) {
    if (!isPlacementAllowed(board, bestRow, bestCol, value)) {
      continue;
    }

    board[bestRow][bestCol] = value;
    if (solveBoard(board)) {
      return true;
    }
    board[bestRow][bestCol] = 0;
  }

  return false;
}

bool SudokuActivity::isCurrentValueValid(const int row, const int col) const {
  const uint8_t value = grid_[row][col];
  if (value == 0) {
    return true;
  }

  Board scratch = grid_;
  scratch[row][col] = 0;
  return isPlacementAllowed(scratch, row, col, value);
}

bool SudokuActivity::boardSolved() const {
  for (int row = 0; row < 9; row++) {
    for (int col = 0; col < 9; col++) {
      if (grid_[row][col] == 0 || grid_[row][col] != solution_[row][col]) {
        return false;
      }
    }
  }
  return true;
}

void SudokuActivity::handleInput() {
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

    const auto gridRect = makeGridRect(renderer);
    if (gridRect.contains(tapX, tapY)) {
      const int col = (tapX - gridRect.x) / kGridCellSize;
      const int row = (tapY - gridRect.y) / kGridCellSize;
      selectCell(row, col);
      return;
    }

    for (int i = 0; i < 9; i++) {
      if (makeDigitButtonRect(renderer, i).contains(tapX, tapY)) {
        applyDigit(static_cast<uint8_t>(i + 1));
        return;
      }
    }

    for (int i = 0; i < 4; i++) {
      if (!makeActionButtonRect(renderer, i).contains(tapX, tapY)) {
        continue;
      }

      switch (i) {
        case 0:
          pencilMode_ = !pencilMode_;
          updateStatusMessage();
          needsRender_ = true;
          break;
        case 1:
          clearSelectedCell();
          break;
        case 2:
          applyHint();
          break;
        case 3:
          resetBoardState();
          break;
        default:
          break;
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
    selectCell(selectedRow_, (selectedCol_ + 8) % 9);
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    selectCell(selectedRow_, (selectedCol_ + 1) % 9);
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selectCell((selectedRow_ + 8) % 9, selectedCol_);
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selectCell((selectedRow_ + 1) % 9, selectedCol_);
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) && !fixed_[selectedRow_][selectedCol_] && !solved_) {
    if (pencilMode_) {
      const uint8_t nextCandidate = static_cast<uint8_t>((grid_[selectedRow_][selectedCol_] % 9) + 1);
      applyDigit(nextCandidate);
    } else {
      uint8_t value = grid_[selectedRow_][selectedCol_];
      value = static_cast<uint8_t>((value + 1) % 10);
      if (value == 0) {
        clearSelectedCell();
      } else {
        applyDigit(value);
      }
    }
  }
}

void SudokuActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  PaperS3Ui::drawScreenHeader(renderer, "Sudoku", pencilMode_ ? "Tap a cell, then a note digit" : "Touch-first board");
  PaperS3Ui::drawBackButton(renderer);

  char statusLine[96];
  snprintf(statusLine, sizeof(statusLine), "Cell %c%d", static_cast<char>('A' + selectedCol_), selectedRow_ + 1);
  renderer.drawCenteredText(UI_10_FONT_ID, 82, statusLine);

  const auto gridRect = makeGridRect(renderer);
  const uint8_t selectedValue = grid_[selectedRow_][selectedCol_];
  const int selectedBoxRow = selectedRow_ / 3;
  const int selectedBoxCol = selectedCol_ / 3;

  for (int row = 0; row < 9; row++) {
    for (int col = 0; col < 9; col++) {
      const int x = gridRect.x + col * kGridCellSize;
      const int y = gridRect.y + row * kGridCellSize;
      const bool isSelected = (row == selectedRow_ && col == selectedCol_);
      const bool sameBox = (row / 3 == selectedBoxRow && col / 3 == selectedBoxCol);
      const bool related = (row == selectedRow_ || col == selectedCol_ || sameBox);
      const bool sameDigit = (selectedValue != 0 && !isSelected && grid_[row][col] == selectedValue);
      const bool invalid = (grid_[row][col] != 0 && !isCurrentValueValid(row, col));

      renderer.fillRect(x, y, kGridCellSize, kGridCellSize, false);
      renderer.drawRect(x, y, kGridCellSize, kGridCellSize, true);

      if (related && !isSelected) {
        renderer.drawRect(x + 2, y + 2, kGridCellSize - 4, kGridCellSize - 4, true);
      }

      if (isSelected) {
        renderer.fillRect(x + 1, y + 1, kGridCellSize - 2, kGridCellSize - 2, true);
        renderer.drawRect(x - 2, y - 2, kGridCellSize + 4, kGridCellSize + 4, true);
      }

      if (sameDigit) {
        renderer.drawLine(x + 6, y + kGridCellSize - 7, x + kGridCellSize - 7, y + kGridCellSize - 7, !isSelected);
      }

      if (invalid) {
        renderer.drawLine(x + 6, y + 6, x + kGridCellSize - 7, y + kGridCellSize - 7, !isSelected);
        renderer.drawLine(x + 6, y + kGridCellSize - 7, x + kGridCellSize - 7, y + 6, !isSelected);
      }

      if (fixed_[row][col]) {
        renderer.fillRect(x + kGridCellSize - 10, y + 3, 5, 5, !isSelected);
      }

      const uint8_t value = grid_[row][col];
      if (value != 0) {
        char digit[2] = {static_cast<char>('0' + value), '\0'};
        const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, digit,
                                                    fixed_[row][col] ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
        const int textX = x + (kGridCellSize - textWidth) / 2;
        const int textY = y + 10;
        renderer.drawText(UI_12_FONT_ID, textX, textY, digit, !isSelected,
                          fixed_[row][col] ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      } else if (notes_[row][col] != 0) {
        for (int note = 1; note <= 9; note++) {
          if ((notes_[row][col] & (1U << (note - 1))) == 0) {
            continue;
          }

          const int miniCol = (note - 1) % 3;
          const int miniRow = (note - 1) / 3;
          char noteText[2] = {static_cast<char>('0' + note), '\0'};
          const int miniCellSize = 12;
          const int noteX = x + 5 + miniCol * miniCellSize;
          const int noteY = y + 4 + miniRow * miniCellSize;
          renderer.drawText(SMALL_FONT_ID, noteX, noteY, noteText, !isSelected);
        }
      }
    }
  }

  for (int i = 0; i <= 9; i += 3) {
    const int x = gridRect.x + i * kGridCellSize;
    const int y = gridRect.y + i * kGridCellSize;
    renderer.fillRect(x - 1, gridRect.y, 3, kGridSize, true);
    renderer.fillRect(gridRect.x, y - 1, kGridSize, 3, true);
  }

  for (int i = 0; i < 9; i++) {
    const auto rect = makeDigitButtonRect(renderer, i);
    char digit[2] = {static_cast<char>('1' + i), '\0'};
    drawButton(renderer, rect, digit);
  }

  for (int i = 0; i < 4; i++) {
    drawButton(renderer, makeActionButtonRect(renderer, i), actionTitle(i, pencilMode_), actionSubtitle(i, pencilMode_),
               (i == 0 && pencilMode_));
  }

  PaperS3Ui::drawFooterStatus(renderer, statusMessage_.c_str());
  PaperS3Ui::drawFooter(renderer, solved_ ? "Puzzle complete" : "Tap a cell, then tap a control");
  renderer.displayBuffer();
}
