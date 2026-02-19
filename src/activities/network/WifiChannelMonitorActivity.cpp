#include "WifiChannelMonitorActivity.h"

#include <WiFi.h>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"

WifiChannelMonitorActivity::WifiChannelMonitorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                       const std::function<void()>& onExit)
    : Activity("WifiChannelMonitor", renderer, mappedInput), onExitCb(onExit) {}

void WifiChannelMonitorActivity::onEnter() {
  Activity::onEnter();
  counts.fill(0);
  startScan();
  needsRender = true;
}

void WifiChannelMonitorActivity::onExit() {
  Activity::onExit();
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
}

void WifiChannelMonitorActivity::startScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
  scanning = true;
}

void WifiChannelMonitorActivity::collectResults() {
  const int16_t count = WiFi.scanComplete();
  if (count < 0) {
    return;
  }
  counts.fill(0);
  for (int i = 0; i < count; i++) {
    const int ch = WiFi.channel(i);
    if (ch >= 1 && ch < static_cast<int>(counts.size())) {
      counts[ch] += 1;
    }
  }
  WiFi.scanDelete();
  scanning = false;
  needsRender = true;
}

void WifiChannelMonitorActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExitCb) {
      onExitCb();
    }
    return;
  }

  if (scanning) {
    collectResults();
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    startScan();
    needsRender = true;
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void WifiChannelMonitorActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  renderer.drawCenteredText(UI_12_FONT_ID, 16, "Channel Monitor");

  if (scanning) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "Scanning...");
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Back: Menu");
    renderer.displayBuffer();
    return;
  }

  int maxCount = 1;
  for (int ch = 1; ch <= 11; ch++) {
    if (counts[ch] > maxCount) maxCount = counts[ch];
  }

  const int chartTop = 70;
  const int chartHeight = 220;
  const int chartWidth = renderer.getScreenWidth() - 80;
  const int barWidth = chartWidth / 11;
  const int startX = 40;

  for (int ch = 1; ch <= 11; ch++) {
    const int barHeight = (counts[ch] * chartHeight) / maxCount;
    const int x = startX + (ch - 1) * barWidth;
    const int y = chartTop + (chartHeight - barHeight);
    renderer.fillRect(x + 2, y, barWidth - 4, barHeight, true);

    char label[4];
    snprintf(label, sizeof(label), "%d", ch);
    renderer.drawText(UI_10_FONT_ID, x + 4, chartTop + chartHeight + 6, label);
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Confirm: Rescan   Back: Menu");
  renderer.displayBuffer();
}
