#include "PoodleActivity.h"

// Source reference:
// - Gameplay/source project: https://github.com/k-natori/Poodle
// - Local implementation for OmniPaper is maintained in this file.

#include <Arduino.h>
#include <SDCardManager.h>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"

namespace {
const char* kWordFile = "/games/poodle_words.txt";
const char* kBuiltinWords[] = {
    "ABOUT", "ABOVE", "ACORN", "ACTOR", "ADULT", "AGENT", "AGREE", "ALARM", "ALBUM", "ALERT",
    "ALIEN", "ALIVE", "AMBER", "AMONG", "ANGLE", "APPLE", "APRIL", "ARENA", "ARGON", "ARISE",
    "ARMOR", "ARROW", "ASIDE", "ATLAS", "AUDIO", "AWARE", "AWOKE", "BADGE", "BASIC", "BASIL",
    "BATCH", "BEACH", "BEGAN", "BEGIN", "BEING", "BENCH", "BERRY", "BIRTH", "BLEND", "BLINK",
    "BLOOM", "BLUNT", "BOARD", "BOOST", "BRAIN", "BRAVE", "BREAD", "BRICK", "BRING", "BRUSH",
    "BUILD", "BUNCH", "BURST", "CABLE", "CALMS", "CANDY", "CARRY", "CHAIN", "CHAIR", "CHART",
    "CHESS", "CHIEF", "CHOIR", "CHORD", "CIVIC", "CLOUD", "COAST", "COLOR", "CORAL", "COVER",
    "CRANE", "CRISP", "CROSS", "CROWN", "DANCE", "DEALT", "DELTA", "DEPTH", "DIARY", "DRIFT",
    "EAGER", "EARTH", "EIGHT", "ELBOW", "ELDER", "EMBER", "ENJOY", "ENTER", "EPOCH", "EQUAL",
    "ERROR", "EVENT", "EXTRA", "FAITH", "FALSE", "FIELD", "FINAL", "FIVER", "FLOAT", "FOCUS",
    "FORGE", "FRAME", "FRESH", "FRONT", "FRUIT", "GHOST", "GLASS", "GLIDE", "GLOVE", "GRACE",
    "GRADE", "GRAIN", "GRAPH", "GREEN", "GROUP", "GUIDE", "HAPPY", "HEART", "HONEY", "HORSE",
    "HOUSE", "IMAGE", "INDEX", "INNER", "INPUT", "ISSUE", "JUICE", "KNIFE", "LAYER", "LEMON",
    "LIGHT", "LIMIT", "LOCAL", "MAGIC", "MAJOR", "MANGO", "MAPLE", "MARCH", "MATCH", "METAL",
    "MODEL", "MONEY", "MORAL", "MOUSE", "MUSIC", "NEVER", "NORTH", "NOVEL", "NURSE", "OCEAN",
    "OFFER", "OPERA", "ORDER", "OTHER", "PAINT", "PANEL", "PAPER", "PARTY", "PASTE", "PAUSE",
    "PEARL", "PHASE", "PHONE", "PIANO", "PLAIN", "PLANE", "PLANT", "PLATE", "POINT", "POWER",
    "PRESS", "PRINT", "PRIZE", "PROUD", "QUERY", "QUIET", "RADIO", "RAINY", "RANGE", "RATIO",
    "REACH", "READY", "REPLY", "RIDER", "RIVER", "ROBIN", "ROUTE", "ROYAL", "RUGBY", "SCALE",
    "SCENE", "SCOPE", "SCORE", "SEVEN", "SHADE", "SHARE", "SHIFT", "SHINE", "SHORE", "SHORT",
    "SIGHT", "SILKY", "SIXTH", "SKILL", "SLEEP", "SMILE", "SOLAR", "SOLID", "SOUND", "SOUTH",
    "SPEED", "SPICE", "SPOKE", "STACK", "STONE", "STORY", "STUDY", "STYLE", "SUGAR", "SUNNY",
    "SWIFT", "TABLE", "TEACH", "THANK", "THEME", "THREE", "TITLE", "TODAY", "TOKEN", "TOUCH",
    "TRACK", "TRADE", "TRAIN", "TRAIL", "TRUST", "TRUTH", "UNITY", "URBAN", "VIVID", "VOICE",
    "WATER", "WHEEL", "WHITE", "WHOLE", "WINDY", "WOMAN", "WORLD", "YOUNG"};
}

PoodleActivity::PoodleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                               const std::function<void()>& onExit)
    : Activity("Poodle", renderer, mappedInput), onExit(onExit) {}

void PoodleActivity::onEnter() {
  Activity::onEnter();
  guesses.clear();
  currentGuess.assign(kWordLength, ' ');
  cursorPos = 0;
  gameOver = false;
  won = false;
  loadWords();
  pickTarget();
  needsRender = true;
}

void PoodleActivity::loop() {
  handleInput();
  if (needsRender) {
    render();
    needsRender = false;
  }
}

void PoodleActivity::loadWords() {
  wordList.clear();

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
        if (line.length() == kWordLength) {
          line.toUpperCase();
          wordList.push_back(std::string(line.c_str()));
        }
        start = end + 1;
      }
    }
  }

  if (wordList.empty()) {
    for (const auto& word : kBuiltinWords) {
      wordList.emplace_back(word);
    }
  }
}

void PoodleActivity::pickTarget() {
  if (wordList.empty()) {
    targetWord = "POODL";
    return;
  }
  const int index = static_cast<int>(millis() % wordList.size());
  targetWord = wordList[index];
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
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    bool hasLetters = false;
    for (char c : currentGuess) {
      if (c != ' ') {
        hasLetters = true;
        break;
      }
    }
    if (hasLetters) {
      currentGuess[cursorPos] = ' ';
    } else if (onExit) {
      onExit();
    }
    needsRender = true;
    return;
  }

  if (gameOver) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) && onExit) {
      onExit();
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
    bool complete = true;
    for (char c : currentGuess) {
      if (c == ' ') {
        complete = false;
        break;
      }
    }
    if (complete) {
      submitGuess();
    }
    needsRender = true;
  }
}

void PoodleActivity::submitGuess() {
  guesses.push_back(currentGuess);

  if (currentGuess == targetWord) {
    won = true;
    gameOver = true;
  } else if (static_cast<int>(guesses.size()) >= kMaxAttempts) {
    gameOver = true;
  }

  currentGuess.assign(kWordLength, ' ');
  cursorPos = 0;
}

void PoodleActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "Poodle (Word Guess)");
  drawGrid();

  if (gameOver) {
    const char* msg = won ? "You Win!" : "No More Guesses";
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() - 60, msg);
    if (!won) {
      std::string answer = "Answer: " + targetWord;
      renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 36, answer.c_str());
    }
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 16, "Confirm: Back to Menu");
  } else {
    drawKeyboardHint();
  }

  renderer.displayBuffer();
}

void PoodleActivity::drawGrid() {
  const int screenW = renderer.getScreenWidth();
  const int cellSize = 60;
  const int gap = 10;
  const int totalWidth = kWordLength * cellSize + (kWordLength - 1) * gap;
  const int startX = (screenW - totalWidth) / 2;
  int startY = 60;

  for (int row = 0; row < kMaxAttempts; row++) {
    for (int col = 0; col < kWordLength; col++) {
      const int x = startX + col * (cellSize + gap);
      const int y = startY + row * (cellSize + gap);
      renderer.drawRect(x, y, cellSize, cellSize, true);

      char letter = ' ';
      bool isCurrentRow = (row == static_cast<int>(guesses.size()));
      if (row < static_cast<int>(guesses.size())) {
        letter = guesses[row][col];
      } else if (isCurrentRow) {
        letter = currentGuess[col];
      }

      if (letter != ' ') {
        char text[2] = {letter, '\0'};
        int textX = x + (cellSize - renderer.getTextWidth(UI_12_FONT_ID, text)) / 2;
        int textY = y + 18;
        renderer.drawText(UI_12_FONT_ID, textX, textY, text);
      }

      if (isCurrentRow && col == cursorPos && !gameOver) {
        renderer.drawRect(x - 2, y - 2, cellSize + 4, cellSize + 4, true);
      }

      if (row < static_cast<int>(guesses.size())) {
        char guessLetter = guesses[row][col];
        bool correct = guessLetter == targetWord[col];
        bool present = !correct && targetWord.find(guessLetter) != std::string::npos;
        if (correct) {
          renderer.fillRect(x + cellSize / 2 - 6, y + cellSize - 12, 12, 8, true);
        } else if (present) {
          renderer.drawRect(x + cellSize / 2 - 6, y + cellSize - 12, 12, 8, true);
        }
      }
    }
  }
}

void PoodleActivity::drawKeyboardHint() {
  int y = renderer.getScreenHeight() - 60;
  renderer.drawCenteredText(SMALL_FONT_ID, y, "Left/Right: Move  Up/Down: Letter  Confirm: Submit  Back: Delete/Exit");
}
