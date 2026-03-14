#include "CalculatorActivity.h"

#include <GfxRenderer.h>

#include <cmath>
#include <cstdlib>

#include "MappedInputManager.h"
#include "PaperS3Ui.h"
#include "fontIds.h"

const CalculatorActivity::Key CalculatorActivity::kScientificKeys[2][4] = {
    {{"sin", Key::Type::FuncSin, 0},
     {"cos", Key::Type::FuncCos, 0},
     {"tan", Key::Type::FuncTan, 0},
     {"sqrt", Key::Type::FuncSqrt, 0}},
    {{"x^2", Key::Type::FuncSquare, 0},
     {"1/x", Key::Type::FuncReciprocal, 0},
     {"log", Key::Type::FuncLog10, 0},
     {"ln", Key::Type::FuncLn, 0}},
};

const CalculatorActivity::Key CalculatorActivity::kKeys[5][4] = {
    {{"7", Key::Type::Digit, '7'},
     {"8", Key::Type::Digit, '8'},
     {"9", Key::Type::Digit, '9'},
     {"/", Key::Type::Op, '/'}},
    {{"4", Key::Type::Digit, '4'},
     {"5", Key::Type::Digit, '5'},
     {"6", Key::Type::Digit, '6'},
     {"*", Key::Type::Op, '*'}},
    {{"1", Key::Type::Digit, '1'},
     {"2", Key::Type::Digit, '2'},
     {"3", Key::Type::Digit, '3'},
     {"-", Key::Type::Op, '-'}},
    {{"0", Key::Type::Digit, '0'}, {".", Key::Type::Dot, '.'}, {"=", Key::Type::Eval, '='}, {"+", Key::Type::Op, '+'}},
    {{"C", Key::Type::Clear, 0}, {"DEL", Key::Type::Del, 0}, {"", Key::Type::Empty, 0}, {"", Key::Type::Empty, 0}},
};

namespace {
PaperS3Ui::Rect scientificKeyRect(const int row, const int col) {
  PaperS3Ui::Rect rect;
  constexpr int startX = 24;
  constexpr int startY = 214;
  constexpr int keyWidth = 114;
  constexpr int keyHeight = 54;
  constexpr int gap = 12;
  rect.x = startX + col * (keyWidth + gap);
  rect.y = startY + row * (keyHeight + gap);
  rect.width = keyWidth;
  rect.height = keyHeight;
  return rect;
}

PaperS3Ui::Rect calculatorKeyRect(const int row, const int col) {
  PaperS3Ui::Rect rect;
  constexpr int startX = 24;
  constexpr int startY = 350;
  constexpr int keyWidth = 114;
  constexpr int keyHeight = 72;
  constexpr int gap = 12;
  rect.x = startX + col * (keyWidth + gap);
  rect.y = startY + row * (keyHeight + gap);
  rect.width = keyWidth;
  rect.height = keyHeight;
  return rect;
}
}  // namespace

CalculatorActivity::CalculatorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                       const std::function<void()>& onExit)
    : Activity("Calculator", renderer, mappedInput), onExit(onExit) {}

void CalculatorActivity::onEnter() {
  Activity::onEnter();
  input.clear();
  result.clear();
  selRow = 0;
  selCol = 0;
  needsRender = true;
}

void CalculatorActivity::loop() {
  handleInput();
  if (needsRender) {
    render();
    needsRender = false;
  }
}

void CalculatorActivity::handleInput() {
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

    for (int row = 0; row < 2; row++) {
      for (int col = 0; col < 4; col++) {
        if (!scientificKeyRect(row, col).contains(tapX, tapY)) {
          continue;
        }
        pressKey(kScientificKeys[row][col]);
        needsRender = true;
        return;
      }
    }

    for (int row = 0; row < 5; row++) {
      for (int col = 0; col < 4; col++) {
        if (!calculatorKeyRect(row, col).contains(tapX, tapY)) {
          continue;
        }
        selRow = row;
        selCol = col;
        const Key& key = kKeys[row][col];
        if (key.type != Key::Type::Empty) {
          pressKey(key);
        }
        needsRender = true;
        return;
      }
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
    selCol = (selCol + 3) % 4;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    selCol = (selCol + 1) % 4;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selRow = (selRow + 4) % 5;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selRow = (selRow + 1) % 5;
    needsRender = true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    const Key& key = kKeys[selRow][selCol];
    if (key.type != Key::Type::Empty) {
      pressKey(key);
      needsRender = true;
    }
  }
}

void CalculatorActivity::pressKey(const Key& key) {
  switch (key.type) {
    case Key::Type::Digit:
      input.push_back(key.value);
      break;
    case Key::Type::Dot:
      input.push_back('.');
      break;
    case Key::Type::Op: {
      if (!input.empty()) {
        char last = input.back();
        if (last == '+' || last == '-' || last == '*' || last == '/') {
          input.back() = key.value;
        } else {
          input.push_back(key.value);
        }
      }
      break;
    }
    case Key::Type::Clear:
      input.clear();
      result.clear();
      break;
    case Key::Type::Del:
      if (!input.empty()) {
        input.pop_back();
      }
      break;
    case Key::Type::Eval: {
      bool ok = false;
      double val = evalExpression(input, ok);
      if (ok) {
        result = formatValue(val);
      } else {
        result = "ERR";
      }
      break;
    }
    case Key::Type::FuncSin:
    case Key::Type::FuncCos:
    case Key::Type::FuncTan:
    case Key::Type::FuncSqrt:
    case Key::Type::FuncSquare:
    case Key::Type::FuncReciprocal:
    case Key::Type::FuncLog10:
    case Key::Type::FuncLn:
    case Key::Type::ConstPi:
    case Key::Type::ConstE:
      applyScientificKey(key);
      break;
    case Key::Type::Empty:
      break;
  }
}

bool CalculatorActivity::resolveWorkingValue(double& value) const {
  bool ok = false;

  if (!input.empty()) {
    value = evalExpression(input, ok);
    if (ok) {
      return true;
    }
  }

  if (!result.empty() && result != "ERR") {
    char* end = nullptr;
    value = strtod(result.c_str(), &end);
    return end != result.c_str() && end != nullptr && *end == '\0';
  }

  return false;
}

void CalculatorActivity::appendLiteral(const char* literal) {
  if (literal == nullptr || *literal == '\0') {
    return;
  }

  if (!input.empty()) {
    const char last = input.back();
    if ((last >= '0' && last <= '9') || last == '.') {
      input.push_back('*');
    }
  }

  input += literal;
}

std::string CalculatorActivity::formatValue(const double value) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.10g", value);
  return buffer;
}

void CalculatorActivity::applyScientificKey(const Key& key) {
  if (key.type == Key::Type::ConstPi) {
    appendLiteral("3.141592654");
    return;
  }
  if (key.type == Key::Type::ConstE) {
    appendLiteral("2.718281828");
    return;
  }

  double value = 0.0;
  if (!resolveWorkingValue(value)) {
    result = "ERR";
    return;
  }

  double output = value;
  bool ok = true;
  switch (key.type) {
    case Key::Type::FuncSin:
      output = sin(value);
      break;
    case Key::Type::FuncCos:
      output = cos(value);
      break;
    case Key::Type::FuncTan:
      output = tan(value);
      break;
    case Key::Type::FuncSqrt:
      if (value < 0.0) {
        ok = false;
      } else {
        output = sqrt(value);
      }
      break;
    case Key::Type::FuncSquare:
      output = value * value;
      break;
    case Key::Type::FuncReciprocal:
      if (value == 0.0) {
        ok = false;
      } else {
        output = 1.0 / value;
      }
      break;
    case Key::Type::FuncLog10:
      if (value <= 0.0) {
        ok = false;
      } else {
        output = log10(value);
      }
      break;
    case Key::Type::FuncLn:
      if (value <= 0.0) {
        ok = false;
      } else {
        output = log(value);
      }
      break;
    default:
      break;
  }

  if (!ok || std::isnan(output) || std::isinf(output)) {
    result = "ERR";
    return;
  }

  input = formatValue(output);
  result.clear();
}

double CalculatorActivity::evalExpression(const std::string& expr, bool& ok) const {
  ok = false;
  if (expr.empty()) {
    return 0.0;
  }

  std::vector<double> numbers;
  std::vector<char> ops;

  const char* ptr = expr.c_str();
  while (*ptr) {
    char* end = nullptr;
    double val = strtod(ptr, &end);
    if (end == ptr) {
      return 0.0;
    }
    numbers.push_back(val);
    ptr = end;
    if (*ptr == '\0') break;
    char op = *ptr;
    if (op != '+' && op != '-' && op != '*' && op != '/') {
      return 0.0;
    }
    ops.push_back(op);
    ptr++;
  }

  // Handle * and /
  for (size_t i = 0; i < ops.size();) {
    if (ops[i] == '*' || ops[i] == '/') {
      double a = numbers[i];
      double b = numbers[i + 1];
      double res = (ops[i] == '*') ? (a * b) : (b == 0.0 ? 0.0 : (a / b));
      numbers[i] = res;
      numbers.erase(numbers.begin() + i + 1);
      ops.erase(ops.begin() + i);
    } else {
      i++;
    }
  }

  double resultVal = numbers[0];
  for (size_t i = 0; i < ops.size(); i++) {
    if (ops[i] == '+') resultVal += numbers[i + 1];
    if (ops[i] == '-') resultVal -= numbers[i + 1];
  }

  ok = true;
  return resultVal;
}

void CalculatorActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  PaperS3Ui::drawScreenHeader(renderer, "Calculator");
  PaperS3Ui::drawBackButton(renderer);

  renderer.drawRect(24, 88, 492, 124);
  int y = 104;
  const std::string displayInput = input.empty() ? "0" : input;
  renderer.drawText(NOTOSANS_18_FONT_ID, 40, y, displayInput.c_str());
  y += 42;
  if (!result.empty()) {
    std::string out = "= " + result;
    renderer.drawText(UI_12_FONT_ID, 40, y, out.c_str());
  }

  for (int r = 0; r < 2; r++) {
    for (int c = 0; c < 4; c++) {
      const Key& key = kScientificKeys[r][c];
      const auto rect = scientificKeyRect(r, c);
      renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
      renderer.drawRect(rect.x, rect.y, rect.width, rect.height, true);
      const int textX = rect.x + (rect.width - renderer.getTextWidth(UI_10_FONT_ID, key.label)) / 2;
      const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
      renderer.drawText(UI_10_FONT_ID, textX, textY, key.label);
    }
  }

  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 4; c++) {
      const Key& key = kKeys[r][c];
      if (key.type == Key::Type::Empty) continue;
      const auto rect = calculatorKeyRect(r, c);
      bool selected = (r == selRow && c == selCol);
      renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
      renderer.drawRect(rect.x, rect.y, rect.width, rect.height, true);
      if (selected) {
        renderer.fillRect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2, true);
      }
      int textX = rect.x + (rect.width - renderer.getTextWidth(UI_12_FONT_ID, key.label)) / 2;
      int textY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
      renderer.drawText(UI_12_FONT_ID, textX, textY, key.label, !selected);
    }
  }

  PaperS3Ui::drawFooter(renderer, "Tap numeric or scientific keys directly");
  renderer.displayBuffer();
}
