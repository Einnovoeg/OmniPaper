#include "WifiTestsActivity.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include <cstdlib>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"

namespace {
constexpr int kItemCount = 5;

std::string normalizeUrl(const std::string& host) {
  if (host.rfind("http://", 0) == 0 || host.rfind("https://", 0) == 0) {
    return host;
  }
  return "http://" + host;
}

std::string stripSchemeAndPath(const std::string& input) {
  std::string out = input;
  const auto schemePos = out.find("://");
  if (schemePos != std::string::npos) {
    out = out.substr(schemePos + 3);
  }
  const auto slashPos = out.find('/');
  if (slashPos != std::string::npos) {
    out = out.substr(0, slashPos);
  }
  return out;
}

void parseHostAndPort(const std::string& input, std::string& hostOut, uint16_t& portOut, const uint16_t defaultPort) {
  hostOut = stripSchemeAndPath(input);
  portOut = defaultPort;
  const auto colonPos = hostOut.find(':');
  if (colonPos != std::string::npos) {
    const std::string portStr = hostOut.substr(colonPos + 1);
    hostOut = hostOut.substr(0, colonPos);
    const int port = atoi(portStr.c_str());
    if (port > 0 && port < 65536) {
      portOut = static_cast<uint16_t>(port);
    }
  }
}
}  // namespace

WifiTestsActivity::WifiTestsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                     const std::function<void()>& onExit)
    : ActivityWithSubactivity("WifiTests", renderer, mappedInput), onExit(onExit) {}

void WifiTestsActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
  selectionIndex = 0;
  resultMessage.clear();
  needsRender = true;
}

void WifiTestsActivity::loop() {
  if (subActivity) {
    ActivityWithSubactivity::loop();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExit) {
      onExit();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selectionIndex = (selectionIndex - 1 + kItemCount) % kItemCount;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selectionIndex = (selectionIndex + 1) % kItemCount;
    needsRender = true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (selectionIndex == 0) {
      ensureWifiThenRun(PendingAction::Http);
    } else if (selectionIndex == 1) {
      ensureWifiThenRun(PendingAction::Dns);
    } else if (selectionIndex == 2) {
      ensureWifiThenRun(PendingAction::Tcp);
    } else if (selectionIndex == 3) {
      launchHostEntry();
    } else {
      if (onExit) {
        onExit();
      }
    }
    return;
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void WifiTestsActivity::ensureWifiThenRun(const PendingAction action) {
  pending = action;
  if (WiFi.status() == WL_CONNECTED) {
    if (action == PendingAction::Http) {
      runHttpTest();
    } else if (action == PendingAction::Dns) {
      runDnsTest();
    } else if (action == PendingAction::Tcp) {
      runTcpTest();
    }
    return;
  }
  launchWifiSelection();
}

void WifiTestsActivity::launchWifiSelection() {
  enterNewActivity(new WifiSelectionActivity(renderer, mappedInput,
                                             [this](const bool connected) {
                                               exitActivity();
                                               if (!connected) {
                                                 resultMessage = "WiFi connection failed";
                                                 needsRender = true;
                                                 return;
                                               }
                                               if (pending == PendingAction::Http) {
                                                 runHttpTest();
                                               } else if (pending == PendingAction::Dns) {
                                                 runDnsTest();
                                               } else if (pending == PendingAction::Tcp) {
                                                 runTcpTest();
                                               }
                                             }));
}

void WifiTestsActivity::launchHostEntry() {
  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, "Test Host", host, 12, 0, false,
      [this](const std::string& value) {
        host = value;
        exitActivity();
        needsRender = true;
      },
      [this]() {
        exitActivity();
        needsRender = true;
      }));
}

void WifiTestsActivity::runDnsTest() {
  resultMessage = "Running DNS test...";
  needsRender = true;

  IPAddress ip;
  std::string hostOnly;
  uint16_t port = 0;
  parseHostAndPort(host, hostOnly, port, 0);
  const uint32_t start = millis();
  const bool ok = WiFi.hostByName(hostOnly.c_str(), ip);
  const uint32_t elapsed = millis() - start;

  char line[64];
  if (ok) {
    snprintf(line, sizeof(line), "DNS OK %s (%lu ms)", ip.toString().c_str(), static_cast<unsigned long>(elapsed));
  } else {
    snprintf(line, sizeof(line), "DNS failed (%lu ms)", static_cast<unsigned long>(elapsed));
  }
  resultMessage = line;
  needsRender = true;
}

void WifiTestsActivity::runTcpTest() {
  resultMessage = "Running TCP test...";
  needsRender = true;

  std::string hostOnly;
  uint16_t port = 0;
  parseHostAndPort(host, hostOnly, port, 80);
  if (hostOnly.empty()) {
    resultMessage = "Host empty";
    needsRender = true;
    return;
  }

  WiFiClient client;
  const uint32_t start = millis();
  const bool ok = client.connect(hostOnly.c_str(), port);
  const uint32_t elapsed = millis() - start;
  if (ok) {
    client.stop();
  }

  char line[64];
  snprintf(line, sizeof(line), "TCP %s:%u %s (%lu ms)", hostOnly.c_str(), port, ok ? "OK" : "fail",
           static_cast<unsigned long>(elapsed));
  resultMessage = line;
  needsRender = true;
}

void WifiTestsActivity::runHttpTest() {
  resultMessage = "Running HTTP test...";
  needsRender = true;

  const std::string url = normalizeUrl(host);
  HTTPClient http;
  const uint32_t start = millis();
  if (!http.begin(url.c_str())) {
    resultMessage = "HTTP begin failed";
    needsRender = true;
    return;
  }
  const int code = http.GET();
  const uint32_t elapsed = millis() - start;
  http.end();

  char line[64];
  if (code > 0) {
    snprintf(line, sizeof(line), "HTTP %d (%lu ms)", code, static_cast<unsigned long>(elapsed));
  } else {
    snprintf(line, sizeof(line), "HTTP error (%lu ms)", static_cast<unsigned long>(elapsed));
  }
  resultMessage = line;
  needsRender = true;
}

void WifiTestsActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  renderer.drawCenteredText(UI_12_FONT_ID, 16, "WiFi Tests");

  const char* items[kItemCount] = {"HTTP GET", "DNS Lookup", "TCP Connect", "Change Host", "Back"};
  int y = 70;
  for (int i = 0; i < kItemCount; i++) {
    if (i == selectionIndex) {
      renderer.fillRect(40, y - 6, renderer.getScreenWidth() - 80, 24, true);
      renderer.drawText(UI_12_FONT_ID, 50, y, items[i], false);
    } else {
      renderer.drawText(UI_12_FONT_ID, 50, y, items[i]);
    }
    y += 28;
  }

  if (!host.empty()) {
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 44, host.c_str());
  }
  if (!resultMessage.empty()) {
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, resultMessage.c_str());
  } else {
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                              "Confirm: Run/Select   Back: Menu");
  }
  renderer.displayBuffer();
}
