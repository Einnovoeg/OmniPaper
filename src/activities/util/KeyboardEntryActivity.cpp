#include "KeyboardEntryActivity.h"

#include "MappedInputManager.h"
#include "../apps/PaperS3Ui.h"
#include "fontIds.h"

// Keyboard layouts - lowercase
const char* const KeyboardEntryActivity::keyboard[NUM_ROWS] = {
    "`1234567890-=", "qwertyuiop[]\\", "asdfghjkl;'", "zxcvbnm,./",
    "^  _____<OK"  // ^ = shift, _ = space, < = backspace, OK = done
};

// Keyboard layouts - uppercase/symbols
const char* const KeyboardEntryActivity::keyboardShift[NUM_ROWS] = {"~!@#$%^&*()_+", "QWERTYUIOP{}|", "ASDFGHJKL:\"",
                                                                    "ZXCVBNM<>?", "SPECIAL ROW"};

namespace {
constexpr int kInputBoxX = 24;
constexpr int kInputBoxY = 92;
constexpr int kInputBoxWidth = 492;
constexpr int kInputBoxHeight = 96;
constexpr int kKeyboardStartY = 220;
constexpr int kKeyUnitWidth = 34;
constexpr int kKeyHeight = 52;
constexpr int kKeyGap = 4;
constexpr int kKeyboardLeft = 27;
constexpr int kNumRows = 5;
constexpr int kSpecialRow = 4;
constexpr int kShiftCol = 0;
constexpr int kSpaceCol = 2;
constexpr int kBackspaceCol = 7;
constexpr int kDoneCol = 9;
constexpr int kKeysPerRow = 13;

PaperS3Ui::Rect keyboardKeyRect(const int row, const int logicalCol) {
  PaperS3Ui::Rect rect;
  rect.y = kKeyboardStartY + row * (kKeyHeight + kKeyGap);
  rect.height = kKeyHeight;

  if (row == kSpecialRow) {
    if (logicalCol < kSpaceCol) {
      rect.x = kKeyboardLeft;
      rect.width = kKeyUnitWidth * 2 + kKeyGap;
      return rect;
    }
    if (logicalCol < kBackspaceCol) {
      rect.x = kKeyboardLeft + 2 * (kKeyUnitWidth + kKeyGap);
      rect.width = kKeyUnitWidth * 5 + kKeyGap * 4;
      return rect;
    }
    if (logicalCol < kDoneCol) {
      rect.x = kKeyboardLeft + 7 * (kKeyUnitWidth + kKeyGap);
      rect.width = kKeyUnitWidth * 2 + kKeyGap;
      return rect;
    }
    rect.x = kKeyboardLeft + 9 * (kKeyUnitWidth + kKeyGap);
    rect.width = kKeyUnitWidth * 2 + kKeyGap;
    return rect;
  }

  rect.x = kKeyboardLeft + logicalCol * (kKeyUnitWidth + kKeyGap);
  rect.width = kKeyUnitWidth;
  return rect;
}

bool hitKeyboard(int x, int y, int& rowOut, int& colOut) {
  for (int row = 0; row < kNumRows; row++) {
    const int rowLength = (row == kSpecialRow) ? 4 : kKeysPerRow;
    for (int i = 0; i < rowLength; i++) {
      int logicalCol = i;
      if (row == kSpecialRow) {
        logicalCol = (i == 0) ? kShiftCol : (i == 1) ? kSpaceCol : (i == 2) ? kBackspaceCol : kDoneCol;
      }
      const auto rect = keyboardKeyRect(row, logicalCol);
      if (rect.contains(x, y)) {
        rowOut = row;
        colOut = logicalCol;
        return true;
      }
    }
  }
  return false;
}
}  // namespace

void KeyboardEntryActivity::taskTrampoline(void* param) {
  auto* self = static_cast<KeyboardEntryActivity*>(param);
  self->displayTaskLoop();
}

void KeyboardEntryActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      render();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void KeyboardEntryActivity::onEnter() {
  Activity::onEnter();

  selectedRow = 0;
  selectedCol = 0;
  shiftActive = false;
  updateRequired = true;

#if !defined(PLATFORM_M5PAPERS3)
  renderingMutex = xSemaphoreCreateMutex();
  xTaskCreate(&KeyboardEntryActivity::taskTrampoline, "KeyboardEntryActivity",
              2048,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
#endif
}

void KeyboardEntryActivity::onExit() {
  Activity::onExit();

#if !defined(PLATFORM_M5PAPERS3)
  // PaperS3 renders inline in the main activity loop to avoid a second UI task
  // competing with touch handling and e-paper refreshes.
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
#endif
}

int KeyboardEntryActivity::getRowLength(const int row) const {
  if (row < 0 || row >= NUM_ROWS) return 0;

  // Return actual length of each row based on keyboard layout
  switch (row) {
    case 0:
      return 13;  // `1234567890-=
    case 1:
      return 13;  // qwertyuiop[]backslash
    case 2:
      return 11;  // asdfghjkl;'
    case 3:
      return 10;  // zxcvbnm,./
    case 4:
      return 10;  // shift (2 wide), space (5 wide), backspace (2 wide), OK
    default:
      return 0;
  }
}

char KeyboardEntryActivity::getSelectedChar() const {
  const char* const* layout = shiftActive ? keyboardShift : keyboard;

  if (selectedRow < 0 || selectedRow >= NUM_ROWS) return '\0';
  if (selectedCol < 0 || selectedCol >= getRowLength(selectedRow)) return '\0';

  return layout[selectedRow][selectedCol];
}

void KeyboardEntryActivity::handleKeyPress() {
  // Handle special row (bottom row with shift, space, backspace, done)
  if (selectedRow == SPECIAL_ROW) {
    if (selectedCol >= SHIFT_COL && selectedCol < SPACE_COL) {
      // Shift toggle
      shiftActive = !shiftActive;
      return;
    }

    if (selectedCol >= SPACE_COL && selectedCol < BACKSPACE_COL) {
      // Space bar
      if (maxLength == 0 || text.length() < maxLength) {
        text += ' ';
      }
      return;
    }

    if (selectedCol >= BACKSPACE_COL && selectedCol < DONE_COL) {
      // Backspace
      if (!text.empty()) {
        text.pop_back();
      }
      return;
    }

    if (selectedCol >= DONE_COL) {
      // Done button
      if (onComplete) {
        onComplete(text);
      }
      return;
    }
  }

  // Regular character
  const char c = getSelectedChar();
  if (c == '\0') {
    return;
  }

  if (maxLength == 0 || text.length() < maxLength) {
    text += c;
    // Auto-disable shift after typing a letter
    if (shiftActive && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
      shiftActive = false;
    }
  }
}

void KeyboardEntryActivity::loop() {
#if defined(PLATFORM_M5PAPERS3)
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasTapped() && PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), tapX, tapY)) {
    if (PaperS3Ui::backButtonRect(renderer).contains(tapX, tapY)) {
      if (onCancel) {
        onCancel();
      }
      return;
    }

    int tappedRow = 0;
    int tappedCol = 0;
    if (hitKeyboard(tapX, tapY, tappedRow, tappedCol)) {
      selectedRow = tappedRow;
      selectedCol = tappedCol;
      handleKeyPress();
      updateRequired = true;
      return;
    }
  }
#endif

  // Navigation
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    if (selectedRow > 0) {
      selectedRow--;
      // Clamp column to valid range for new row
      const int maxCol = getRowLength(selectedRow) - 1;
      if (selectedCol > maxCol) selectedCol = maxCol;
    } else {
      // Wrap to bottom row
      selectedRow = NUM_ROWS - 1;
      const int maxCol = getRowLength(selectedRow) - 1;
      if (selectedCol > maxCol) selectedCol = maxCol;
    }
    updateRequired = true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    if (selectedRow < NUM_ROWS - 1) {
      selectedRow++;
      const int maxCol = getRowLength(selectedRow) - 1;
      if (selectedCol > maxCol) selectedCol = maxCol;
    } else {
      // Wrap to top row
      selectedRow = 0;
      const int maxCol = getRowLength(selectedRow) - 1;
      if (selectedCol > maxCol) selectedCol = maxCol;
    }
    updateRequired = true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    const int maxCol = getRowLength(selectedRow) - 1;

    // Special bottom row case
    if (selectedRow == SPECIAL_ROW) {
      // Bottom row has special key widths
      if (selectedCol >= SHIFT_COL && selectedCol < SPACE_COL) {
        // In shift key, wrap to end of row
        selectedCol = maxCol;
      } else if (selectedCol >= SPACE_COL && selectedCol < BACKSPACE_COL) {
        // In space bar, move to shift
        selectedCol = SHIFT_COL;
      } else if (selectedCol >= BACKSPACE_COL && selectedCol < DONE_COL) {
        // In backspace, move to space
        selectedCol = SPACE_COL;
      } else if (selectedCol >= DONE_COL) {
        // At done button, move to backspace
        selectedCol = BACKSPACE_COL;
      }
      updateRequired = true;
      return;
    }

    if (selectedCol > 0) {
      selectedCol--;
    } else {
      // Wrap to end of current row
      selectedCol = maxCol;
    }
    updateRequired = true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    const int maxCol = getRowLength(selectedRow) - 1;

    // Special bottom row case
    if (selectedRow == SPECIAL_ROW) {
      // Bottom row has special key widths
      if (selectedCol >= SHIFT_COL && selectedCol < SPACE_COL) {
        // In shift key, move to space
        selectedCol = SPACE_COL;
      } else if (selectedCol >= SPACE_COL && selectedCol < BACKSPACE_COL) {
        // In space bar, move to backspace
        selectedCol = BACKSPACE_COL;
      } else if (selectedCol >= BACKSPACE_COL && selectedCol < DONE_COL) {
        // In backspace, move to done
        selectedCol = DONE_COL;
      } else if (selectedCol >= DONE_COL) {
        // At done button, wrap to beginning of row
        selectedCol = SHIFT_COL;
      }
      updateRequired = true;
      return;
    }

    if (selectedCol < maxCol) {
      selectedCol++;
    } else {
      // Wrap to beginning of current row
      selectedCol = 0;
    }
    updateRequired = true;
  }

  // Selection
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleKeyPress();
    updateRequired = true;
  }

  // Cancel
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onCancel) {
      onCancel();
    }
    updateRequired = true;
  }

#if defined(PLATFORM_M5PAPERS3)
  if (updateRequired) {
    render();
    updateRequired = false;
  }
#endif
}

void KeyboardEntryActivity::render() const {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  PaperS3Ui::drawScreenHeader(const_cast<GfxRenderer&>(renderer), title.c_str(), isPassword ? "Password input" : "");
  PaperS3Ui::drawBackButton(const_cast<GfxRenderer&>(renderer), "Cancel");

  renderer.drawRect(kInputBoxX, kInputBoxY, kInputBoxWidth, kInputBoxHeight);

  std::string displayText;
  if (isPassword) {
    displayText = std::string(text.length(), '*');
  } else {
    displayText = text;
  }

  // Show cursor at end
  displayText += "_";

  // Render input text across multiple lines
  int textY = kInputBoxY + 14;
  int lineStartIdx = 0;
  int lineEndIdx = displayText.length();
  int linesDrawn = 0;
  while (lineStartIdx < static_cast<int>(displayText.length()) && linesDrawn < 3) {
    std::string lineText = displayText.substr(lineStartIdx, lineEndIdx - lineStartIdx);
    const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, lineText.c_str());
    if (textWidth <= (kInputBoxWidth - 24)) {
      renderer.drawText(UI_10_FONT_ID, kInputBoxX + 12, textY, lineText.c_str());
      textY += renderer.getLineHeight(UI_10_FONT_ID) + 4;
      lineStartIdx = lineEndIdx;
      lineEndIdx = displayText.length();
      linesDrawn++;
    } else {
      lineEndIdx -= 1;
    }
  }

  const char* const* layout = shiftActive ? keyboardShift : keyboard;

  for (int row = 0; row < NUM_ROWS; row++) {
    if (row == 4) {
      const bool shiftSelected = (selectedRow == 4 && selectedCol >= SHIFT_COL && selectedCol < SPACE_COL);
      renderItemWithSelector(keyboardKeyRect(row, SHIFT_COL).x, keyboardKeyRect(row, SHIFT_COL).y,
                             keyboardKeyRect(row, SHIFT_COL).width, keyboardKeyRect(row, SHIFT_COL).height,
                             shiftActive ? "SHIFT" : "shift", shiftSelected);
      const bool spaceSelected = (selectedRow == 4 && selectedCol >= SPACE_COL && selectedCol < BACKSPACE_COL);
      renderItemWithSelector(keyboardKeyRect(row, SPACE_COL).x, keyboardKeyRect(row, SPACE_COL).y,
                             keyboardKeyRect(row, SPACE_COL).width, keyboardKeyRect(row, SPACE_COL).height,
                             "Space", spaceSelected);
      const bool bsSelected = (selectedRow == 4 && selectedCol >= BACKSPACE_COL && selectedCol < DONE_COL);
      renderItemWithSelector(keyboardKeyRect(row, BACKSPACE_COL).x, keyboardKeyRect(row, BACKSPACE_COL).y,
                             keyboardKeyRect(row, BACKSPACE_COL).width, keyboardKeyRect(row, BACKSPACE_COL).height,
                             "Del", bsSelected);
      const bool okSelected = (selectedRow == 4 && selectedCol >= DONE_COL);
      renderItemWithSelector(keyboardKeyRect(row, DONE_COL).x, keyboardKeyRect(row, DONE_COL).y,
                             keyboardKeyRect(row, DONE_COL).width, keyboardKeyRect(row, DONE_COL).height, "Done",
                             okSelected);
    } else {
      for (int col = 0; col < getRowLength(row); col++) {
        const char c = layout[row][col];
        std::string keyLabel(1, c);
        const bool isSelected = row == selectedRow && col == selectedCol;
        const auto rect = keyboardKeyRect(row, col);
        renderItemWithSelector(rect.x, rect.y, rect.width, rect.height, keyLabel.c_str(), isSelected);
      }
    }
  }

  PaperS3Ui::drawFooter(const_cast<GfxRenderer&>(renderer), "Tap keys directly");

  renderer.displayBuffer();
}

void KeyboardEntryActivity::renderItemWithSelector(const int x, const int y, const int width, const int height,
                                                   const char* item, const bool isSelected) const {
  renderer.fillRect(x, y, width, height, false);
  renderer.drawRect(x, y, width, height);
  if (isSelected) {
    renderer.fillRect(x + 1, y + 1, width - 2, height - 2, true);
  }
  const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, item);
  const int textX = x + (width - textWidth) / 2;
  const int textY = y + (height - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
  renderer.drawText(UI_10_FONT_ID, textX, textY, item, !isSelected);
}
