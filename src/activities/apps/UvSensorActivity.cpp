#include "UvSensorActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>

#include <cstdlib>
#include <ctime>

#include "MappedInputManager.h"
#include "PaperS3Ui.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"
#include "util/TimeUtils.h"

namespace {
#ifdef PLATFORM_M5PAPER
constexpr int kExI2cSda = 25;
constexpr int kExI2cScl = 32;
#endif
constexpr const char* kLogDir = "/logs";
constexpr const char* kLogFile = "/logs/uv.csv";
}  // namespace

UvSensorActivity::UvSensorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                   const std::function<void()>& onExit)
    : ActivityWithSubactivity("UvSensor", renderer, mappedInput), onExitCb(onExit), uvWire(1) {}

void UvSensorActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
  statusMessage.clear();
  sensorReady = false;
  lastSampleMs = 0;
  initSensor();
  needsRender = true;
}

void UvSensorActivity::initSensor() {
#ifdef PLATFORM_M5PAPER
  uvWire.begin(kExI2cSda, kExI2cScl);
  uvWire.setClock(400000);
#else
  uvWire.begin();
#endif

  sensor.reset(new AS7331(i2cAddress, &uvWire));
  sensorReady = sensor->begin();
  if (!sensorReady) {
    statusMessage = "AS7331 not found";
    return;
  }

  statusMessage = "Ready";
}

void UvSensorActivity::takeSample() {
  if (!sensorReady) {
    initSensor();
    if (!sensorReady) {
      needsRender = true;
      return;
    }
  }

  sensor->startMeasurement();
  const unsigned long start = millis();
  while (!sensor->conversionReady() && (millis() - start) < 1000) {
    delay(5);
  }
  if (!sensor->conversionReady()) {
    statusMessage = "Not ready";
    needsRender = true;
    return;
  }

  uva = sensor->getUVA_uW();
  uvb = sensor->getUVB_uW();
  uvc = sensor->getUVC_uW();
  tempC = sensor->getCelsius();
  const bool logged = logSample();
  if (logged) {
    statusMessage = "OK + logged";
  } else if (!SdMan.ready()) {
    statusMessage = "OK (no SD)";
  } else if (statusMessage.empty() || statusMessage == "OK") {
    statusMessage = "OK (log failed)";
  }
  needsRender = true;
}

bool UvSensorActivity::logSample() {
  if (!SdMan.ready()) {
    return false;
  }

  SdMan.ensureDirectoryExists(kLogDir);
  FsFile logFile = SdMan.open(kLogFile, O_WRITE | O_APPEND | O_CREAT);
  if (!logFile) {
    return false;
  }

  if (logFile.fileSize() == 0) {
    logFile.println("timestamp,addr,uva_uw_cm2,uvb_uw_cm2,uvc_uw_cm2,temp_c");
  }

  const bool hasTime = TimeUtils::isTimeValid();
  const time_t now = time(nullptr);
  const long ts = hasTime ? static_cast<long>(now) : 0L;

  char line[160];
  snprintf(line, sizeof(line), "%ld,0x%02X,%.2f,%.2f,%.2f,%.2f", ts, i2cAddress, uva, uvb, uvc, tempC);
  logFile.println(line);
  logFile.close();
  return true;
}

void UvSensorActivity::promptAddress() {
  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, "I2C Address (hex)", "", 4, 0, false,
      [this](const std::string& value) {
        uint8_t addr = 0;
        if (parseHexAddress(value, addr)) {
          i2cAddress = addr;
          statusMessage = "Address set";
          sensorReady = false;
          initSensor();
        } else {
          statusMessage = "Invalid address";
        }
        exitActivity();
        needsRender = true;
      },
      [this]() {
        exitActivity();
        needsRender = true;
      }));
}

bool UvSensorActivity::parseHexAddress(const std::string& text, uint8_t& out) const {
  std::string s = text;
  if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
    s = s.substr(2);
  }
  if (s.empty() || s.size() > 2) {
    return false;
  }
  char* end = nullptr;
  const long value = strtol(s.c_str(), &end, 16);
  if (!end || *end != '\0') {
    return false;
  }
  if (value <= 0 || value > 0x77) {
    return false;
  }
  out = static_cast<uint8_t>(value);
  return true;
}

void UvSensorActivity::loop() {
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
      promptAddress();
      return;
    }
    if (PaperS3Ui::listRowRect(renderer, 1).contains(tapX, tapY)) {
      autoSample = !autoSample;
      statusMessage = autoSample ? "Auto sample on" : "Auto sample off";
      needsRender = true;
      return;
    }
    if (PaperS3Ui::primaryActionRect(renderer).contains(tapX, tapY)) {
      takeSample();
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
    takeSample();
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    promptAddress();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    autoSample = !autoSample;
    statusMessage = autoSample ? "Auto sample on" : "Auto sample off";
    needsRender = true;
  }

  if (autoSample) {
    const unsigned long now = millis();
    if (now - lastSampleMs > sampleIntervalMs) {
      lastSampleMs = now;
      takeSample();
    }
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void UvSensorActivity::render() {
  renderer.clearScreen();
#if defined(PLATFORM_M5PAPERS3)
  {
    renderer.setOrientation(GfxRenderer::Orientation::Portrait);
    PaperS3Ui::drawScreenHeader(renderer, "UV Sensor", sensorReady ? "AS7331 connected" : "Waiting for AS7331");
    PaperS3Ui::drawBackButton(renderer);

    char addrLine[16];
    snprintf(addrLine, sizeof(addrLine), "0x%02X", i2cAddress);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 0), false, "I2C Address", addrLine,
                           "Tap row to edit");
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 1), false, "Auto Sample",
                           autoSample ? "Enabled" : "Disabled", "Tap row to toggle");

    char line[32];
    snprintf(line, sizeof(line), "%.1f uW/cm2", uva);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 2), false, "UVA", line);
    snprintf(line, sizeof(line), "%.1f uW/cm2", uvb);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 3), false, "UVB", line);
    snprintf(line, sizeof(line), "%.1f uW/cm2", uvc);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 4), false, "UVC", line);
    snprintf(line, sizeof(line), "%.1f C", tempC);
    PaperS3Ui::drawListRow(renderer, PaperS3Ui::listRowRect(renderer, 5), false, "Temperature", line);

    PaperS3Ui::drawPrimaryActionButton(renderer, "Sample Now");
    if (!statusMessage.empty()) {
      PaperS3Ui::drawFooterStatus(renderer, statusMessage.c_str());
    }
    PaperS3Ui::drawFooter(renderer, "Tap I2C Address to open the keyboard");
    renderer.displayBuffer();
    return;
  }
#endif

  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  renderer.drawCenteredText(UI_12_FONT_ID, 16, "UV Sensor (AS7331)");

  char addrLine[32];
  snprintf(addrLine, sizeof(addrLine), "Addr: 0x%02X", i2cAddress);
  renderer.drawText(UI_12_FONT_ID, 40, 60, addrLine);

  int y = 100;
  char line[48];
  snprintf(line, sizeof(line), "UVA: %.1f uW/cm2", uva);
  renderer.drawText(UI_12_FONT_ID, 40, y, line);
  y += 28;
  snprintf(line, sizeof(line), "UVB: %.1f uW/cm2", uvb);
  renderer.drawText(UI_12_FONT_ID, 40, y, line);
  y += 28;
  snprintf(line, sizeof(line), "UVC: %.1f uW/cm2", uvc);
  renderer.drawText(UI_12_FONT_ID, 40, y, line);
  y += 28;
  snprintf(line, sizeof(line), "Temp: %.1f C", tempC);
  renderer.drawText(UI_12_FONT_ID, 40, y, line);

  if (!statusMessage.empty()) {
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 44, statusMessage.c_str());
  }

  const char* autoLabel = autoSample ? "Auto: ON" : "Auto: OFF";
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                            "Confirm: Sample   Left: Addr   Right: Auto   Back: Menu");
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 64, autoLabel);

  renderer.displayBuffer();
}
