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
    enum class Type {
      Digit,
      Dot,
      Op,
      Eval,
      Clear,
      Del,
      Empty,
      FuncSin,
      FuncCos,
      FuncTan,
      FuncSqrt,
      FuncSquare,
      FuncReciprocal,
      FuncLog10,
      FuncLn,
      ConstPi,
      ConstE
    } type;
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
  void applyScientificKey(const Key& key);
  bool resolveWorkingValue(double& value) const;
  void appendLiteral(const char* literal);
  static std::string formatValue(double value);
  double evalExpression(const std::string& expr, bool& ok) const;

  static const Key kKeys[5][4];
  static const Key kScientificKeys[2][4];
};
