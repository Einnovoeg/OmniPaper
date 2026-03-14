#include "WifiStatusActivity.h"

#include <WiFi.h>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "activities/apps/PaperS3Ui.h"
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

#if defined(PLATFORM_M5PAPERS3)
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasTapped() && PaperS3Ui::rawTouchToPortrait(mappedInput.getTouchX(), mappedInput.getTouchY(), tapX, tapY)) {
    if (PaperS3Ui::backButtonRect(renderer).contains(tapX, tapY)) {
      if (onExit) {
        onExit();
      }
      return;
    }

    if (WiFi.status() != WL_CONNECTED && PaperS3Ui::primaryActionRect(renderer).contains(tapX, tapY)) {
      launchWifiSelection();
      return;
    }
  }
#endif

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
#if defined(PLATFORM_M5PAPERS3)
  {
    renderer.setOrientation(GfxRenderer::Orientation::Portrait);
    PaperS3Ui::drawScreenHeader(renderer, "WiFi Status",
                                WiFi.status() == WL_CONNECTED ? "Connected" : "Not connected");
    PaperS3Ui::drawBackButton(renderer);

    if (WiFi.status() != WL_CONNECTED) {
      renderer.drawCenteredText(UI_12_FONT_ID, 320, "No active WiFi connection");
      renderer.drawCenteredText(UI_10_FONT_ID, 352, "Tap Connect to choose a network");
      PaperS3Ui::drawPrimaryActionButton(renderer, "Connect");
      PaperS3Ui::drawFooter(renderer, "Touchscreen navigation only");
      renderer.displayBuffer();
      return;
    }

    const String ssid = WiFi.SSID();
    const String ip = WiFi.localIP().toString();
    const String gw = WiFi.gatewayIP().toString();
    const String dns = WiFi.dnsIP().toString();
    const String mac = WiFi.macAddress();
    const int rssi = WiFi.RSSI();

    const char* titles[] = {"SSID", "IP Address", "Gateway", "DNS", "MAC", "Signal"};
    std::string values[] = {ssid.c_str(), ip.c_str(), gw.c_str(), dns.c_str(), mac.c_str(), ""};
    char rssiLine[32];
    snprintf(rssiLine, sizeof(rssiLine), "%d dBm", rssi);
    values[5] = rssiLine;

    for (int i = 0; i < 6; i++) {
      const auto rect = PaperS3Ui::listRowRect(renderer, i);
      PaperS3Ui::drawListRow(renderer, rect, false, titles[i], values[i].c_str());
    }

    PaperS3Ui::drawFooter(renderer, "Tap Back to return");
    renderer.displayBuffer();
    return;
  }
#endif

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
