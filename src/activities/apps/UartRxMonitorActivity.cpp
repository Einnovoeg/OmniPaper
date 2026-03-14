#include "UartRxMonitorActivity.h"

// Source reference:
// - Diagnostics source project: https://github.com/geo-tp/ESP32-Bus-Pirate
// - Local implementation for OmniPaper is maintained in this file.

#include <GfxRenderer.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "MappedInputManager.h"
#include "PaperS3Ui.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"

namespace {
constexpr int kMaxLines = 60;
constexpr int kMaxLineLen = 96;
}  // namespace

UartRxMonitorActivity::UartRxMonitorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                             const std::function<void()>& onExit)
    : ActivityWithSubactivity("UartRxMonitor", renderer, mappedInput), onExitCb(onExit), uart(2) {}

void UartRxMonitorActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
  lines.clear();
  currentLine.clear();
  statusMessage.clear();
  startListening();
  needsRender = true;
}

void UartRxMonitorActivity::onExit() {
  stopListening();
  ActivityWithSubactivity::onExit();
}

void UartRxMonitorActivity::startListening() {
  stopListening();
  uart.begin(static_cast<uint32_t>(baudRate), SERIAL_8N1, rxPin, -1);
  listening = true;
  statusMessage = "Listening";
}

void UartRxMonitorActivity::stopListening() {
  if (!listening) {
    return;
  }
  uart.end();
  listening = false;
}

void UartRxMonitorActivity::appendLine(const std::string& line) {
  if (line.empty()) {
    return;
  }
  lines.push_back(line);
  if (static_cast<int>(lines.size()) > kMaxLines) {
    lines.erase(lines.begin());
  }
  needsRender = true;
}

void UartRxMonitorActivity::pollUart() {
  if (!listening) {
    return;
  }

  int processed = 0;
  while (uart.available() > 0 && processed < 512) {
    const int raw = uart.read();
    if (raw < 0) {
      break;
    }
    const char c = static_cast<char>(raw);
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      appendLine(currentLine);
      currentLine.clear();
      processed++;
      continue;
    }

    if (std::isprint(static_cast<unsigned char>(c)) || c == '\t') {
      currentLine.push_back(c);
    } else {
      currentLine.push_back('.');
    }
    if (static_cast<int>(currentLine.size()) >= kMaxLineLen) {
      appendLine(currentLine);
      currentLine.clear();
    }
    processed++;
  }
}

void UartRxMonitorActivity::promptBaud() {
  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, "UART Baud", std::to_string(baudRate), 7, 0, false,
      [this](const std::string& value) {
        char* end = nullptr;
        const long v = strtol(value.c_str(), &end, 10);
        if (end && *end == '\0' && v >= 300 && v <= 2000000) {
          baudRate = static_cast<int>(v);
          if (listening) {
            startListening();
          }
          statusMessage = "Baud updated";
        } else {
          statusMessage = "Invalid baud";
        }
        exitActivity();
        needsRender = true;
      },
      [this]() {
        exitActivity();
        needsRender = true;
      }));
}

void UartRxMonitorActivity::promptRxPin() {
  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, "RX Pin", std::to_string(rxPin), 3, 0, false,
      [this](const std::string& value) {
        char* end = nullptr;
        const long v = strtol(value.c_str(), &end, 10);
        if (end && *end == '\0' && v >= 0 && v <= 39) {
          rxPin = static_cast<int>(v);
          if (listening) {
            startListening();
          }
          statusMessage = "RX pin updated";
        } else {
          statusMessage = "Invalid pin";
        }
        exitActivity();
        needsRender = true;
      },
      [this]() {
        exitActivity();
        needsRender = true;
      }));
}

void UartRxMonitorActivity::loop() {
  if (subActivity) {
    ActivityWithSubactivity::loop();
    return;
  }

#if defined(PLATFORM_M5PAPERS3)
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasTapped() &&
      PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), tapX, tapY)) {
    if (PaperS3Ui::backButtonRect(renderer).contains(tapX, tapY)) {
      if (onExitCb) {
        onExitCb();
      }
      return;
    }

    if (PaperS3Ui::listRowRect(renderer, 0).contains(tapX, tapY)) {
      promptBaud();
      return;
    }
    if (PaperS3Ui::listRowRect(renderer, 1).contains(tapX, tapY)) {
      promptRxPin();
      return;
    }
    if (PaperS3Ui::sideActionRect(renderer, false).contains(tapX, tapY)) {
      lines.clear();
      currentLine.clear();
      statusMessage = "Cleared";
      needsRender = true;
      return;
    }
    if (PaperS3Ui::primaryActionRect(renderer).contains(tapX, tapY)) {
      if (listening) {
        stopListening();
        statusMessage = "Paused";
      } else {
        startListening();
      }
      needsRender = true;
      return;
    }
  }
#endif

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExitCb) {
      onExitCb();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (listening) {
      stopListening();
      statusMessage = "Paused";
    } else {
      startListening();
    }
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    promptBaud();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    promptRxPin();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    lines.clear();
    currentLine.clear();
    statusMessage = "Cleared";
    needsRender = true;
  }

  pollUart();
  if (needsRender) {
    render();
    needsRender = false;
  }
}

void UartRxMonitorActivity::render() {
  renderer.clearScreen();
#if defined(PLATFORM_M5PAPERS3)
  {
    renderer.setOrientation(GfxRenderer::Orientation::Portrait);
    PaperS3Ui::drawScreenHeader(renderer, "UART RX", listening ? "Listening" : "Paused");
    PaperS3Ui::drawBackButton(renderer);

    char baudLine[32];
    snprintf(baudLine, sizeof(baudLine), "%d baud", baudRate);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 0), false, "Baud Rate", baudLine,
                           "Tap row to edit");

    char rxLine[24];
    snprintf(rxLine, sizeof(rxLine), "GPIO%d", rxPin);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 1), false, "RX Pin", rxLine, "Tap row to edit");
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 2), false, "State",
                           listening ? "Listening" : "Paused");

    const int maxRows = 4;
    const int start = std::max(0, static_cast<int>(lines.size()) - maxRows);
    for (int i = 0; i < maxRows; i++) {
      const int lineIndex = start + i;
      if (lineIndex >= static_cast<int>(lines.size())) {
        break;
      }
      PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, i + 3), false, lines[lineIndex].c_str());
    }

    PaperS3Ui::drawSideActionButton(renderer, false, "Clear");
    PaperS3Ui::drawPrimaryActionButton(renderer, listening ? "Pause" : "Listen");
    if (!statusMessage.empty()) {
      PaperS3Ui::drawFooterStatus(renderer, statusMessage.c_str());
    }
    PaperS3Ui::drawFooter(renderer, "Tap Baud or RX Pin to open the keyboard");
    renderer.displayBuffer();
    return;
  }
#endif

  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  renderer.drawCenteredText(UI_12_FONT_ID, 16, "UART RX Monitor");

  char cfg[64];
  snprintf(cfg, sizeof(cfg), "RX GPIO%d @ %d baud", rxPin, baudRate);
  renderer.drawText(SMALL_FONT_ID, 20, 40, cfg);
  renderer.drawText(SMALL_FONT_ID, 20, 56, listening ? "State: Listening" : "State: Paused");
  if (!statusMessage.empty()) {
    renderer.drawText(SMALL_FONT_ID, 20, 72, statusMessage.c_str());
  }

  const int maxRows = 10;
  const int start = std::max(0, static_cast<int>(lines.size()) - maxRows);
  int y = 96;
  for (int i = start; i < static_cast<int>(lines.size()); i++) {
    renderer.drawText(UI_10_FONT_ID, 20, y, lines[i].c_str());
    y += 18;
  }
  if (!currentLine.empty() && y < renderer.getScreenHeight() - 28) {
    renderer.drawText(UI_10_FONT_ID, 20, y, currentLine.c_str());
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                            "Confirm: Run/Pause   Left: Baud   Right: RX Pin   Up: Clear   Back: Menu");
  renderer.displayBuffer();
}
