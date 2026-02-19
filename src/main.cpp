#include <Arduino.h>
#include <Epub.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <SDCardManager.h>
#include <SPI.h>
#include <esp_sleep.h>
#include <memory>
#ifdef PLATFORM_M5PAPER
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include <builtinFonts/notosans_8_regular.h>
#include "fonts/SdFontLoader.h"
#else
#include <builtinFonts/all.h>
#endif

#include <cstring>

#include "Battery.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "KOReaderCredentialStore.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "activities/boot_sleep/BootActivity.h"
#include "activities/boot_sleep/SleepActivity.h"
#include "activities/browser/OpdsBookBrowserActivity.h"
#include "activities/home/HomeActivity.h"
#include "activities/home/MyLibraryActivity.h"
#include "activities/network/CrossPointWebServerActivity.h"
#include "activities/reader/ReaderActivity.h"
#include "activities/settings/SettingsActivity.h"
#include "activities/util/FullScreenMessageActivity.h"
#include "fontIds.h"
#ifdef PLATFORM_M5PAPER
#include "activities/apps/CalculatorActivity.h"
#include "activities/apps/DashboardActivity.h"
#include "activities/apps/DrawingActivity.h"
#include "activities/apps/FileManagerActivity.h"
#include "activities/apps/GpioMonitorActivity.h"
#include "activities/apps/HardwareTestActivity.h"
#include "activities/apps/I2CToolsActivity.h"
#include "activities/apps/I2cEepromDumpActivity.h"
#include "activities/apps/ImageViewerActivity.h"
#include "activities/apps/KeyboardHostActivity.h"
#include "activities/apps/NotesActivity.h"
#include "activities/apps/ToolsOtaUpdateActivity.h"
#include "activities/apps/PoodleActivity.h"
#include "activities/apps/SensorsActivity.h"
#include "activities/apps/SleepTimerActivity.h"
#include "activities/apps/SshClientActivity.h"
#include "activities/apps/SudokuActivity.h"
#include "activities/apps/TrackpadActivity.h"
#include "activities/apps/TimeSettingsActivity.h"
#include "activities/apps/UartRxMonitorActivity.h"
#include "activities/apps/UvSensorActivity.h"
#include "activities/apps/UvLogViewerActivity.h"
#include "activities/apps/WeatherActivity.h"
#include "activities/launcher/LauncherActivity.h"
#include "activities/launcher/LauncherActions.h"
#include "activities/network/BleScannerActivity.h"
#include "activities/network/WifiChannelMonitorActivity.h"
#include "activities/network/WifiScannerActivity.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/network/WifiStatusActivity.h"
#include "activities/network/WifiTestsActivity.h"
#include "network/CrossPointWebServer.h"
#endif

HalDisplay display;
HalGPIO gpio;
MappedInputManager mappedInputManager(gpio);
GfxRenderer renderer(display);
Activity* currentActivity;
#ifdef PLATFORM_M5PAPER
std::string pendingReaderPath;

namespace {
constexpr const char* IDLE_WEB_AP_SSID = "OmniPaper";
constexpr const char* IDLE_WEB_AP_HOSTNAME = "omnipaper";
constexpr uint8_t IDLE_WEB_AP_CHANNEL = 1;
constexpr uint8_t IDLE_WEB_AP_MAX_CONNECTIONS = 4;
constexpr uint16_t IDLE_WEB_DNS_PORT = 53;

enum class PendingWebCommandType : uint8_t { None = 0, LaunchAction, OpenReader };

struct PendingWebCommand {
  PendingWebCommandType type = PendingWebCommandType::None;
  LauncherAction action = LauncherAction::None;
  std::string path;
};

std::unique_ptr<CrossPointWebServer> idleWebServer;
std::unique_ptr<DNSServer> idleDnsServer;
PendingWebCommand pendingWebCommand;
bool idleHotspotActive = false;
}  // namespace
#endif

// Fonts
#ifdef PLATFORM_M5PAPER
EpdFont fallbackFont(&notosans_8_regular);
EpdFontFamily fallbackFontFamily(&fallbackFont);
#else
EpdFont bookerly14RegularFont(&bookerly_14_regular);
EpdFont bookerly14BoldFont(&bookerly_14_bold);
EpdFont bookerly14ItalicFont(&bookerly_14_italic);
EpdFont bookerly14BoldItalicFont(&bookerly_14_bolditalic);
EpdFontFamily bookerly14FontFamily(&bookerly14RegularFont, &bookerly14BoldFont, &bookerly14ItalicFont,
                                   &bookerly14BoldItalicFont);
#ifndef OMIT_FONTS
EpdFont bookerly12RegularFont(&bookerly_12_regular);
EpdFont bookerly12BoldFont(&bookerly_12_bold);
EpdFont bookerly12ItalicFont(&bookerly_12_italic);
EpdFont bookerly12BoldItalicFont(&bookerly_12_bolditalic);
EpdFontFamily bookerly12FontFamily(&bookerly12RegularFont, &bookerly12BoldFont, &bookerly12ItalicFont,
                                   &bookerly12BoldItalicFont);
EpdFont bookerly16RegularFont(&bookerly_16_regular);
EpdFont bookerly16BoldFont(&bookerly_16_bold);
EpdFont bookerly16ItalicFont(&bookerly_16_italic);
EpdFont bookerly16BoldItalicFont(&bookerly_16_bolditalic);
EpdFontFamily bookerly16FontFamily(&bookerly16RegularFont, &bookerly16BoldFont, &bookerly16ItalicFont,
                                   &bookerly16BoldItalicFont);
EpdFont bookerly18RegularFont(&bookerly_18_regular);
EpdFont bookerly18BoldFont(&bookerly_18_bold);
EpdFont bookerly18ItalicFont(&bookerly_18_italic);
EpdFont bookerly18BoldItalicFont(&bookerly_18_bolditalic);
EpdFontFamily bookerly18FontFamily(&bookerly18RegularFont, &bookerly18BoldFont, &bookerly18ItalicFont,
                                   &bookerly18BoldItalicFont);

EpdFont notosans12RegularFont(&notosans_12_regular);
EpdFont notosans12BoldFont(&notosans_12_bold);
EpdFont notosans12ItalicFont(&notosans_12_italic);
EpdFont notosans12BoldItalicFont(&notosans_12_bolditalic);
EpdFontFamily notosans12FontFamily(&notosans12RegularFont, &notosans12BoldFont, &notosans12ItalicFont,
                                   &notosans12BoldItalicFont);
EpdFont notosans14RegularFont(&notosans_14_regular);
EpdFont notosans14BoldFont(&notosans_14_bold);
EpdFont notosans14ItalicFont(&notosans_14_italic);
EpdFont notosans14BoldItalicFont(&notosans_14_bolditalic);
EpdFontFamily notosans14FontFamily(&notosans14RegularFont, &notosans14BoldFont, &notosans14ItalicFont,
                                   &notosans14BoldItalicFont);
EpdFont notosans16RegularFont(&notosans_16_regular);
EpdFont notosans16BoldFont(&notosans_16_bold);
EpdFont notosans16ItalicFont(&notosans_16_italic);
EpdFont notosans16BoldItalicFont(&notosans_16_bolditalic);
EpdFontFamily notosans16FontFamily(&notosans16RegularFont, &notosans16BoldFont, &notosans16ItalicFont,
                                   &notosans16BoldItalicFont);
EpdFont notosans18RegularFont(&notosans_18_regular);
EpdFont notosans18BoldFont(&notosans_18_bold);
EpdFont notosans18ItalicFont(&notosans_18_italic);
EpdFont notosans18BoldItalicFont(&notosans_18_bolditalic);
EpdFontFamily notosans18FontFamily(&notosans18RegularFont, &notosans18BoldFont, &notosans18ItalicFont,
                                   &notosans18BoldItalicFont);

EpdFont opendyslexic8RegularFont(&opendyslexic_8_regular);
EpdFont opendyslexic8BoldFont(&opendyslexic_8_bold);
EpdFont opendyslexic8ItalicFont(&opendyslexic_8_italic);
EpdFont opendyslexic8BoldItalicFont(&opendyslexic_8_bolditalic);
EpdFontFamily opendyslexic8FontFamily(&opendyslexic8RegularFont, &opendyslexic8BoldFont, &opendyslexic8ItalicFont,
                                      &opendyslexic8BoldItalicFont);
EpdFont opendyslexic10RegularFont(&opendyslexic_10_regular);
EpdFont opendyslexic10BoldFont(&opendyslexic_10_bold);
EpdFont opendyslexic10ItalicFont(&opendyslexic_10_italic);
EpdFont opendyslexic10BoldItalicFont(&opendyslexic_10_bolditalic);
EpdFontFamily opendyslexic10FontFamily(&opendyslexic10RegularFont, &opendyslexic10BoldFont, &opendyslexic10ItalicFont,
                                       &opendyslexic10BoldItalicFont);
EpdFont opendyslexic12RegularFont(&opendyslexic_12_regular);
EpdFont opendyslexic12BoldFont(&opendyslexic_12_bold);
EpdFont opendyslexic12ItalicFont(&opendyslexic_12_italic);
EpdFont opendyslexic12BoldItalicFont(&opendyslexic_12_bolditalic);
EpdFontFamily opendyslexic12FontFamily(&opendyslexic12RegularFont, &opendyslexic12BoldFont, &opendyslexic12ItalicFont,
                                       &opendyslexic12BoldItalicFont);
EpdFont opendyslexic14RegularFont(&opendyslexic_14_regular);
EpdFont opendyslexic14BoldFont(&opendyslexic_14_bold);
EpdFont opendyslexic14ItalicFont(&opendyslexic_14_italic);
EpdFont opendyslexic14BoldItalicFont(&opendyslexic_14_bolditalic);
EpdFontFamily opendyslexic14FontFamily(&opendyslexic14RegularFont, &opendyslexic14BoldFont, &opendyslexic14ItalicFont,
                                       &opendyslexic14BoldItalicFont);
#endif  // OMIT_FONTS

EpdFont smallFont(&notosans_8_regular);
EpdFontFamily smallFontFamily(&smallFont);

EpdFont ui10RegularFont(&ubuntu_10_regular);
EpdFont ui10BoldFont(&ubuntu_10_bold);
EpdFontFamily ui10FontFamily(&ui10RegularFont, &ui10BoldFont);

EpdFont ui12RegularFont(&ubuntu_12_regular);
EpdFont ui12BoldFont(&ubuntu_12_bold);
EpdFontFamily ui12FontFamily(&ui12RegularFont, &ui12BoldFont);
#endif

// measurement of power button press duration calibration value
unsigned long t1 = 0;
unsigned long t2 = 0;

#ifdef PLATFORM_M5PAPER
void stopIdleHotspotWebUi();
#endif

void exitActivity() {
  if (currentActivity) {
    currentActivity->onExit();
    delete currentActivity;
    currentActivity = nullptr;
  }
}

void enterNewActivity(Activity* activity) {
  currentActivity = activity;
  currentActivity->onEnter();
}

// Verify power button press duration on wake-up from deep sleep
// Pre-condition: isWakeupByPowerButton() == true
void verifyPowerButtonDuration() {
  if (SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::SLEEP) {
    // Fast path for short press
    // Needed because inputManager.isPressed() may take up to ~500ms to return the correct state
    return;
  }

  // Give the user up to 1000ms to start holding the power button, and must hold for SETTINGS.getPowerButtonDuration()
  const auto start = millis();
  bool abort = false;
  // Subtract the current time, because inputManager only starts counting the HeldTime from the first update()
  // This way, we remove the time we already took to reach here from the duration,
  // assuming the button was held until now from millis()==0 (i.e. device start time).
  const uint16_t calibration = start;
  const uint16_t calibratedPressDuration =
      (calibration < SETTINGS.getPowerButtonDuration()) ? SETTINGS.getPowerButtonDuration() - calibration : 1;

  gpio.update();
  // Needed because inputManager.isPressed() may take up to ~500ms to return the correct state
  while (!gpio.isPressed(HalGPIO::BTN_POWER) && millis() - start < 1000) {
    delay(10);  // only wait 10ms each iteration to not delay too much in case of short configured duration.
    gpio.update();
  }

  t2 = millis();
  if (gpio.isPressed(HalGPIO::BTN_POWER)) {
    do {
      delay(10);
      gpio.update();
    } while (gpio.isPressed(HalGPIO::BTN_POWER) && gpio.getHeldTime() < calibratedPressDuration);
    abort = gpio.getHeldTime() < calibratedPressDuration;
  } else {
    abort = true;
  }

  if (abort) {
    // Button released too early. Returning to sleep.
    // IMPORTANT: Re-arm the wakeup trigger before sleeping again
    gpio.startDeepSleep();
  }
}

void waitForPowerRelease() {
  gpio.update();
  while (gpio.isPressed(HalGPIO::BTN_POWER)) {
    delay(50);
    gpio.update();
  }
}

// Enter deep sleep mode
void enterDeepSleep() {
#ifdef PLATFORM_M5PAPER
  stopIdleHotspotWebUi();
#endif
  exitActivity();
  enterNewActivity(new SleepActivity(renderer, mappedInputManager));

  display.deepSleep();
  Serial.printf("[%lu] [   ] Power button press calibration value: %lu ms\n", millis(), t2 - t1);
  Serial.printf("[%lu] [   ] Entering deep sleep.\n", millis());

  gpio.startDeepSleep();
}

#ifdef PLATFORM_M5PAPER
void enterDeepSleepForSeconds(const uint64_t seconds) {
  stopIdleHotspotWebUi();
  exitActivity();
  enterNewActivity(new SleepActivity(renderer, mappedInputManager));

  display.deepSleep();
  const uint64_t us = seconds * 1000000ULL;
  Serial.printf("[%lu] [SLP] Entering timed deep sleep (%llu seconds)\n", millis(),
                static_cast<unsigned long long>(seconds));
  M5.Power.deepSleep(us, true);
}
#endif

void onGoHome();
void onGoToMyLibraryWithTab(const std::string& path, MyLibraryActivity::Tab tab);

#ifdef PLATFORM_M5PAPER
void onGoLauncher();
void handleLauncherAction(LauncherAction action);
void enterDeepSleepForSeconds(uint64_t seconds);
bool isLauncherIdleState();
void stopIdleHotspotWebUi();
void updateIdleHotspotWebUi();
bool idleHotspotShouldPreventSleep();
void processPendingWebCommand();
#endif

void onGoToReader(const std::string& initialEpubPath, MyLibraryActivity::Tab fromTab) {
  exitActivity();
  enterNewActivity(
      new ReaderActivity(renderer, mappedInputManager, initialEpubPath, fromTab, onGoHome, onGoToMyLibraryWithTab));
}
void onContinueReading() { onGoToReader(APP_STATE.openEpubPath, MyLibraryActivity::Tab::Recent); }

void onGoToFileTransfer() {
  exitActivity();
  enterNewActivity(new CrossPointWebServerActivity(renderer, mappedInputManager, onGoHome));
}

void onGoToSettings() {
  exitActivity();
  enterNewActivity(new SettingsActivity(renderer, mappedInputManager, onGoHome));
}

void onGoToMyLibrary() {
  exitActivity();
  enterNewActivity(new MyLibraryActivity(renderer, mappedInputManager, onGoHome, onGoToReader));
}

void onGoToMyLibraryWithTab(const std::string& path, MyLibraryActivity::Tab tab) {
  exitActivity();
  enterNewActivity(new MyLibraryActivity(renderer, mappedInputManager, onGoHome, onGoToReader, tab, path));
}

void onGoToBrowser() {
  exitActivity();
  enterNewActivity(new OpdsBookBrowserActivity(renderer, mappedInputManager, onGoHome));
}

void onGoHome() {
#ifdef PLATFORM_M5PAPER
  onGoLauncher();
#else
  exitActivity();
  enterNewActivity(new HomeActivity(renderer, mappedInputManager, onContinueReading, onGoToMyLibrary, onGoToSettings,
                                    onGoToFileTransfer, onGoToBrowser));
#endif
}

#ifdef PLATFORM_M5PAPER
void onGoLauncher() {
  exitActivity();
  enterNewActivity(new LauncherActivity(renderer, mappedInputManager, handleLauncherAction));
}

void handleLauncherAction(LauncherAction action) {
  stopIdleHotspotWebUi();

  switch (action) {
    case LauncherAction::Reader:
      if (!pendingReaderPath.empty()) {
        const auto path = pendingReaderPath;
        pendingReaderPath.clear();
        onGoToReader(path, MyLibraryActivity::Tab::Recent);
      } else {
        onGoToMyLibrary();
      }
      break;
    case LauncherAction::Dashboard:
      exitActivity();
      enterNewActivity(new DashboardActivity(renderer, mappedInputManager, gpio, onGoLauncher));
      break;
    case LauncherAction::SensorsBuiltIn:
      exitActivity();
      enterNewActivity(new SensorsActivity(renderer, mappedInputManager, gpio, SensorsActivity::Mode::BuiltIn, onGoLauncher));
      break;
    case LauncherAction::SensorsExternal:
      exitActivity();
      enterNewActivity(new SensorsActivity(renderer, mappedInputManager, gpio, SensorsActivity::Mode::External, onGoLauncher));
      break;
    case LauncherAction::SensorsI2CTools:
      exitActivity();
      enterNewActivity(new I2CToolsActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::SensorsUv:
      exitActivity();
      enterNewActivity(new UvSensorActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::SensorsUvLogs:
      exitActivity();
      enterNewActivity(new UvLogViewerActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::SensorsGpio:
      exitActivity();
      enterNewActivity(new GpioMonitorActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::SensorsUartRx:
      exitActivity();
      enterNewActivity(new UartRxMonitorActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::SensorsEepromDump:
      exitActivity();
      enterNewActivity(new I2cEepromDumpActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::Weather:
      exitActivity();
      enterNewActivity(new WeatherActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::NetworkWifiScan:
      exitActivity();
      enterNewActivity(new WifiScannerActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::NetworkWifiStatus:
      exitActivity();
      enterNewActivity(new WifiStatusActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::NetworkWifiTests:
      exitActivity();
      enterNewActivity(new WifiTestsActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::NetworkWifiChannels:
      exitActivity();
      enterNewActivity(new WifiChannelMonitorActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::NetworkBleScan:
      exitActivity();
      enterNewActivity(new BleScannerActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::NetworkWebPortal:
      exitActivity();
      enterNewActivity(new CrossPointWebServerActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::NetworkKeyboardHost:
      exitActivity();
      enterNewActivity(new KeyboardHostActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::NetworkTrackpad:
      exitActivity();
      enterNewActivity(new TrackpadActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::NetworkSshClient:
      exitActivity();
      enterNewActivity(new SshClientActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::GamePoodle:
      exitActivity();
      enterNewActivity(new PoodleActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::GameSudoku:
      exitActivity();
      enterNewActivity(new SudokuActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::ImagesViewer:
      exitActivity();
      enterNewActivity(new ImageViewerActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::ImagesDraw:
      exitActivity();
      enterNewActivity(new DrawingActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::ToolsNotes:
      exitActivity();
      enterNewActivity(new NotesActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::ToolsFileManager:
      exitActivity();
      enterNewActivity(new FileManagerActivity(renderer, mappedInputManager, onGoLauncher,
                                               [](const std::string& path) {
                                                 onGoToReader(path, MyLibraryActivity::Tab::Recent);
                                               }));
      break;
    case LauncherAction::ToolsTime:
      exitActivity();
      enterNewActivity(new TimeSettingsActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::ToolsSleepTimer:
      exitActivity();
      enterNewActivity(new SleepTimerActivity(renderer, mappedInputManager, enterDeepSleepForSeconds, onGoLauncher));
      break;
    case LauncherAction::ToolsOtaUpdate:
      exitActivity();
      enterNewActivity(new ToolsOtaUpdateActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::SettingsWifi:
      exitActivity();
      enterNewActivity(new WifiSelectionActivity(renderer, mappedInputManager, [](bool) { onGoLauncher(); }));
      break;
    case LauncherAction::SettingsHardwareTest:
      exitActivity();
      enterNewActivity(new HardwareTestActivity(renderer, mappedInputManager, gpio, onGoLauncher));
      break;
    case LauncherAction::Calculator:
      exitActivity();
      enterNewActivity(new CalculatorActivity(renderer, mappedInputManager, onGoLauncher));
      break;
    case LauncherAction::None:
    default:
      break;
  }
}

bool mapWebAppIdToAction(const std::string& appId, LauncherAction& action) {
  if (appId == "dashboard") {
    action = LauncherAction::Dashboard;
  } else if (appId == "sensors-built-in") {
    action = LauncherAction::SensorsBuiltIn;
  } else if (appId == "sensors-external") {
    action = LauncherAction::SensorsExternal;
  } else if (appId == "weather") {
    action = LauncherAction::Weather;
  } else if (appId == "wifi-status") {
    action = LauncherAction::NetworkWifiStatus;
  } else if (appId == "wifi-scan") {
    action = LauncherAction::NetworkWifiScan;
  } else if (appId == "ble-scan") {
    action = LauncherAction::NetworkBleScan;
  } else if (appId == "web-portal") {
    action = LauncherAction::NetworkWebPortal;
  } else if (appId == "host-keyboard") {
    action = LauncherAction::NetworkKeyboardHost;
  } else if (appId == "trackpad") {
    action = LauncherAction::NetworkTrackpad;
  } else if (appId == "ssh-client") {
    action = LauncherAction::NetworkSshClient;
  } else if (appId == "image-viewer") {
    action = LauncherAction::ImagesViewer;
  } else if (appId == "drawing") {
    action = LauncherAction::ImagesDraw;
  } else if (appId == "notes") {
    action = LauncherAction::ToolsNotes;
  } else if (appId == "calculator") {
    action = LauncherAction::Calculator;
  } else if (appId == "poodle") {
    action = LauncherAction::GamePoodle;
  } else if (appId == "sudoku") {
    action = LauncherAction::GameSudoku;
  } else if (appId == "file-manager") {
    action = LauncherAction::ToolsFileManager;
  } else if (appId == "hardware-test") {
    action = LauncherAction::SettingsHardwareTest;
  } else {
    return false;
  }
  return true;
}

bool isLauncherIdleState() { return currentActivity && currentActivity->getName() == "Launcher"; }

void stopIdleHotspotWebUi() {
  if (!idleHotspotActive && !idleWebServer && !idleDnsServer) {
    return;
  }

  idleHotspotActive = false;

  if (idleWebServer) {
    idleWebServer->stop();
    idleWebServer.reset();
  }
  if (idleDnsServer) {
    idleDnsServer->stop();
    idleDnsServer.reset();
  }

  if (WiFi.getMode() & WIFI_MODE_AP) {
    MDNS.end();
    WiFi.softAPdisconnect(true);
    delay(20);

    // The idle web UI uses AP-only mode, so power down WiFi when shutting it down.
    if (WiFi.getMode() == WIFI_AP) {
      WiFi.mode(WIFI_OFF);
      delay(20);
    }
  }
}

void startIdleHotspotWebUi() {
  Serial.printf("[%lu] [IWEB] Starting idle hotspot web UI...\n", millis());

  WiFi.mode(WIFI_AP);
  delay(80);

  if (!WiFi.softAP(IDLE_WEB_AP_SSID, nullptr, IDLE_WEB_AP_CHANNEL, false, IDLE_WEB_AP_MAX_CONNECTIONS)) {
    Serial.printf("[%lu] [IWEB] Failed to start AP\n", millis());
    return;
  }
  idleHotspotActive = true;
  delay(80);

  if (MDNS.begin(IDLE_WEB_AP_HOSTNAME)) {
    Serial.printf("[%lu] [IWEB] mDNS started: http://%s.local/\n", millis(), IDLE_WEB_AP_HOSTNAME);
  }

  idleDnsServer.reset(new DNSServer());
  idleDnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  idleDnsServer->start(IDLE_WEB_DNS_PORT, "*", WiFi.softAPIP());

  idleWebServer.reset(new CrossPointWebServer());
  idleWebServer->setReaderOpenCallback([](const std::string& path) -> bool {
    if (pendingWebCommand.type != PendingWebCommandType::None) {
      return false;
    }
    pendingWebCommand.type = PendingWebCommandType::OpenReader;
    pendingWebCommand.path = path;
    pendingWebCommand.action = LauncherAction::None;
    return true;
  });
  idleWebServer->setAppLaunchCallback([](const std::string& appId) -> bool {
    if (pendingWebCommand.type != PendingWebCommandType::None) {
      return false;
    }
    LauncherAction action = LauncherAction::None;
    if (!mapWebAppIdToAction(appId, action)) {
      return false;
    }
    pendingWebCommand.type = PendingWebCommandType::LaunchAction;
    pendingWebCommand.action = action;
    pendingWebCommand.path.clear();
    return true;
  });
  idleWebServer->begin();

  if (!idleWebServer->isRunning()) {
    Serial.printf("[%lu] [IWEB] Web server failed to start\n", millis());
    stopIdleHotspotWebUi();
  } else {
    Serial.printf("[%lu] [IWEB] Hotspot ready: SSID %s, URL http://%s/\n", millis(), IDLE_WEB_AP_SSID,
                  WiFi.softAPIP().toString().c_str());
  }
}

void updateIdleHotspotWebUi() {
  static unsigned long retryAfterMs = 0;

  const bool shouldRun = SETTINGS.idleHotspotWebUi && isLauncherIdleState();
  if (!shouldRun) {
    stopIdleHotspotWebUi();
    return;
  }

  if (!idleWebServer || !idleWebServer->isRunning()) {
    if (retryAfterMs > millis()) {
      return;
    }
    startIdleHotspotWebUi();
    if (!idleWebServer || !idleWebServer->isRunning()) {
      retryAfterMs = millis() + 5000;
      return;
    }
    retryAfterMs = 0;
  }

  if (idleDnsServer) {
    idleDnsServer->processNextRequest();
  }

  constexpr int MAX_ITERATIONS = 20;
  for (int i = 0; i < MAX_ITERATIONS && idleWebServer && idleWebServer->isRunning(); i++) {
    idleWebServer->handleClient();
    if ((i & 0x07) == 0x07) {
      yield();
    }
  }
}

bool idleHotspotShouldPreventSleep() {
  if (!idleWebServer || !idleWebServer->isRunning()) {
    return false;
  }
  if (WiFi.softAPgetStationNum() > 0) {
    return true;
  }
  return idleWebServer->getWsUploadStatus().inProgress;
}

void processPendingWebCommand() {
  if (pendingWebCommand.type == PendingWebCommandType::None) {
    return;
  }

  PendingWebCommand command = pendingWebCommand;
  pendingWebCommand = {};

  if (command.type == PendingWebCommandType::OpenReader && !command.path.empty()) {
    stopIdleHotspotWebUi();
    onGoToReader(command.path, MyLibraryActivity::Tab::Recent);
    return;
  }

  if (command.type == PendingWebCommandType::LaunchAction && command.action != LauncherAction::None) {
    handleLauncherAction(command.action);
  }
}
#endif

void setupDisplayAndFonts() {
  display.begin();
  Serial.printf("[%lu] [   ] Display initialized\n", millis());
#ifdef PLATFORM_M5PAPER
  const bool allLoaded = SdFontLoader::registerFonts(renderer, fallbackFontFamily);
  if (!allLoaded) {
    Serial.printf("[%lu] [   ] One or more SD fonts missing; using fallback\n", millis());
  }
#else
  renderer.insertFont(BOOKERLY_14_FONT_ID, bookerly14FontFamily);
#ifndef OMIT_FONTS
  renderer.insertFont(BOOKERLY_12_FONT_ID, bookerly12FontFamily);
  renderer.insertFont(BOOKERLY_16_FONT_ID, bookerly16FontFamily);
  renderer.insertFont(BOOKERLY_18_FONT_ID, bookerly18FontFamily);

  renderer.insertFont(NOTOSANS_12_FONT_ID, notosans12FontFamily);
  renderer.insertFont(NOTOSANS_14_FONT_ID, notosans14FontFamily);
  renderer.insertFont(NOTOSANS_16_FONT_ID, notosans16FontFamily);
  renderer.insertFont(NOTOSANS_18_FONT_ID, notosans18FontFamily);
  renderer.insertFont(OPENDYSLEXIC_8_FONT_ID, opendyslexic8FontFamily);
  renderer.insertFont(OPENDYSLEXIC_10_FONT_ID, opendyslexic10FontFamily);
  renderer.insertFont(OPENDYSLEXIC_12_FONT_ID, opendyslexic12FontFamily);
  renderer.insertFont(OPENDYSLEXIC_14_FONT_ID, opendyslexic14FontFamily);
#endif  // OMIT_FONTS
  renderer.insertFont(UI_10_FONT_ID, ui10FontFamily);
  renderer.insertFont(UI_12_FONT_ID, ui12FontFamily);
  renderer.insertFont(SMALL_FONT_ID, smallFontFamily);
#endif
  Serial.printf("[%lu] [   ] Fonts setup\n", millis());
}

#ifdef PLATFORM_M5PAPER
void waitDisplayWithTimeout() {
  const uint32_t start = millis();
  constexpr uint32_t kDisplayWaitTimeoutMs = 4000;
  while (M5.Display.displayBusy()) {
    if (millis() - start >= kDisplayWaitTimeoutMs) {
      if (Serial) {
        Serial.println("[M5Paper] displayBusy timeout in splash");
      }
      break;
    }
    delay(1);
  }
}

void showBootSplash() {
  if (M5.Display.width() < M5.Display.height()) {
    M5.Display.setRotation((M5.Display.getRotation() + 1) & 3);
  }
  M5.Display.setAutoDisplay(false);
  M5.Display.setEpdMode(lgfx::epd_quality);
  const int w = M5.Display.width();
  const int h = M5.Display.height();

  // Force one visible full-cycle refresh first so stale post-flash ghosting is cleared.
  M5.Display.fillScreen(0x000000);
  M5.Display.display(0, 0, w, h);
  waitDisplayWithTimeout();
  M5.Display.fillScreen(0xFFFFFF);
  M5.Display.display(0, 0, w, h);
  waitDisplayWithTimeout();

  const int boxW = (w * 3) / 4;
  const int boxH = h / 3;
  const int boxX = (w - boxW) / 2;
  const int boxY = (h - boxH) / 2;

  M5.Display.drawRoundRect(boxX, boxY, boxW, boxH, 12, 0x000000);
  M5.Display.setTextColor(0x000000, 0xFFFFFF);
  M5.Display.setTextSize(2);
  M5.Display.drawString("OmniPaper", boxX + 24, boxY + 32);
  M5.Display.setTextSize(1);
  M5.Display.drawString("Booting...", boxX + 24, boxY + 76);

  M5.Display.display(0, 0, w, h);
  waitDisplayWithTimeout();
}
#endif

void setup() {
#ifdef PLATFORM_M5PAPER
  // Bring up serial first so boot/display init diagnostics are visible.
  Serial.begin(115200);
  unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart) < 1000) {
    delay(10);
  }
#endif

  t1 = millis();

  gpio.begin();

#ifndef PLATFORM_M5PAPER
  // Only start serial if USB connected
  if (gpio.isUsbConnected()) {
    Serial.begin(115200);
    // Wait up to 3 seconds for Serial to be ready to catch early logs
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 3000) {
      delay(10);
    }
  }
#endif

#ifdef PLATFORM_M5PAPER
  if (Serial) {
    Serial.printf("[%lu] [M5P] M5Unified board=%d, Display board=%d\n", millis(), static_cast<int>(M5.getBoard()),
                  static_cast<int>(M5.Display.getBoard()));
  }
#endif

  // SD Card Initialization
  // We need 6 open files concurrently when parsing a new chapter
  if (!SdMan.begin()) {
    Serial.printf("[%lu] [   ] SD card initialization failed\n", millis());
    setupDisplayAndFonts();
    exitActivity();
    enterNewActivity(new FullScreenMessageActivity(renderer, mappedInputManager, "SD card error", EpdFontFamily::BOLD));
    return;
  }

  SETTINGS.loadFromFile();
  KOREADER_STORE.loadFromFile();

  switch (gpio.getWakeupReason()) {
    case HalGPIO::WakeupReason::PowerButton:
      // For normal wakeups, verify power button press duration
      Serial.printf("[%lu] [   ] Verifying power button press duration\n", millis());
      verifyPowerButtonDuration();
      break;
    case HalGPIO::WakeupReason::AfterUSBPower:
      // If USB power caused a cold boot, go back to sleep
      Serial.printf("[%lu] [   ] Wakeup reason: After USB Power\n", millis());
      gpio.startDeepSleep();
      break;
    case HalGPIO::WakeupReason::AfterFlash:
      // After flashing, just proceed to boot
    case HalGPIO::WakeupReason::Other:
    default:
      break;
  }

  // First serial output only here to avoid timing inconsistencies for power button press duration verification
  Serial.printf("[%lu] [   ] Starting OmniPaper version " CROSSPOINT_VERSION "\n", millis());

  setupDisplayAndFonts();
#ifdef PLATFORM_M5PAPER
  showBootSplash();
#endif

  exitActivity();
  enterNewActivity(new BootActivity(renderer, mappedInputManager));

  APP_STATE.loadFromFile();
  RECENT_BOOKS.loadFromFile();

#ifdef PLATFORM_M5PAPER
  pendingReaderPath = APP_STATE.openEpubPath;
  if (!APP_STATE.openEpubPath.empty()) {
    APP_STATE.openEpubPath = "";
    APP_STATE.saveToFile();
  }
  onGoLauncher();
#else
  if (APP_STATE.openEpubPath.empty()) {
    onGoHome();
  } else {
    // Clear app state to avoid getting into a boot loop if the epub doesn't load
    const auto path = APP_STATE.openEpubPath;
    APP_STATE.openEpubPath = "";
    APP_STATE.saveToFile();
    onGoToReader(path, MyLibraryActivity::Tab::Recent);
  }
#endif

  // Ensure we're not still holding the power button before leaving setup
  waitForPowerRelease();
}

void loop() {
  static unsigned long maxLoopDuration = 0;
  const unsigned long loopStartTime = millis();
  static unsigned long lastMemPrint = 0;

  gpio.update();

#ifdef PLATFORM_M5PAPER
  updateIdleHotspotWebUi();
  processPendingWebCommand();
#endif

  if (Serial && millis() - lastMemPrint >= 10000) {
    Serial.printf("[%lu] [MEM] Free: %d bytes, Total: %d bytes, Min Free: %d bytes\n", millis(), ESP.getFreeHeap(),
                  ESP.getHeapSize(), ESP.getMinFreeHeap());
    lastMemPrint = millis();
  }

  // Check for any user activity (button press or release) or active background work
  static unsigned long lastActivityTime = millis();
  if (gpio.wasAnyPressed() || gpio.wasAnyReleased() || (currentActivity && currentActivity->preventAutoSleep())
#ifdef PLATFORM_M5PAPER
      || idleHotspotShouldPreventSleep()
#endif
  ) {
    lastActivityTime = millis();  // Reset inactivity timer
  }

  const unsigned long sleepTimeoutMs = SETTINGS.getSleepTimeoutMs();
  if (millis() - lastActivityTime >= sleepTimeoutMs) {
    Serial.printf("[%lu] [SLP] Auto-sleep triggered after %lu ms of inactivity\n", millis(), sleepTimeoutMs);
    enterDeepSleep();
    // This should never be hit as `enterDeepSleep` calls esp_deep_sleep_start
    return;
  }

  if (gpio.isPressed(HalGPIO::BTN_POWER) && gpio.getHeldTime() > SETTINGS.getPowerButtonDuration()) {
    enterDeepSleep();
    // This should never be hit as `enterDeepSleep` calls esp_deep_sleep_start
    return;
  }

  const unsigned long activityStartTime = millis();
  if (currentActivity) {
    currentActivity->loop();
  }
  const unsigned long activityDuration = millis() - activityStartTime;

  const unsigned long loopDuration = millis() - loopStartTime;
  if (loopDuration > maxLoopDuration) {
    maxLoopDuration = loopDuration;
    if (maxLoopDuration > 50) {
      Serial.printf("[%lu] [LOOP] New max loop duration: %lu ms (activity: %lu ms)\n", millis(), maxLoopDuration,
                    activityDuration);
    }
  }

  // Add delay at the end of the loop to prevent tight spinning
  // When an activity requests skip loop delay (e.g., webserver running), use yield() for faster response
  // Otherwise, use longer delay to save power
  if ((currentActivity && currentActivity->skipLoopDelay())
#ifdef PLATFORM_M5PAPER
      || (idleWebServer && idleWebServer->isRunning())
#endif
  ) {
    yield();  // Give FreeRTOS a chance to run tasks, but return immediately
  } else {
    delay(10);  // Normal delay when no activity requires fast response
  }
}
