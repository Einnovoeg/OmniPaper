#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "../ActivityWithSubactivity.h"

class PoodleActivity final : public ActivityWithSubactivity {
 public:
  enum class LetterState : uint8_t { Unknown = 0, Absent = 1, Present = 2, Correct = 3 };

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
  std::vector<std::array<LetterState, kWordLength>> guessStates;
  std::array<LetterState, 26> keyboardStates{};
  std::string currentGuess;
  std::string statusMessage;
  int cursorPos = 0;
  bool gameOver = false;
  bool won = false;
  bool usingSdWordList = false;
  bool statusError = false;
  bool needsRender = true;

  void loadWords();
  void pickTarget();
  void handleInput();
  void startNewGame();
  void setStatus(const std::string& message, bool isError);
  void clearGuess();
  void deleteLetter();
  void insertLetter(char letter);
  void submitGuess();
  void render();
  void drawGrid();
  void drawKeyboard();
  void drawLegend() const;
  void drawActionButtons() const;
  void launchKeyboardEntry();
  bool isGuessComplete() const;
  bool isValidGuess(const std::string& guess) const;
  std::array<LetterState, kWordLength> evaluateGuess(const std::string& guess);
  void promoteKeyboardState(char letter, LetterState state);

  char nextLetter(char c, int delta) const;
};
