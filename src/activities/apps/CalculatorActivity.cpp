#include "CalculatorActivity.h"

#include <cmath>
#include <cstdlib>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"

const CalculatorActivity::Key CalculatorActivity::kKeys[5][4] = {
    { {"7", Key::Type::Digit, '7'}, {"8", Key::Type::Digit, '8'}, {"9", Key::Type::Digit, '9'}, {"/", Key::Type::Op, '/'} },
    { {"4", Key::Type::Digit, '4'}, {"5", Key::Type::Digit, '5'}, {"6", Key::Type::Digit, '6'}, {"*", Key::Type::Op, '*'} },
    { {"1", Key::Type::Digit, '1'}, {"2", Key::Type::Digit, '2'}, {"3", Key::Type::Digit, '3'}, {"-", Key::Type::Op, '-'} },
    { {"0", Key::Type::Digit, '0'}, {".", Key::Type::Dot, '.'}, {"=", Key::Type::Eval, '='}, {"+", Key::Type::Op, '+'} },
    { {"C", Key::Type::Clear, 0}, {"DEL", Key::Type::Del, 0}, {"", Key::Type::Empty, 0}, {"", Key::Type::Empty, 0} },
};

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
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4g", val);
        result = buf;
      } else {
        result = "ERR";
      }
      break;
    }
    case Key::Type::Empty:
      break;
  }
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
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "Calculator");

  int y = 50;
  renderer.drawText(UI_12_FONT_ID, 40, y, input.c_str());
  y += 30;
  if (!result.empty()) {
    std::string out = "= " + result;
    renderer.drawText(UI_12_FONT_ID, 40, y, out.c_str());
  }

  const int startX = 120;
  const int startY = 130;
  const int keyW = 120;
  const int keyH = 60;
  const int gap = 10;

  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 4; c++) {
      const Key& key = kKeys[r][c];
      if (key.type == Key::Type::Empty) continue;
      int x = startX + c * (keyW + gap);
      int yKey = startY + r * (keyH + gap);
      bool selected = (r == selRow && c == selCol);
      if (selected) {
        renderer.fillRect(x, yKey, keyW, keyH, true);
      }
      renderer.drawRect(x, yKey, keyW, keyH, true);
      int textX = x + (keyW - renderer.getTextWidth(UI_12_FONT_ID, key.label)) / 2;
      int textY = yKey + 18;
      renderer.drawText(UI_12_FONT_ID, textX, textY, key.label, !selected);
    }
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Confirm: Select   Back: Menu");
  renderer.displayBuffer();
}
