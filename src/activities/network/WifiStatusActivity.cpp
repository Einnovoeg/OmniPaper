#include "WifiStatusActivity.h"

#include <WiFi.h>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "fontIds.h"

WifiStatusActivity::WifiStatusActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                       const std::function<void()>& onExit)
    : ActivityWithSubactivity("WifiStatus", renderer, mappedInput), onExit(onExit) {}

void WifiStatusActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
  needsRender = true;
}

void WifiStatusActivity::loop() {
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

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (WiFi.status() != WL_CONNECTED) {
      launchWifiSelection();
      return;
    }
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void WifiStatusActivity::launchWifiSelection() {
  enterNewActivity(new WifiSelectionActivity(renderer, mappedInput,
                                             [this](const bool connected) {
                                               exitActivity();
                                               (void)connected;
                                               needsRender = true;
                                             }));
}

void WifiStatusActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  renderer.drawCenteredText(UI_12_FONT_ID, 16, "WiFi Status");

  if (WiFi.status() != WL_CONNECTED) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "Not connected");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                              "Confirm: Connect   Back: Menu");
    renderer.displayBuffer();
    return;
  }

  const String ssid = WiFi.SSID();
  const String ip = WiFi.localIP().toString();
  const String gw = WiFi.gatewayIP().toString();
  const String dns = WiFi.dnsIP().toString();
  const String mac = WiFi.macAddress();
  const int rssi = WiFi.RSSI();

  int y = 60;
  renderer.drawText(UI_10_FONT_ID, 40, y, ("SSID: " + ssid).c_str());
  y += 20;
  renderer.drawText(UI_10_FONT_ID, 40, y, ("IP: " + ip).c_str());
  y += 20;
  renderer.drawText(UI_10_FONT_ID, 40, y, ("Gateway: " + gw).c_str());
  y += 20;
  renderer.drawText(UI_10_FONT_ID, 40, y, ("DNS: " + dns).c_str());
  y += 20;
  renderer.drawText(UI_10_FONT_ID, 40, y, ("MAC: " + mac).c_str());
  y += 20;
  char rssiLine[32];
  snprintf(rssiLine, sizeof(rssiLine), "RSSI: %d dBm", rssi);
  renderer.drawText(UI_10_FONT_ID, 40, y, rssiLine);

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Back: Menu");
  renderer.displayBuffer();
}
