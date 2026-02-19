#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"

class PoodleActivity final : public Activity {
 public:
  PoodleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  static constexpr int kWordLength = 5;
  static constexpr int kMaxAttempts = 6;

  std::function<void()> onExit;
  std::vector<std::string> wordList;
  std::string targetWord;
  std::vector<std::string> guesses;
  std::string currentGuess;
  int cursorPos = 0;
  bool gameOver = false;
  bool won = false;
  bool needsRender = true;

  void loadWords();
  void pickTarget();
  void handleInput();
  void submitGuess();
  void render();
  void drawGrid();
  void drawKeyboardHint();

  char nextLetter(char c, int delta) const;
};
