#include "SudokuActivity.h"

// Source reference:
// - Gameplay/source snippet: https://gist.github.com/palaniraja/d5a55f9bd1f990410c8a0099844cec91
// - Local implementation for OmniPaper is maintained in this file.

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"

namespace {
const uint8_t kPuzzle[9][9] = {
    {8, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 3, 6, 0, 0, 0, 0, 0},
    {0, 7, 0, 0, 9, 0, 2, 0, 0},
    {0, 5, 0, 0, 0, 7, 0, 0, 0},
    {0, 0, 0, 0, 4, 5, 7, 0, 0},
    {0, 0, 0, 1, 0, 0, 0, 3, 0},
    {0, 0, 1, 0, 0, 0, 0, 6, 8},
    {0, 0, 8, 5, 0, 0, 0, 1, 0},
    {0, 9, 0, 0, 0, 0, 4, 0, 0},
};
}

SudokuActivity::SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                               const std::function<void()>& onExit)
    : Activity("Sudoku", renderer, mappedInput), onExit(onExit) {}

void SudokuActivity::onEnter() {
  Activity::onEnter();
  for (int y = 0; y < 9; y++) {
    for (int x = 0; x < 9; x++) {
      grid[y][x] = kPuzzle[y][x];
      fixed[y][x] = kPuzzle[y][x] != 0 ? 1 : 0;
    }
  }
  selX = 0;
  selY = 0;
  needsRender = true;
}

void SudokuActivity::loop() {
  handleInput();
  if (needsRender) {
    render();
    needsRender = false;
  }
}

void SudokuActivity::handleInput() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExit) {
      onExit();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    selX = (selX + 8) % 9;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    selX = (selX + 1) % 9;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selY = (selY + 8) % 9;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selY = (selY + 1) % 9;
    needsRender = true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (!fixed[selY][selX]) {
      uint8_t value = grid[selY][selX];
      value = (value + 1) % 10; // 0-9
      grid[selY][selX] = value;
      needsRender = true;
    }
  }
}

void SudokuActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "Sudoku");

  const int screenW = renderer.getScreenWidth();
  const int cellSize = 50;
  const int gridSize = cellSize * 9;
  const int startX = (screenW - gridSize) / 2;
  const int startY = 60;

  for (int y = 0; y < 9; y++) {
    for (int x = 0; x < 9; x++) {
      int xPos = startX + x * cellSize;
      int yPos = startY + y * cellSize;
      renderer.drawRect(xPos, yPos, cellSize, cellSize, true);

      if (x == selX && y == selY) {
        renderer.drawRect(xPos - 2, yPos - 2, cellSize + 4, cellSize + 4, true);
      }

      uint8_t value = grid[y][x];
      if (value != 0) {
        char text[2] = {static_cast<char>('0' + value), '\0'};
        int textX = xPos + (cellSize - renderer.getTextWidth(UI_12_FONT_ID, text)) / 2;
        int textY = yPos + 16;
        renderer.drawText(UI_12_FONT_ID, textX, textY, text, true,
                          fixed[y][x] ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      }
    }
  }

  // Thick grid lines for 3x3 blocks
  for (int i = 0; i <= 9; i += 3) {
    int x = startX + i * cellSize;
    int y = startY + i * cellSize;
    renderer.drawRect(x - 2, startY, 3, gridSize, true);
    renderer.drawRect(startX, y - 2, gridSize, 3, true);
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                            "Confirm: Cycle  Back: Menu");
  renderer.displayBuffer();
}
