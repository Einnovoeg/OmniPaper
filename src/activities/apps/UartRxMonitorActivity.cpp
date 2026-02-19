#include "UartRxMonitorActivity.h"

// Source reference:
// - Diagnostics source project: https://github.com/geo-tp/ESP32-Bus-Pirate
// - Local implementation for OmniPaper is maintained in this file.

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"

namespace {
constexpr int kMaxLines = 60;
constexpr int kMaxLineLen = 96;
}

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
