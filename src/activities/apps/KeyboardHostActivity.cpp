#include "KeyboardHostActivity.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#define US_KEYBOARD
#include <HIDKeyboardTypes.h>

#include <algorithm>
#include <cctype>
#include <memory>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"

namespace {
constexpr const char* kDeviceName = "OmniPaper KB";

// Keyboard mode workflow is adapted for this codebase from:
// https://github.com/robo8080/M5Paper_Keyboard
// https://github.com/m5stack/M5Paper_FactoryTest
// BLE HID transport here is implemented directly using Arduino-ESP32 BLE APIs.

// Standard HID keyboard report descriptor.
constexpr uint8_t kHidReportDescriptor[] = {
    0x05, 0x01,  // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,  // USAGE (Keyboard)
    0xA1, 0x01,  // COLLECTION (Application)
    0x85, 0x01,  //   REPORT_ID (1)
    0x05, 0x07,  //   USAGE_PAGE (Keyboard)
    0x19, 0xE0,  //   USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xE7,  //   USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,  //   LOGICAL_MINIMUM (0)
    0x25, 0x01,  //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,  //   REPORT_SIZE (1)
    0x95, 0x08,  //   REPORT_COUNT (8)
    0x81, 0x02,  //   INPUT (Data,Var,Abs)
    0x95, 0x01,  //   REPORT_COUNT (1)
    0x75, 0x08,  //   REPORT_SIZE (8)
    0x81, 0x01,  //   INPUT (Cnst,Var,Abs)
    0x95, 0x05,  //   REPORT_COUNT (5)
    0x75, 0x01,  //   REPORT_SIZE (1)
    0x05, 0x08,  //   USAGE_PAGE (LEDs)
    0x19, 0x01,  //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,  //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,  //   OUTPUT (Data,Var,Abs)
    0x95, 0x01,  //   REPORT_COUNT (1)
    0x75, 0x03,  //   REPORT_SIZE (3)
    0x91, 0x01,  //   OUTPUT (Cnst,Var,Abs)
    0x95, 0x06,  //   REPORT_COUNT (6)
    0x75, 0x08,  //   REPORT_SIZE (8)
    0x15, 0x00,  //   LOGICAL_MINIMUM (0)
    0x25, 0x65,  //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,  //   USAGE_PAGE (Keyboard)
    0x19, 0x00,  //   USAGE_MINIMUM (Reserved)
    0x29, 0x65,  //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,  //   INPUT (Data,Ary,Abs)
    0xC0         // END_COLLECTION
};

class BleKeyboardCallbacks : public BLEServerCallbacks {
 public:
  explicit BleKeyboardCallbacks(bool& connectedFlag) : connected(connectedFlag) {}

  void onConnect(BLEServer* /*server*/) override { connected = true; }

  void onDisconnect(BLEServer* server) override {
    connected = false;
    if (server != nullptr && server->getAdvertising() != nullptr) {
      server->getAdvertising()->start();
    }
  }

 private:
  bool& connected;
};

class BleHidKeyboardEngine {
 public:
  bool begin(const char* deviceName) {
    if (started) {
      return true;
    }

    BLEDevice::init(deviceName != nullptr ? deviceName : kDeviceName);

    server = BLEDevice::createServer();
    if (server == nullptr) {
      return false;
    }

    callbacks.reset(new BleKeyboardCallbacks(connected));
    server->setCallbacks(callbacks.get());

    hidDevice.reset(new BLEHIDDevice(server));
    if (!hidDevice) {
      return false;
    }

    inputReport = hidDevice->inputReport(1);
    outputReport = hidDevice->outputReport(1);

    // Keep report characteristic subscriptions enabled.
    if (inputReport != nullptr) {
      inputReport->addDescriptor(new BLE2902());
    }
    if (outputReport != nullptr) {
      outputReport->addDescriptor(new BLE2902());
    }

    hidDevice->manufacturer()->setValue("M5Paper");
    hidDevice->pnp(0x02, 0x16C0, 0x27DB, 0x0110);
    hidDevice->hidInfo(0x00, 0x01);

    hidDevice->reportMap(const_cast<uint8_t*>(kHidReportDescriptor), sizeof(kHidReportDescriptor));
    hidDevice->startServices();
    hidDevice->setBatteryLevel(95);

    // No input/output pairing requirement to keep host pairing simple.
    security.reset(new BLESecurity());
    security->setAuthenticationMode(ESP_LE_AUTH_BOND);
    security->setCapability(ESP_IO_CAP_NONE);
    security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    advertising = server->getAdvertising();
    if (advertising == nullptr) {
      return false;
    }

    advertising->setAppearance(HID_KEYBOARD);
    advertising->addServiceUUID(hidDevice->hidService()->getUUID());
    advertising->start();

    started = true;
    connected = false;
    return true;
  }

  void end() {
    if (!started) {
      return;
    }

    if (advertising != nullptr) {
      advertising->stop();
      advertising = nullptr;
    }

    inputReport = nullptr;
    outputReport = nullptr;
    hidDevice.reset();
    callbacks.reset();
    security.reset();
    server = nullptr;

    BLEDevice::deinit(true);

    started = false;
    connected = false;
  }

  bool isStarted() const { return started; }

  bool isConnected() const { return connected; }

  bool sendText(const std::string& text) {
    if (!started || !connected || inputReport == nullptr) {
      return false;
    }

    for (char c : text) {
      if (c == '\r') {
        continue;
      }
      uint8_t modifiers = 0;
      uint8_t usage = 0;
      if (!asciiToHid(c, modifiers, usage)) {
        continue;
      }
      sendReport(modifiers, usage);
    }
    return true;
  }

  bool sendCtrlShortcut(char letter) {
    if (!started || !connected || inputReport == nullptr) {
      return false;
    }

    if (letter >= 'A' && letter <= 'Z') {
      letter = static_cast<char>(letter - 'A' + 'a');
    }

    uint8_t modifiers = 0;
    uint8_t usage = 0;
    if (!asciiToHid(letter, modifiers, usage) || usage == 0) {
      return false;
    }

    sendReport(static_cast<uint8_t>(KEY_CTRL), usage);
    return true;
  }

  bool sendEnter() {
    if (!started || !connected || inputReport == nullptr) {
      return false;
    }

    sendReport(0, 0x28);  // Enter.
    return true;
  }

 private:
  bool started = false;
  bool connected = false;

  BLEServer* server = nullptr;
  BLEAdvertising* advertising = nullptr;
  std::unique_ptr<BLEHIDDevice> hidDevice;
  BLECharacteristic* inputReport = nullptr;
  BLECharacteristic* outputReport = nullptr;
  std::unique_ptr<BleKeyboardCallbacks> callbacks;
  std::unique_ptr<BLESecurity> security;

  void sendReport(uint8_t modifiers, uint8_t usage) {
    uint8_t report[8] = {modifiers, 0x00, usage, 0x00, 0x00, 0x00, 0x00, 0x00};
    inputReport->setValue(report, sizeof(report));
    inputReport->notify();
    delay(8);

    uint8_t release[8] = {0};
    inputReport->setValue(release, sizeof(release));
    inputReport->notify();
    delay(8);
  }

  bool asciiToHid(char c, uint8_t& modifiers, uint8_t& usage) const {
    const uint8_t ascii = static_cast<uint8_t>(c);
    if (ascii >= KEYMAP_SIZE) {
      return false;
    }

    const KEYMAP& map = keymap[ascii];
    modifiers = map.modifier;
    usage = map.usage;
    return usage != 0;
  }
};

BleHidKeyboardEngine gBleKeyboard;

std::string maskPassword(const std::string& input) {
  if (input.empty()) {
    return "(empty)";
  }
  return std::string(input.size(), '*');
}

std::string trimForUi(const std::string& value, size_t maxLen) {
  if (value.size() <= maxLen) {
    return value;
  }
  if (maxLen < 4) {
    return value.substr(0, maxLen);
  }
  return value.substr(0, maxLen - 3) + "...";
}
}  // namespace

KeyboardHostActivity::KeyboardHostActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           const std::function<void()>& onExit)
    : ActivityWithSubactivity("HostKeyboard", renderer, mappedInput), onExitCb(onExit) {}

void KeyboardHostActivity::onEnter() {
  ActivityWithSubactivity::onEnter();

  selectionIndex = 0;
  statusMessage.clear();
  statusError = false;
  statusExpiresAtMs = 0;

  transport = Transport::BluetoothHid;
  beginBleKeyboard();
  needsRender = true;
}

void KeyboardHostActivity::onExit() {
  ActivityWithSubactivity::onExit();
  stopBleKeyboard();
}

void KeyboardHostActivity::beginBleKeyboard() {
  if (gBleKeyboard.isStarted()) {
    return;
  }

  if (!gBleKeyboard.begin(kDeviceName)) {
    setStatus("BLE keyboard init failed", true, 4000);
  } else {
    setStatus("BLE advertising: pair as OmniPaper KB", false, 4000);
  }
}

void KeyboardHostActivity::stopBleKeyboard() {
  if (gBleKeyboard.isStarted()) {
    gBleKeyboard.end();
  }
}

bool KeyboardHostActivity::isBleConnected() const { return gBleKeyboard.isConnected(); }

void KeyboardHostActivity::launchCompose() {
  const char* title = transport == Transport::BluetoothHid ? "Type text for BLE host" : "Type text for USB serial";

  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, title, "", 24, 160, false,
      [this](const std::string& text) {
        exitActivity();
        if (text.empty()) {
          setStatus("Nothing sent", true, 2000);
        } else {
          sendTextToHost(text);
        }
        needsRender = true;
      },
      [this]() {
        exitActivity();
        setStatus("Cancelled", false, 1200);
        needsRender = true;
      }));
}

bool KeyboardHostActivity::sendTextToHost(const std::string& text) {
  if (transport == Transport::BluetoothHid) {
    if (!gBleKeyboard.isStarted()) {
      beginBleKeyboard();
    }
    if (!gBleKeyboard.isConnected()) {
      setStatus("BLE host not connected", true, 3000);
      return false;
    }
    if (gBleKeyboard.sendText(text)) {
      setStatus("Sent over BLE", false, 2000);
      return true;
    }
    setStatus("BLE send failed", true, 3000);
    return false;
  }

  if (!Serial) {
    Serial.begin(115200);
    delay(40);
  }

  if (!Serial) {
    setStatus("USB serial unavailable", true, 3000);
    return false;
  }

  Serial.print(text.c_str());
  setStatus("Sent to USB serial", false, 2000);
  return true;
}

bool KeyboardHostActivity::sendCtrlShortcut(char letter) {
  if (transport != Transport::BluetoothHid) {
    setStatus("Ctrl shortcuts require BLE mode", true, 2600);
    return false;
  }
  if (!gBleKeyboard.isConnected()) {
    setStatus("BLE host not connected", true, 3000);
    return false;
  }
  if (!gBleKeyboard.sendCtrlShortcut(letter)) {
    setStatus("Failed to send shortcut", true, 2600);
    return false;
  }
  std::string msg = "Sent Ctrl+";
  msg.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(letter))));
  setStatus(msg, false, 2000);
  return true;
}

void KeyboardHostActivity::sendEnterKey() {
  if (transport == Transport::BluetoothHid) {
    if (!gBleKeyboard.isConnected()) {
      setStatus("BLE host not connected", true, 3000);
      return;
    }
    if (!gBleKeyboard.sendEnter()) {
      setStatus("Failed to send Enter", true, 3000);
      return;
    }
    setStatus("Sent Enter over BLE", false, 2000);
    return;
  }

  if (!Serial) {
    Serial.begin(115200);
    delay(40);
  }
  if (!Serial) {
    setStatus("USB serial unavailable", true, 3000);
    return;
  }
  Serial.print("\n");
  setStatus("Sent newline to USB serial", false, 2000);
}

void KeyboardHostActivity::setStatus(const std::string& text, const bool isError, const unsigned long timeoutMs) {
  statusMessage = text;
  statusError = isError;
  statusExpiresAtMs = timeoutMs == 0 ? 0 : millis() + timeoutMs;
  needsRender = true;
}

void KeyboardHostActivity::clearExpiredStatus() {
  if (!statusMessage.empty() && statusExpiresAtMs != 0 && millis() >= statusExpiresAtMs) {
    statusMessage.clear();
    statusError = false;
    statusExpiresAtMs = 0;
    needsRender = true;
  }
}

void KeyboardHostActivity::loop() {
  if (subActivity) {
    ActivityWithSubactivity::loop();
    return;
  }

  clearExpiredStatus();

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExitCb) {
      onExitCb();
    }
    return;
  }

  constexpr int kMenuCount = 5;
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selectionIndex = (selectionIndex - 1 + kMenuCount) % kMenuCount;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selectionIndex = (selectionIndex + 1) % kMenuCount;
    needsRender = true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    switch (selectionIndex) {
      case 0:
        transport = transport == Transport::BluetoothHid ? Transport::UsbSerial : Transport::BluetoothHid;
        if (transport == Transport::BluetoothHid) {
          beginBleKeyboard();
        } else {
          stopBleKeyboard();
        }
        setStatus(transport == Transport::BluetoothHid ? "Mode: BLE HID" : "Mode: USB serial", false, 1800);
        break;
      case 1:
        launchCompose();
        break;
      case 2:
        sendEnterKey();
        break;
      case 3:
        sendCtrlShortcut('c');
        break;
      case 4:
        sendCtrlShortcut('l');
        break;
      default:
        break;
    }
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void KeyboardHostActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  renderer.drawCenteredText(UI_12_FONT_ID, 16, "Host Keyboard");

  const char* modeLabel = transport == Transport::BluetoothHid ? "Bluetooth HID" : "USB Serial";
  renderer.drawText(UI_10_FONT_ID, 30, 46, "Transport:");
  renderer.drawText(UI_10_FONT_ID, 150, 46, modeLabel);

  if (transport == Transport::BluetoothHid) {
    renderer.drawText(UI_10_FONT_ID, 30, 68, "BLE link:");
    renderer.drawText(UI_10_FONT_ID, 150, 68, isBleConnected() ? "Connected" : "Advertising");
    renderer.drawText(UI_10_FONT_ID, 30, 90, "Pairing name:");
    renderer.drawText(UI_10_FONT_ID, 150, 90, trimForUi(kDeviceName, 24).c_str());
  } else {
    renderer.drawText(UI_10_FONT_ID, 30, 68, "USB link:");
    renderer.drawText(UI_10_FONT_ID, 150, 68, "Serial @ 115200 baud");
    renderer.drawText(UI_10_FONT_ID, 30, 90, "Host setup:");
    renderer.drawText(UI_10_FONT_ID, 150, 90, "Open a serial terminal");
  }

  const char* menuItems[] = {
      "Toggle Transport", "Compose + Send", "Send Enter", "Send Ctrl+C", "Send Ctrl+L",
  };

  int y = 134;
  for (int i = 0; i < 5; i++) {
    const bool selected = i == selectionIndex;
    if (selected) {
      renderer.fillRect(24, y - 6, renderer.getScreenWidth() - 48, 24, true);
      renderer.drawText(UI_10_FONT_ID, 36, y, menuItems[i], false);
    } else {
      renderer.drawText(UI_10_FONT_ID, 36, y, menuItems[i]);
    }
    y += 26;
  }

  if (!statusMessage.empty()) {
    const int statusY = renderer.getScreenHeight() - 52;
    if (statusError) {
      renderer.drawCenteredText(UI_10_FONT_ID, statusY, ("Error: " + trimForUi(statusMessage, 60)).c_str());
    } else {
      renderer.drawCenteredText(UI_10_FONT_ID, statusY, trimForUi(statusMessage, 66).c_str());
    }
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                            "Up/Down: Select   Confirm: Action   Back: Menu");
  renderer.displayBuffer();
}
