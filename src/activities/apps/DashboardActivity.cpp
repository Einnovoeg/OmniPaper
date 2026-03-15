#include "DashboardActivity.h"

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <string>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "PaperS3Ui.h"
#include "fontIds.h"
#include "util/TimeUtils.h"

#ifdef PLATFORM_M5PAPER
#include <M5Unified.h>
#endif

namespace {
#if defined(PLATFORM_M5PAPERS3)
struct PaperS3DashboardState {
  int16_t batteryMv = 0;
  int16_t vbusMv = 0;
  bool usbCablePresent = false;
  bool charging = false;
  bool rtcReady = false;
  bool usbSerialOpen = false;
};

PaperS3DashboardState readPaperS3DashboardState() {
  PaperS3DashboardState state;
  // Keep the launcher/dashboard path on the lowest-risk PMIC/RTC queries. The
  // more aggressive touch/IMU/current probes are useful for diagnostics, but
  // they are not worth destabilizing the first status screen the user sees.
  state.batteryMv = M5.Power.getBatteryVoltage();
  state.vbusMv = M5.Power.getVBUSVoltage();
  state.usbCablePresent = PaperS3Ui::usbCablePresent(state.vbusMv);
  state.charging = (M5.Power.isCharging() == m5::Power_Class::is_charging);
  state.rtcReady = M5.Rtc.isEnabled();
  state.usbSerialOpen = static_cast<bool>(Serial);
  return state;
}

std::string formatUtcOffset(const int16_t minutes) {
  const int sign = (minutes < 0) ? -1 : 1;
  const int absoluteMinutes = minutes * sign;
  const int hours = absoluteMinutes / 60;
  const int mins = absoluteMinutes % 60;

  char buffer[24];
  snprintf(buffer, sizeof(buffer), "UTC%c%02d:%02d", sign < 0 ? '-' : '+', hours, mins);
  return std::string(buffer);
}
#endif
}  // namespace

DashboardActivity::DashboardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, HalGPIO& gpio,
                                     const std::function<void()>& onExit)
    : Activity("Dashboard", renderer, mappedInput), gpio(gpio), onExit(onExit) {}

void DashboardActivity::onEnter() {
  Activity::onEnter();
  needsRender = true;
  snapshot.valid = false;
  fetching = false;
}

void DashboardActivity::loop() {
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
    if (PaperS3Ui::primaryActionRect(renderer).contains(tapX, tapY)) {
      updateWeather();
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
    updateWeather();
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void DashboardActivity::updateWeather() {
  fetching = true;
  needsRender = true;

  // Paint a busy state before the synchronous network requests start so the
  // PaperS3 doesn't look frozen while TLS and JSON parsing are active.
  render();
  needsRender = false;
  delay(1);

  if (WiFi.status() != WL_CONNECTED) {
    snapshot.valid = false;
    snapshot.location = "WiFi disconnected";
    fetching = false;
    needsRender = true;
    return;
  }

  LocationInfo loc;
  if (!fetchLocation(loc)) {
    snapshot.valid = false;
    snapshot.location = "Location lookup failed";
    fetching = false;
    needsRender = true;
    return;
  }

  if (!fetchWeather(loc, snapshot)) {
    snapshot.valid = false;
    snapshot.location = loc.city;
    fetching = false;
    needsRender = true;
    return;
  }

  snapshot.location = loc.city;
  snapshot.valid = true;
  fetching = false;
  needsRender = true;
}

bool DashboardActivity::fetchLocation(LocationInfo& out) {
  HTTPClient http;
  if (!http.begin("http://ip-api.com/json")) {
    return false;
  }
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  JsonDocument doc;
  const auto err = deserializeJson(doc, payload);
  if (err) {
    return false;
  }

  const char* status = doc["status"] | "";
  if (strcmp(status, "success") != 0) {
    return false;
  }

  out.latitude = doc["lat"] | 0.0;
  out.longitude = doc["lon"] | 0.0;
  out.city = std::string(doc["city"] | "Unknown");
  out.valid = true;
  return true;
}

bool DashboardActivity::fetchWeather(const LocationInfo& loc, WeatherSnapshot& out) {
  WiFiClientSecure client;
  client.setInsecure();

  char url[256];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current_weather=true&timezone=auto",
           loc.latitude, loc.longitude);

  HTTPClient http;
  if (!http.begin(client, url)) {
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  JsonDocument doc;
  const auto err = deserializeJson(doc, payload);
  if (err) {
    return false;
  }

  JsonObject current = doc["current_weather"];
  if (current.isNull()) {
    return false;
  }

  out.temperatureC = current["temperature"] | 0.0f;
  out.weatherCode = current["weathercode"] | 0;
  return true;
}

std::string DashboardActivity::weatherDescription(const int code) const {
  switch (code) {
    case 0:
      return "Clear";
    case 1:
      return "Mostly clear";
    case 2:
      return "Partly cloudy";
    case 3:
      return "Overcast";
    case 45:
      return "Fog";
    case 48:
      return "Rime fog";
    case 51:
      return "Light drizzle";
    case 53:
      return "Drizzle";
    case 55:
      return "Heavy drizzle";
    case 61:
      return "Light rain";
    case 63:
      return "Rain";
    case 65:
      return "Heavy rain";
    case 71:
      return "Light snow";
    case 73:
      return "Snow";
    case 75:
      return "Heavy snow";
    case 80:
      return "Rain showers";
    case 95:
      return "Thunder";
    default:
      return "Unknown";
  }
}

void DashboardActivity::render() {
  renderer.clearScreen();
  PaperS3Ui::applyDefaultOrientation(renderer);

#if defined(PLATFORM_M5PAPERS3)
  renderPaperS3();
  return;
#endif

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "Dashboard");

  int y = 58;
  std::tm localTime{};
  if (TimeUtils::getLocalTimeWithOffset(localTime, SETTINGS.timezoneOffsetMinutes)) {
    char timeLine[32];
    char dateLine[32];
    snprintf(timeLine, sizeof(timeLine), "%02d:%02d:%02d", localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    snprintf(dateLine, sizeof(dateLine), "%04d-%02d-%02d", localTime.tm_year + 1900, localTime.tm_mon + 1,
             localTime.tm_mday);
    renderer.drawText(UI_12_FONT_ID, 40, y, timeLine);
    y += 22;
    renderer.drawText(UI_10_FONT_ID, 40, y, dateLine);
  } else {
    renderer.drawText(UI_12_FONT_ID, 40, y, "Time not synced");
    y += 22;
  }

  y += 8;
  char line[96];
  snprintf(line, sizeof(line), "Battery: %d%%", gpio.getBatteryPercentage());
  renderer.drawText(UI_10_FONT_ID, 40, y, line);
  y += 18;

#ifdef PLATFORM_M5PAPER
  const int16_t batteryMv = M5.Power.getBatteryVoltage();
  const bool charging = (M5.Power.isCharging() == m5::Power_Class::is_charging);
  snprintf(line, sizeof(line), "Battery mV: %d   Charging: %s", batteryMv, charging ? "Yes" : "No");
  renderer.drawText(SMALL_FONT_ID, 40, y, line);
  y += 16;
#endif

  if (WiFi.status() == WL_CONNECTED) {
    const String ssid = WiFi.SSID();
    const String ip = WiFi.localIP().toString();
    snprintf(line, sizeof(line), "WiFi: %s", ssid.c_str());
    renderer.drawText(SMALL_FONT_ID, 40, y, line);
    y += 16;
    renderer.drawText(SMALL_FONT_ID, 40, y, ip.c_str());
    y += 16;
  } else {
    renderer.drawText(SMALL_FONT_ID, 40, y, "WiFi: disconnected");
    y += 16;
  }

#if defined(PLATFORM_M5PAPERS3)
  const int16_t vbusMv = M5.Power.getVBUSVoltage();
  renderer.drawText(SMALL_FONT_ID, 40, y,
                    PaperS3Ui::usbCablePresent(vbusMv) ? "USB cable: Present" : "USB cable: Absent");
  y += 16;
  renderer.drawText(SMALL_FONT_ID, 40, y, static_cast<bool>(Serial) ? "USB CDC: Open" : "USB CDC: Waiting");
  y += 16;
#endif

  y += 6;
  if (fetching) {
    renderer.drawText(UI_10_FONT_ID, 40, y, "Weather: fetching...");
  } else if (!snapshot.valid) {
    if (!snapshot.location.empty()) {
      renderer.drawText(UI_10_FONT_ID, 40, y, snapshot.location.c_str());
    } else {
      renderer.drawText(UI_10_FONT_ID, 40, y, "Weather: not loaded");
    }
  } else {
    snprintf(line, sizeof(line), "Weather: %.1f C", snapshot.temperatureC);
    renderer.drawText(UI_10_FONT_ID, 40, y, line);
    y += 18;
    const std::string desc = weatherDescription(snapshot.weatherCode);
    renderer.drawText(UI_10_FONT_ID, 40, y, desc.c_str());
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Confirm: Refresh   Back: Menu");
  renderer.displayBuffer();
}

#if defined(PLATFORM_M5PAPERS3)
void DashboardActivity::renderPaperS3() {
  const auto layout = PaperS3Ui::fourCardLayout(renderer);
  const auto state = readPaperS3DashboardState();
  const int smallLineStep = renderer.getLineHeight(SMALL_FONT_ID) + 4;

  PaperS3Ui::drawScreenHeader(renderer, "Dashboard", "PaperS3 status board");
  PaperS3Ui::drawBackButton(renderer);
  PaperS3Ui::drawPrimaryActionButton(renderer, "Refresh");

  // PaperS3 has enough screen area and onboard telemetry to make the landing
  // screen a compact status board instead of a sparse legacy summary.
  PaperS3Ui::drawCard(renderer, layout.leftX, layout.topY, layout.cardWidth, layout.cardHeight, "Clock");
  PaperS3Ui::drawCard(renderer, layout.rightX, layout.topY, layout.cardWidth, layout.cardHeight, "Power");
  PaperS3Ui::drawCard(renderer, layout.leftX, layout.bottomY, layout.cardWidth, layout.cardHeight, "Weather");
  PaperS3Ui::drawCard(renderer, layout.rightX, layout.bottomY, layout.cardWidth, layout.cardHeight, "Device");

  int x = PaperS3Ui::bodyX(layout.leftX);
  int y = PaperS3Ui::bodyY(layout.topY);
  std::tm localTime{};
  if (TimeUtils::getLocalTimeWithOffset(localTime, SETTINGS.timezoneOffsetMinutes)) {
    char line[96];
    snprintf(line, sizeof(line), "%02d:%02d:%02d", localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    renderer.drawText(UI_12_FONT_ID, x, y, line);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 6;
    snprintf(line, sizeof(line), "%04d-%02d-%02d", localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday);
    renderer.drawText(UI_10_FONT_ID, x, y, line);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 4;
  } else {
    renderer.drawText(UI_12_FONT_ID, x, y, "Time not synced");
    y += renderer.getLineHeight(UI_12_FONT_ID) + 6;
  }

  const std::string tzLabel = formatUtcOffset(SETTINGS.timezoneOffsetMinutes);
  renderer.drawText(SMALL_FONT_ID, x, y, tzLabel.c_str());
  y += smallLineStep;
  renderer.drawText(SMALL_FONT_ID, x, y, state.rtcReady ? "RTC mirror: Ready" : "RTC mirror: Off");

  char line[128];
  x = PaperS3Ui::bodyX(layout.rightX);
  y = PaperS3Ui::bodyY(layout.topY);
  snprintf(line, sizeof(line), "Battery: %d%%", gpio.getBatteryPercentage());
  renderer.drawText(UI_12_FONT_ID, x, y, line);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 6;
  snprintf(line, sizeof(line), "Voltage: %d mV", state.batteryMv);
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "Charging: %s", PaperS3Ui::yesNo(state.charging));
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "VBUS: %d mV", state.vbusMv);
  renderer.drawText(SMALL_FONT_ID, x, y, line);

  x = PaperS3Ui::bodyX(layout.leftX);
  y = PaperS3Ui::bodyY(layout.bottomY);
  if (fetching) {
    renderer.drawText(UI_12_FONT_ID, x, y, "Fetching weather...");
    y += renderer.getLineHeight(UI_12_FONT_ID) + 6;
  } else if (!snapshot.valid) {
    renderer.drawText(UI_12_FONT_ID, x, y, "Weather unavailable");
    y += renderer.getLineHeight(UI_12_FONT_ID) + 6;
    if (!snapshot.location.empty()) {
      const std::string message =
          renderer.truncatedText(SMALL_FONT_ID, snapshot.location.c_str(), layout.cardWidth - 24);
      renderer.drawText(SMALL_FONT_ID, x, y, message.c_str());
      y += smallLineStep;
    }
  } else {
    const std::string location =
        renderer.truncatedText(UI_10_FONT_ID, snapshot.location.c_str(), layout.cardWidth - 24);
    renderer.drawText(UI_10_FONT_ID, x, y, location.c_str());
    y += renderer.getLineHeight(UI_10_FONT_ID) + 6;
    snprintf(line, sizeof(line), "%.1f C", snapshot.temperatureC);
    renderer.drawText(UI_12_FONT_ID, x, y, line);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 6;
    const std::string desc = weatherDescription(snapshot.weatherCode);
    renderer.drawText(SMALL_FONT_ID, x, y, desc.c_str());
    y += smallLineStep;
  }

  if (WiFi.status() == WL_CONNECTED) {
    const String ssid = WiFi.SSID();
    const std::string ssidLine = renderer.truncatedText(SMALL_FONT_ID, ssid.c_str(), layout.cardWidth - 24);
    renderer.drawText(SMALL_FONT_ID, x, y, ssidLine.c_str());
    y += smallLineStep;
    const String ip = WiFi.localIP().toString();
    renderer.drawText(SMALL_FONT_ID, x, y, ip.c_str());
  } else {
    renderer.drawText(SMALL_FONT_ID, x, y, "WiFi: disconnected");
  }

  x = PaperS3Ui::bodyX(layout.rightX);
  y = PaperS3Ui::bodyY(layout.bottomY);
  snprintf(line, sizeof(line), "USB cable: %s", PaperS3Ui::presentAbsent(state.usbCablePresent));
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "CDC session: %s", PaperS3Ui::openWaiting(state.usbSerialOpen));
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  snprintf(line, sizeof(line), "RTC mirror: %s", PaperS3Ui::readyOff(state.rtcReady));
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += smallLineStep;
  renderer.drawText(SMALL_FONT_ID, x, y, "Touch/IMU live stats: Sensors");

  PaperS3Ui::drawFooter(renderer, "Tap Refresh to update weather");
  renderer.displayBuffer();
}
#endif
