#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"

class CalculatorActivity final : public Activity {
 public:
  CalculatorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  struct Key {
    const char* label;
    enum class Type { Digit, Dot, Op, Eval, Clear, Del, Empty } type;
    char value;
  };

  std::function<void()> onExit;
  std::string input;
  std::string result;
  int selRow = 0;
  int selCol = 0;
  bool needsRender = true;

  void handleInput();
  void render();
  void pressKey(const Key& key);
  double evalExpression(const std::string& expr, bool& ok) const;

  static const Key kKeys[5][4];
};
