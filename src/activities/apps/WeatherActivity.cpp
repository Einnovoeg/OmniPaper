#include "WeatherActivity.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "MappedInputManager.h"
#include "PaperS3Ui.h"
#include "fontIds.h"

namespace {
constexpr unsigned long kUpdateIntervalMs = 10UL * 60UL * 1000UL;  // 10 minutes
}

WeatherActivity::WeatherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                 const std::function<void()>& onExit)
    : Activity("Weather", renderer, mappedInput), onExit(onExit) {}

void WeatherActivity::onEnter() {
  Activity::onEnter();
  lastUpdate = millis();
  snapshot.valid = false;
  snapshot.fetchedAt = 0;
  snapshot.location = "Tap Refresh to load weather";
  needsRender = true;
}

void WeatherActivity::loop() {
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
      lastUpdate = 0;
      updateWeather();
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
    lastUpdate = 0;
    updateWeather();
  }

  if (snapshot.fetchedAt != 0 && millis() - lastUpdate > kUpdateIntervalMs) {
    updateWeather();
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void WeatherActivity::updateWeather() {
  lastUpdate = millis();
  fetching = true;
  needsRender = true;

  // Paint the busy state before starting synchronous HTTP/TLS work so the
  // device does not look wedged while the network stack is busy.
  render();
  needsRender = false;
  delay(1);

  if (WiFi.status() != WL_CONNECTED) {
    snapshot.valid = false;
    snapshot.fetchedAt = 0;
    snapshot.location = "WiFi disconnected";
    fetching = false;
    needsRender = true;
    return;
  }

  LocationInfo loc;
  if (!fetchLocation(loc)) {
    snapshot.valid = false;
    snapshot.fetchedAt = 0;
    snapshot.location = "Location lookup failed";
    fetching = false;
    needsRender = true;
    return;
  }

  if (!fetchWeather(loc, snapshot)) {
    snapshot.valid = false;
    snapshot.fetchedAt = 0;
    snapshot.location = loc.city;
    fetching = false;
    needsRender = true;
    return;
  }

  snapshot.location = loc.city;
  snapshot.fetchedAt = lastUpdate;
  snapshot.valid = true;
  fetching = false;
  needsRender = true;
}

bool WeatherActivity::fetchLocation(LocationInfo& out) {
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

bool WeatherActivity::fetchWeather(const LocationInfo& loc, WeatherSnapshot& out) {
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
  out.windSpeed = current["windspeed"] | 0.0f;
  out.weatherCode = current["weathercode"] | 0;
  return true;
}

std::string WeatherActivity::weatherDescription(int code) const {
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

void WeatherActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  PaperS3Ui::drawScreenHeader(renderer, "Weather", snapshot.location.c_str());
  PaperS3Ui::drawBackButton(renderer);

  if (fetching) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "Fetching...");
    PaperS3Ui::drawFooter(renderer, "Refreshing weather");
    renderer.displayBuffer();
    return;
  }

  if (!snapshot.valid) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, snapshot.location.c_str());
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() / 2 + 30, "Check WiFi / retry");
    PaperS3Ui::drawPrimaryActionButton(renderer, "Refresh");
    PaperS3Ui::drawFooter(renderer, "Tap Refresh to try again");
    renderer.displayBuffer();
    return;
  }

  char line[64];
  PaperS3Ui::drawCard(renderer, 24, 112, 492, 170, "Current Conditions");
  int y = 160;

  snprintf(line, sizeof(line), "Temp: %.1f C", snapshot.temperatureC);
  renderer.drawText(UI_12_FONT_ID, 40, y, line);
  y += 30;

  snprintf(line, sizeof(line), "Wind: %.1f m/s", snapshot.windSpeed);
  renderer.drawText(UI_12_FONT_ID, 40, y, line);
  y += 30;

  std::string desc = weatherDescription(snapshot.weatherCode);
  renderer.drawText(UI_12_FONT_ID, 40, y, desc.c_str());

  PaperS3Ui::drawPrimaryActionButton(renderer, "Refresh");
  PaperS3Ui::drawFooter(renderer, "Tap Refresh to update");
  renderer.displayBuffer();
}
