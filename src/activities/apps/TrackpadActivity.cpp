#include "TrackpadActivity.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#ifdef PLATFORM_M5PAPER
#include <M5Unified.h>
#endif

#include <algorithm>
#include <cstdint>
#include <memory>

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"

namespace {
constexpr const char* kDeviceName = "OmniPaper Trackpad";

constexpr int kMaxRelative = 127;

constexpr uint8_t kMouseReportDescriptor[] = {
    0x05, 0x01,  // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,  // USAGE (Mouse)
    0xA1, 0x01,  // COLLECTION (Application)
    0x09, 0x01,  //   USAGE (Pointer)
    0xA1, 0x00,  //   COLLECTION (Physical)
    0x85, 0x01,  //     REPORT_ID (1)
    0x05, 0x09,  //     USAGE_PAGE (Button)
    0x19, 0x01,  //     USAGE_MINIMUM (Button 1)
    0x29, 0x03,  //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,  //     LOGICAL_MINIMUM (0)
    0x25, 0x01,  //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,  //     REPORT_COUNT (3)
    0x75, 0x01,  //     REPORT_SIZE (1)
    0x81, 0x02,  //     INPUT (Data,Var,Abs)
    0x95, 0x01,  //     REPORT_COUNT (1)
    0x75, 0x05,  //     REPORT_SIZE (5)
    0x81, 0x01,  //     INPUT (Const,Array,Abs)
    0x05, 0x01,  //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,  //     USAGE (X)
    0x09, 0x31,  //     USAGE (Y)
    0x09, 0x38,  //     USAGE (Wheel)
    0x15, 0x81,  //     LOGICAL_MINIMUM (-127)
    0x25, 0x7F,  //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,  //     REPORT_SIZE (8)
    0x95, 0x03,  //     REPORT_COUNT (3)
    0x81, 0x06,  //     INPUT (Data,Var,Rel)
    0xC0,        //   END_COLLECTION
    0xC0         // END_COLLECTION
};

class BleMouseCallbacks : public BLEServerCallbacks {
 public:
  explicit BleMouseCallbacks(bool& connectedFlag) : connected(connectedFlag) {}

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

class BleHidMouseEngine {
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

    callbacks.reset(new BleMouseCallbacks(connected));
    server->setCallbacks(callbacks.get());

    hidDevice.reset(new BLEHIDDevice(server));
    if (!hidDevice) {
      return false;
    }

    inputReport = hidDevice->inputReport(1);
    if (inputReport != nullptr) {
      inputReport->addDescriptor(new BLE2902());
    }

    hidDevice->manufacturer()->setValue("M5Paper");
    hidDevice->pnp(0x02, 0x16C0, 0x27DC, 0x0110);
    hidDevice->hidInfo(0x00, 0x01);
    hidDevice->reportMap(const_cast<uint8_t*>(kMouseReportDescriptor), sizeof(kMouseReportDescriptor));
    hidDevice->startServices();
    hidDevice->setBatteryLevel(95);

    security.reset(new BLESecurity());
    security->setAuthenticationMode(ESP_LE_AUTH_BOND);
    security->setCapability(ESP_IO_CAP_NONE);
    security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    advertising = server->getAdvertising();
    if (advertising == nullptr) {
      return false;
    }
    advertising->setAppearance(HID_MOUSE);
    advertising->addServiceUUID(hidDevice->hidService()->getUUID());
    advertising->start();

    connected = false;
    buttons = 0;
    started = true;
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
    hidDevice.reset();
    callbacks.reset();
    security.reset();
    server = nullptr;

    BLEDevice::deinit(true);

    connected = false;
    buttons = 0;
    started = false;
  }

  bool isConnected() const { return started && connected; }

  bool move(int dx, int dy, int wheel = 0) {
    if (!isConnected() || inputReport == nullptr) {
      return false;
    }

    dx = std::max(-kMaxRelative, std::min(kMaxRelative, dx));
    dy = std::max(-kMaxRelative, std::min(kMaxRelative, dy));
    wheel = std::max(-kMaxRelative, std::min(kMaxRelative, wheel));

    sendReport(buttons, static_cast<int8_t>(dx), static_cast<int8_t>(dy), static_cast<int8_t>(wheel));
    return true;
  }

  bool click(const uint8_t buttonMask) {
    if (!isConnected()) {
      return false;
    }

    press(buttonMask);
    delay(8);
    release(buttonMask);
    return true;
  }

  bool press(const uint8_t buttonMask) {
    if (!isConnected() || inputReport == nullptr) {
      return false;
    }

    buttons |= buttonMask;
    sendReport(buttons, 0, 0, 0);
    return true;
  }

  bool release(const uint8_t buttonMask) {
    if (!isConnected() || inputReport == nullptr) {
      return false;
    }

    buttons &= ~buttonMask;
    sendReport(buttons, 0, 0, 0);
    return true;
  }

  bool releaseAll() {
    if (!isConnected() || inputReport == nullptr) {
      return false;
    }

    buttons = 0;
    sendReport(0, 0, 0, 0);
    return true;
  }

 private:
  bool started = false;
  bool connected = false;
  uint8_t buttons = 0;

  BLEServer* server = nullptr;
  BLEAdvertising* advertising = nullptr;
  std::unique_ptr<BLEHIDDevice> hidDevice;
  BLECharacteristic* inputReport = nullptr;
  std::unique_ptr<BleMouseCallbacks> callbacks;
  std::unique_ptr<BLESecurity> security;

  void sendReport(uint8_t buttonState, int8_t x, int8_t y, int8_t wheel) {
    uint8_t report[4] = {buttonState, static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint8_t>(wheel)};
    inputReport->setValue(report, sizeof(report));
    inputReport->notify();
    delay(6);
  }
};

BleHidMouseEngine gBleMouse;
}  // namespace

TrackpadActivity::TrackpadActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                   const std::function<void()>& onExit)
    : Activity("Trackpad", renderer, mappedInput), onExitCb(onExit) {}

void TrackpadActivity::onEnter() {
  Activity::onEnter();

  selectionIndex = 0;
  needsRender = true;
  touchDown = false;
  dragging = false;
  eventIndex = 0;
  lastFingerCount = 1;
  statusMessage.clear();
  statusError = false;
  statusExpiresAtMs = 0;

  beginBleMouse();
  lastBleConnected = isBleConnected();
}

void TrackpadActivity::onExit() {
  Activity::onExit();
  stopBleMouse();
}

void TrackpadActivity::beginBleMouse() {
  if (!gBleMouse.begin(kDeviceName)) {
    setStatus("BLE mouse init failed", true, 3800);
    return;
  }
  setStatus("Pair as OmniPaper Trackpad", false, 4200);
}

void TrackpadActivity::stopBleMouse() {
  gBleMouse.end();
}

bool TrackpadActivity::isBleConnected() const { return gBleMouse.isConnected(); }

void TrackpadActivity::pushTouchEvent(bool down) {
  if (!down && eventIndex <= 0) {
    return;
  }
  if (down && eventIndex >= 4) {
    eventIndex = 0;
  }
  events[eventIndex++] = millis();
}

TrackpadActivity::GestureMode TrackpadActivity::judgeMode() {
  if (eventIndex <= 0) {
    return GestureMode::None;
  }

  const unsigned long now = millis();
  const unsigned long last = events[eventIndex - 1];
  if ((now - last) < 150) {
    return GestureMode::None;
  }

  // Gesture timing classification adapted from:
  // https://github.com/hollyhockberry/m5paper-trackpad (MIT), src/main.cpp
  const int clamped = std::max(0, std::min(4, eventIndex));
  eventIndex = 0;
  return static_cast<GestureMode>(clamped);
}

void TrackpadActivity::sendClick(bool rightButton) {
  if (!isBleConnected()) {
    setStatus("BLE host not connected", true, 2200);
    return;
  }
  const uint8_t button = rightButton ? 0x02 : 0x01;
  gBleMouse.click(button);
  setStatus(rightButton ? "Right click" : "Left click", false, 900);
}

void TrackpadActivity::sendScrollStep(const int step) {
  if (!isBleConnected()) {
    setStatus("BLE host not connected", true, 2200);
    return;
  }
  gBleMouse.move(0, 0, step);
  setStatus("Scroll", false, 800);
}

void TrackpadActivity::handleTouch() {
#ifdef PLATFORM_M5PAPER
  const auto detail = M5.Touch.getDetail();
  const bool pressed = detail.isPressed();

  if (!pressed) {
    if (touchDown) {
      touchDown = false;
      pushTouchEvent(false);
      if (dragging) {
        gBleMouse.release(0x01);
        dragging = false;
        setStatus("Drag release", false, 900);
      }
      return;
    }

    if (!dragging) {
      const GestureMode mode = judgeMode();
      if (mode == GestureMode::Tap) {
        sendClick(lastFingerCount >= 2);
      } else if (mode == GestureMode::DoubleTap) {
        if (isBleConnected()) {
          gBleMouse.click(0x01);
          gBleMouse.click(0x01);
          setStatus("Double click", false, 1000);
        }
      } else if (mode == GestureMode::Drag) {
        if (isBleConnected()) {
          gBleMouse.press(0x01);
          dragging = true;
          setStatus("Drag hold", false, 1000);
        }
      }
    }
    return;
  }

  const int fingers = std::max(1, static_cast<int>(M5.Touch.getCount()));
  if (!touchDown) {
    touchDown = true;
    pushTouchEvent(true);
    posX = detail.x;
    posY = detail.y;
    lastFingerCount = fingers;
    return;
  }

  if (!isBleConnected()) {
    posX = detail.x;
    posY = detail.y;
    return;
  }

  const int dx = detail.x - posX;
  const int dy = detail.y - posY;
  posX = detail.x;
  posY = detail.y;
  lastFingerCount = fingers;

  if (dx == 0 && dy == 0) {
    return;
  }

  if (fingers >= 2) {
    const int wheel = std::max(-6, std::min(6, -dy / 12));
    if (wheel != 0) {
      gBleMouse.move(0, 0, wheel);
    }
    return;
  }

  // Invert Y so upward finger movement maps to upward cursor movement.
  const int moveX = dx;
  const int moveY = -dy;
  gBleMouse.move(moveX, moveY, 0);
#endif
}

void TrackpadActivity::setStatus(const std::string& text, const bool isError, const unsigned long timeoutMs) {
  statusMessage = text;
  statusError = isError;
  statusExpiresAtMs = timeoutMs == 0 ? 0 : millis() + timeoutMs;
  needsRender = true;
}

void TrackpadActivity::clearExpiredStatus() {
  if (!statusMessage.empty() && statusExpiresAtMs != 0 && millis() >= statusExpiresAtMs) {
    statusMessage.clear();
    statusError = false;
    statusExpiresAtMs = 0;
    needsRender = true;
  }
}

void TrackpadActivity::loop() {
  clearExpiredStatus();

  const bool connected = isBleConnected();
  if (connected != lastBleConnected) {
    lastBleConnected = connected;
    setStatus(connected ? "BLE connected" : "BLE disconnected", !connected, 2200);
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExitCb) {
      onExitCb();
    }
    return;
  }

  constexpr int kActionCount = 4;
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selectionIndex = (selectionIndex - 1 + kActionCount) % kActionCount;
    needsRender = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selectionIndex = (selectionIndex + 1) % kActionCount;
    needsRender = true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    switch (selectionIndex) {
      case 0:
        sendClick(false);
        break;
      case 1:
        sendClick(true);
        break;
      case 2:
        sendScrollStep(1);
        break;
      case 3:
        sendScrollStep(-1);
        break;
      default:
        break;
    }
  }

  handleTouch();

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void TrackpadActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "Trackpad");
  renderer.drawText(UI_10_FONT_ID, 24, 42, "BLE Device:");
  renderer.drawText(UI_10_FONT_ID, 170, 42, kDeviceName);
  renderer.drawText(UI_10_FONT_ID, 24, 64, "Host Link:");
  renderer.drawText(UI_10_FONT_ID, 170, 64, isBleConnected() ? "Connected" : "Advertising");

  renderer.drawText(UI_10_FONT_ID, 24, 92, "Gestures:");
  renderer.drawText(UI_10_FONT_ID, 170, 92, "1 finger move = cursor");
  renderer.drawText(UI_10_FONT_ID, 170, 112, "2 fingers move = scroll");
  renderer.drawText(UI_10_FONT_ID, 170, 132, "Tap=click, double tap=double click");
  renderer.drawText(UI_10_FONT_ID, 170, 152, "Tap-hold starts drag");

  const char* actions[] = {"Left Click", "Right Click", "Scroll Up", "Scroll Down"};
  int y = 186;
  for (int i = 0; i < 4; i++) {
    const bool selected = i == selectionIndex;
    if (selected) {
      renderer.fillRect(20, y - 5, 220, 22, true);
      renderer.drawText(UI_10_FONT_ID, 28, y, actions[i], false);
    } else {
      renderer.drawText(UI_10_FONT_ID, 28, y, actions[i]);
    }
    y += 24;
  }

  if (!statusMessage.empty()) {
    if (statusError) {
      renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 48,
                                ("Error: " + statusMessage).c_str());
    } else {
      renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 48, statusMessage.c_str());
    }
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                            "Touch: Trackpad   Confirm: Action   Back: Menu");
  renderer.displayBuffer();
}
