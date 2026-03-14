#include "PoodleActivity.h"

// Source reference:
// - Gameplay/source project: https://github.com/k-natori/Poodle
// - Local PaperS3 implementation for OmniPaper is maintained in this file.

#include <Arduino.h>
#include <GfxRenderer.h>
#include <SDCardManager.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>

#include "MappedInputManager.h"
#include "PaperS3Ui.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"

namespace {
const char* kWordFile = "/games/poodle_words.txt";
const char* kKeyboardRows[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
constexpr int kKeyboardRowCount = 3;
constexpr int kGridTopY = 124;
constexpr int kGridCellSize = 70;
constexpr int kGridGap = 10;
constexpr int kKeyboardTopY = 622;
constexpr int kKeyboardGap = 8;
constexpr int kKeyboardRowGap = 12;
constexpr int kKeyboardHeight = 58;
constexpr int kGameWordLength = 5;
const char* kBuiltinWords[] = {
    "ABOUT", "ABOVE", "ACORN", "ACTOR", "ADULT", "AGENT", "AGREE", "ALARM", "ALBUM", "ALERT", "ALIEN", "ALIVE", "AMBER",
    "AMONG", "ANGLE", "APPLE", "APRIL", "ARENA", "ARGON", "ARISE", "ARMOR", "ARROW", "ASIDE", "ATLAS", "AUDIO", "AWARE",
    "AWOKE", "BADGE", "BASIC", "BASIL", "BATCH", "BEACH", "BEGAN", "BEGIN", "BEING", "BENCH", "BERRY", "BIRTH", "BLEND",
    "BLINK", "BLOOM", "BLUNT", "BOARD", "BOOST", "BRAIN", "BRAVE", "BREAD", "BRICK", "BRING", "BRUSH", "BUILD", "BUNCH",
    "BURST", "CABLE", "CALMS", "CANDY", "CARRY", "CHAIN", "CHAIR", "CHART", "CHESS", "CHIEF", "CHOIR", "CHORD", "CIVIC",
    "CLOUD", "COAST", "COLOR", "CORAL", "COVER", "CRANE", "CRISP", "CROSS", "CROWN", "DANCE", "DEALT", "DELTA", "DEPTH",
    "DIARY", "DRIFT", "EAGER", "EARTH", "EIGHT", "ELBOW", "ELDER", "EMBER", "ENJOY", "ENTER", "EPOCH", "EQUAL", "ERROR",
    "EVENT", "EXTRA", "FAITH", "FALSE", "FIELD", "FINAL", "FIVER", "FLOAT", "FOCUS", "FORGE", "FRAME", "FRESH", "FRONT",
    "FRUIT", "GHOST", "GLASS", "GLIDE", "GLOVE", "GRACE", "GRADE", "GRAIN", "GRAPH", "GREEN", "GROUP", "GUIDE", "HAPPY",
    "HEART", "HONEY", "HORSE", "HOUSE", "IMAGE", "INDEX", "INNER", "INPUT", "ISSUE", "JUICE", "KNIFE", "LAYER", "LEMON",
    "LIGHT", "LIMIT", "LOCAL", "MAGIC", "MAJOR", "MANGO", "MAPLE", "MARCH", "MATCH", "METAL", "MODEL", "MONEY", "MORAL",
    "MOUSE", "MUSIC", "NEVER", "NORTH", "NOVEL", "NURSE", "OCEAN", "OFFER", "OPERA", "ORDER", "OTHER", "PAINT", "PANEL",
    "PAPER", "PARTY", "PASTE", "PAUSE", "PEARL", "PHASE", "PHONE", "PIANO", "PLAIN", "PLANE", "PLANT", "PLATE", "POINT",
    "POWER", "PRESS", "PRINT", "PRIZE", "PROUD", "QUERY", "QUIET", "RADIO", "RAINY", "RANGE", "RATIO", "REACH", "READY",
    "REPLY", "RIDER", "RIVER", "ROBIN", "ROUTE", "ROYAL", "RUGBY", "SCALE", "SCENE", "SCOPE", "SCORE", "SEVEN", "SHADE",
    "SHARE", "SHIFT", "SHINE", "SHORE", "SHORT", "SIGHT", "SILKY", "SIXTH", "SKILL", "SLEEP", "SMILE", "SOLAR", "SOLID",
    "SOUND", "SOUTH", "SPEED", "SPICE", "SPOKE", "STACK", "STONE", "STORY", "STUDY", "STYLE", "SUGAR", "SUNNY", "SWIFT",
    "TABLE", "TEACH", "THANK", "THEME", "THREE", "TITLE", "TODAY", "TOKEN", "TOUCH", "TRACK", "TRADE", "TRAIN", "TRAIL",
    "TRUST", "TRUTH", "UNITY", "URBAN", "VIVID", "VOICE", "WATER", "WHEEL", "WHITE", "WHOLE", "WINDY", "WOMAN", "WORLD",
    "YOUNG"};

PaperS3Ui::Rect gridCellRect(const GfxRenderer& renderer, const int row, const int col) {
  const int totalWidth = kGameWordLength * kGridCellSize + (kGameWordLength - 1) * kGridGap;
  const int startX = (renderer.getScreenWidth() - totalWidth) / 2;
  return {startX + col * (kGridCellSize + kGridGap), kGridTopY + row * (kGridCellSize + kGridGap), kGridCellSize,
          kGridCellSize};
}

PaperS3Ui::Rect keyboardKeyRect(const GfxRenderer& renderer, const int row, const int col) {
  const int rowLength = static_cast<int>(strlen(kKeyboardRows[row]));
  const int maxWidth = renderer.getScreenWidth() - PaperS3Ui::kOuterMargin * 2;
  const int keyWidth = std::min(58, (maxWidth - (rowLength - 1) * kKeyboardGap) / rowLength);
  const int totalWidth = rowLength * keyWidth + (rowLength - 1) * kKeyboardGap;
  const int startX = (renderer.getScreenWidth() - totalWidth) / 2;
  return {startX + col * (keyWidth + kKeyboardGap), kKeyboardTopY + row * (kKeyboardHeight + kKeyboardRowGap), keyWidth,
          kKeyboardHeight};
}

PoodleActivity::LetterState strongest(PoodleActivity::LetterState lhs, PoodleActivity::LetterState rhs) {
  return static_cast<int>(rhs) > static_cast<int>(lhs) ? rhs : lhs;
}
}  // namespace

PoodleActivity::PoodleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                               const std::function<void()>& onExit)
    : ActivityWithSubactivity("Poodle", renderer, mappedInput), onExit(onExit) {}

void PoodleActivity::onEnter() {
  Activity::onEnter();
  loadWords();
  startNewGame();
  needsRender = true;
}

void PoodleActivity::loop() {
  if (subActivity) {
    ActivityWithSubactivity::loop();
    return;
  }

  handleInput();
  if (needsRender) {
    render();
    needsRender = false;
  }
}

void PoodleActivity::loadWords() {
  wordList.clear();
  usingSdWordList = false;

  // Prefer an SD-backed dictionary so larger word lists can be added without
  // inflating the firmware image. The built-in list keeps the game usable when
  // the card is missing or the file has not been installed yet.
  if (SdMan.ready()) {
    String data = SdMan.readFile(kWordFile);
    if (data.length() > 0) {
      int start = 0;
      while (start < data.length()) {
        int end = data.indexOf('\n', start);
        if (end < 0) {
          end = data.length();
        }
        String line = data.substring(start, end);
        line.trim();
        line.toUpperCase();
        bool valid = line.length() == kWordLength;
        for (int i = 0; valid && i < line.length(); i++) {
          valid = std::isalpha(static_cast<unsigned char>(line[i])) != 0;
        }
        if (valid) {
          wordList.emplace_back(line.c_str());
        }
        start = end + 1;
      }
      usingSdWordList = !wordList.empty();
    }
  }

  if (wordList.empty()) {
    for (const auto& word : kBuiltinWords) {
      wordList.emplace_back(word);
    }
  }

  std::sort(wordList.begin(), wordList.end());
  wordList.erase(std::unique(wordList.begin(), wordList.end()), wordList.end());
}

void PoodleActivity::pickTarget() {
  if (wordList.empty()) {
    targetWord = "PAPER";
    return;
  }
  const size_t index = static_cast<size_t>(millis()) % wordList.size();
  targetWord = wordList[index];
}

void PoodleActivity::startNewGame() {
  guesses.clear();
  guessStates.clear();
  currentGuess.assign(kWordLength, ' ');
  std::fill(keyboardStates.begin(), keyboardStates.end(), LetterState::Unknown);
  cursorPos = 0;
  gameOver = false;
  won = false;
  pickTarget();
  setStatus(usingSdWordList ? "Dictionary loaded from SD card" : "Using the built-in dictionary", false);
}

void PoodleActivity::setStatus(const std::string& message, const bool isError) {
  statusMessage = message;
  statusError = isError;
}

void PoodleActivity::clearGuess() {
  currentGuess.assign(kWordLength, ' ');
  cursorPos = 0;
}

void PoodleActivity::deleteLetter() {
  if (currentGuess[cursorPos] != ' ') {
    currentGuess[cursorPos] = ' ';
    return;
  }
  for (int i = cursorPos - 1; i >= 0; i--) {
    if (currentGuess[i] != ' ') {
      cursorPos = i;
      currentGuess[i] = ' ';
      return;
    }
  }
}

void PoodleActivity::insertLetter(const char letter) {
  currentGuess[cursorPos] = letter;
  for (int i = cursorPos + 1; i < kWordLength; i++) {
    if (currentGuess[i] == ' ') {
      cursorPos = i;
      return;
    }
  }
  cursorPos = std::min(cursorPos + 1, kWordLength - 1);
}

bool PoodleActivity::isGuessComplete() const {
  return std::none_of(currentGuess.begin(), currentGuess.end(), [](const char c) { return c == ' '; });
}

bool PoodleActivity::isValidGuess(const std::string& guess) const {
  return std::binary_search(wordList.begin(), wordList.end(), guess);
}

std::array<PoodleActivity::LetterState, PoodleActivity::kWordLength> PoodleActivity::evaluateGuess(
    const std::string& guess) {
  std::array<LetterState, kWordLength> result{};
  std::array<int, 26> remaining{};

  // Two-pass scoring preserves duplicate-letter behaviour. Exact hits consume
  // the target pool first, then remaining letters can score as misplaced.
  for (int i = 0; i < kWordLength; i++) {
    if (guess[i] == targetWord[i]) {
      result[i] = LetterState::Correct;
    } else {
      result[i] = LetterState::Absent;
      remaining[targetWord[i] - 'A']++;
    }
  }

  for (int i = 0; i < kWordLength; i++) {
    if (result[i] == LetterState::Correct) {
      continue;
    }
    const int index = guess[i] - 'A';
    if (remaining[index] > 0) {
      result[i] = LetterState::Present;
      remaining[index]--;
    }
  }
  return result;
}

void PoodleActivity::promoteKeyboardState(const char letter, const LetterState state) {
  if (letter < 'A' || letter > 'Z') {
    return;
  }
  const size_t index = static_cast<size_t>(letter - 'A');
  keyboardStates[index] = strongest(keyboardStates[index], state);
}

char PoodleActivity::nextLetter(char c, int delta) const {
  if (c < 'A' || c > 'Z') {
    c = 'A';
  }
  int offset = c - 'A';
  offset = (offset + delta + 26) % 26;
  return static_cast<char>('A' + offset);
}

void PoodleActivity::handleInput() {
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

    if (gameOver) {
      if (PaperS3Ui::sideActionRect(renderer, false).contains(tapX, tapY)) {
        if (onExit) {
          onExit();
        }
        return;
      }
      if (PaperS3Ui::primaryActionRect(renderer).contains(tapX, tapY)) {
        startNewGame();
        needsRender = true;
        return;
      }
      if (PaperS3Ui::sideActionRect(renderer, true).contains(tapX, tapY)) {
        setStatus(won ? "Solved cleanly" : "Answer: " + targetWord, false);
        needsRender = true;
        return;
      }
    } else {
      const int activeRow = static_cast<int>(guesses.size());
      for (int col = 0; col < kWordLength; col++) {
        if (gridCellRect(renderer, activeRow, col).contains(tapX, tapY)) {
          cursorPos = col;
          needsRender = true;
          return;
        }
      }

      for (int row = 0; row < kKeyboardRowCount; row++) {
        const int rowLength = static_cast<int>(strlen(kKeyboardRows[row]));
        for (int col = 0; col < rowLength; col++) {
          if (!keyboardKeyRect(renderer, row, col).contains(tapX, tapY)) {
            continue;
          }
          insertLetter(kKeyboardRows[row][col]);
          setStatus("Tap Enter to submit the current guess", false);
          needsRender = true;
          return;
        }
      }

      if (PaperS3Ui::sideActionRect(renderer, false).contains(tapX, tapY)) {
        clearGuess();
        setStatus("Current guess cleared", false);
        needsRender = true;
        return;
      }
      if (PaperS3Ui::sideActionRect(renderer, true).contains(tapX, tapY)) {
        deleteLetter();
        setStatus("Deleted the previous letter", false);
        needsRender = true;
        return;
      }
      if (PaperS3Ui::primaryActionRect(renderer).contains(tapX, tapY)) {
        submitGuess();
        needsRender = true;
        return;
      }
    }
  }
#endif

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    bool hasLetters = std::any_of(currentGuess.begin(), currentGuess.end(), [](const char c) { return c != ' '; });
    if (!gameOver && hasLetters) {
      deleteLetter();
      setStatus("Deleted the previous letter", false);
    } else if (onExit) {
      onExit();
      return;
    }
    needsRender = true;
    return;
  }

  if (gameOver) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      startNewGame();
      needsRender = true;
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    cursorPos = (cursorPos - 1 + kWordLength) % kWordLength;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    cursorPos = (cursorPos + 1) % kWordLength;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    currentGuess[cursorPos] = nextLetter(currentGuess[cursorPos], 1);
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    currentGuess[cursorPos] = nextLetter(currentGuess[cursorPos], -1);
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    submitGuess();
    needsRender = true;
  }
}

void PoodleActivity::launchKeyboardEntry() {
  std::string initialText;
  initialText.reserve(kWordLength);
  for (char c : currentGuess) {
    if (c >= 'A' && c <= 'Z') {
      initialText.push_back(c);
    }
  }

  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, "Enter Guess", initialText, 12, kWordLength, false,
      [this](const std::string& text) {
        std::string cleaned;
        cleaned.reserve(kWordLength);
        for (char c : text) {
          if (c >= 'a' && c <= 'z') {
            cleaned.push_back(static_cast<char>(c - ('a' - 'A')));
          } else if (c >= 'A' && c <= 'Z') {
            cleaned.push_back(c);
          }
          if (cleaned.size() >= kWordLength) {
            break;
          }
        }

        clearGuess();
        for (size_t i = 0; i < cleaned.size(); i++) {
          currentGuess[i] = cleaned[i];
        }
        cursorPos = cleaned.empty() ? 0 : static_cast<int>(std::min(cleaned.size(), static_cast<size_t>(kWordLength - 1)));
        exitActivity();
        needsRender = true;
      },
      [this]() {
        exitActivity();
        needsRender = true;
      }));
}

void PoodleActivity::submitGuess() {
  if (!isGuessComplete()) {
#if !defined(PLATFORM_M5PAPERS3)
    launchKeyboardEntry();
#endif
    setStatus("Enter all 5 letters before submitting", true);
    return;
  }

  if (!isValidGuess(currentGuess)) {
    setStatus("That word is not in the dictionary", true);
    return;
  }

  guesses.push_back(currentGuess);
  const auto state = evaluateGuess(currentGuess);
  guessStates.push_back(state);
  for (int i = 0; i < kWordLength; i++) {
    promoteKeyboardState(currentGuess[i], state[i]);
  }

  if (currentGuess == targetWord) {
    won = true;
    gameOver = true;
    setStatus("Solved in " + std::to_string(guesses.size()) + " tries", false);
  } else if (static_cast<int>(guesses.size()) >= kMaxAttempts) {
    gameOver = true;
    setStatus("Out of guesses", true);
  } else {
    setStatus("Attempt " + std::to_string(guesses.size() + 1) + " of 6", false);
  }

  if (!gameOver) {
    clearGuess();
  }
}

void PoodleActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  PaperS3Ui::drawScreenHeader(renderer, "Poodle", "Touch-first PaperS3 word puzzle");
  PaperS3Ui::drawBackButton(renderer);

  std::string attemptLine;
  if (gameOver) {
    attemptLine = won ? "Puzzle solved" : "No guesses left";
  } else {
    attemptLine = "Attempt " + std::to_string(guesses.size() + 1) + " of 6";
  }
  renderer.drawCenteredText(NOTOSANS_14_FONT_ID, 100, attemptLine.c_str(), true, EpdFontFamily::BOLD);

  drawGrid();
  drawLegend();
  drawKeyboard();
  drawActionButtons();

  if (!statusMessage.empty()) {
    PaperS3Ui::drawFooterStatus(renderer, statusMessage.c_str(), statusError);
  }

  if (gameOver) {
    const std::string footer = won ? "Tap New Game to start another round" : "Tap Reveal to show the answer";
    PaperS3Ui::drawFooter(renderer, footer.c_str());
  } else {
    PaperS3Ui::drawFooter(renderer, "Tap a cell to move the cursor, then tap letters to fill it");
  }

  renderer.displayBuffer();
}

void PoodleActivity::drawGrid() {
  const int activeRow = static_cast<int>(guesses.size());
  for (int row = 0; row < kMaxAttempts; row++) {
    for (int col = 0; col < kWordLength; col++) {
      const auto rect = gridCellRect(renderer, row, col);
      const bool currentRow = row == activeRow && !gameOver;
      const bool activeCell = currentRow && col == cursorPos;
      char letter = ' ';
      LetterState state = LetterState::Unknown;

      if (row < static_cast<int>(guesses.size())) {
        letter = guesses[row][col];
        state = guessStates[row][col];
      } else if (currentRow) {
        letter = currentGuess[col];
      }

      const bool fillCorrect = state == LetterState::Correct;
      renderer.fillRect(rect.x, rect.y, rect.width, rect.height, fillCorrect);
      renderer.drawRect(rect.x, rect.y, rect.width, rect.height, !fillCorrect);

      if (state == LetterState::Present) {
        renderer.drawRect(rect.x + 4, rect.y + 4, rect.width - 8, rect.height - 8, true);
      } else if (state == LetterState::Absent) {
        renderer.drawLine(rect.x + 10, rect.y + rect.height - 12, rect.x + rect.width - 10, rect.y + 12, true);
      }

      if (activeCell) {
        renderer.drawRect(rect.x - 3, rect.y - 3, rect.width + 6, rect.height + 6, true);
      }

      char text[2] = {letter == ' ' ? '_' : letter, '\0'};
      const int fontId = NOTOSANS_18_FONT_ID;
      const int textX = rect.x + (rect.width - renderer.getTextWidth(fontId, text, EpdFontFamily::BOLD)) / 2;
      const int textY = rect.y + (rect.height - renderer.getLineHeight(fontId)) / 2;
      renderer.drawText(fontId, textX, textY, text, !fillCorrect, EpdFontFamily::BOLD);
    }
  }
}

void PoodleActivity::drawLegend() const {
  const int top = 576;
  const int swatchSize = 24;
  const int gap = 18;
  const int groupWidth = 116;
  const int startX = (renderer.getScreenWidth() - groupWidth * 3 - gap * 2) / 2;

  const auto drawLegendItem = [&](const int x, const char* label, const LetterState state) {
    renderer.fillRect(x, top, swatchSize, swatchSize, state == LetterState::Correct);
    renderer.drawRect(x, top, swatchSize, swatchSize, state != LetterState::Correct ? true : false);
    if (state == LetterState::Present) {
      renderer.drawRect(x + 4, top + 4, swatchSize - 8, swatchSize - 8, true);
    }
    if (state == LetterState::Absent) {
      renderer.drawLine(x + 6, top + swatchSize - 6, x + swatchSize - 6, top + 6, true);
    }
    renderer.drawText(UI_10_FONT_ID, x + swatchSize + 10, top + 6, label);
  };

  drawLegendItem(startX, "Hit", LetterState::Correct);
  drawLegendItem(startX + groupWidth + gap, "Present", LetterState::Present);
  drawLegendItem(startX + (groupWidth + gap) * 2, "Miss", LetterState::Absent);
}

void PoodleActivity::drawKeyboard() {
  for (int row = 0; row < kKeyboardRowCount; row++) {
    const int rowLength = static_cast<int>(strlen(kKeyboardRows[row]));
    for (int col = 0; col < rowLength; col++) {
      const auto rect = keyboardKeyRect(renderer, row, col);
      const char letter = kKeyboardRows[row][col];
      const LetterState state = keyboardStates[letter - 'A'];
      const bool fillCorrect = state == LetterState::Correct;

      renderer.fillRect(rect.x, rect.y, rect.width, rect.height, fillCorrect);
      renderer.drawRect(rect.x, rect.y, rect.width, rect.height, !fillCorrect);

      if (state == LetterState::Present) {
        renderer.drawRect(rect.x + 3, rect.y + 3, rect.width - 6, rect.height - 6, true);
      } else if (state == LetterState::Absent) {
        renderer.drawLine(rect.x + 8, rect.y + rect.height - 8, rect.x + rect.width - 8, rect.y + 8, true);
        renderer.drawLine(rect.x + 8, rect.y + 8, rect.x + rect.width - 8, rect.y + rect.height - 8, true);
      }

      char text[2] = {letter, '\0'};
      const int fontId = NOTOSANS_14_FONT_ID;
      const int textX = rect.x + (rect.width - renderer.getTextWidth(fontId, text, EpdFontFamily::BOLD)) / 2;
      const int textY = rect.y + (rect.height - renderer.getLineHeight(fontId)) / 2;
      renderer.drawText(fontId, textX, textY, text, !fillCorrect, EpdFontFamily::BOLD);
    }
  }
}

void PoodleActivity::drawActionButtons() const {
  if (gameOver) {
    PaperS3Ui::drawSideActionButton(renderer, false, "Back");
    PaperS3Ui::drawPrimaryActionButton(renderer, "New Game");
    PaperS3Ui::drawSideActionButton(renderer, true, "Reveal");
    if (!won) {
      renderer.drawCenteredText(UI_12_FONT_ID, 842, ("Answer: " + targetWord).c_str(), true, EpdFontFamily::BOLD);
    }
    return;
  }

  PaperS3Ui::drawSideActionButton(renderer, false, "Clear");
  PaperS3Ui::drawPrimaryActionButton(renderer, "Enter");
  PaperS3Ui::drawSideActionButton(renderer, true, "Delete");
}
