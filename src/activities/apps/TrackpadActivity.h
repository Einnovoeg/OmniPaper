#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "activities/Activity.h"

class TrackpadActivity final : public Activity {
 public:
  TrackpadActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  enum class GestureMode : uint8_t {
    None = 0,
    Swipe = 1,
    Tap = 2,
    Drag = 3,
    DoubleTap = 4,
  };

  std::function<void()> onExitCb;

  bool needsRender = true;
  bool touchDown = false;
  bool dragging = false;
  bool lastBleConnected = false;
  int lastFingerCount = 1;
  int posX = 0;
  int posY = 0;

  int selectionIndex = 0;
  // Stores touch begin/end event timestamps for gesture classification.
  int eventIndex = 0;
  unsigned long events[4] = {0, 0, 0, 0};

  std::string statusMessage;
  bool statusError = false;
  unsigned long statusExpiresAtMs = 0;

  void beginBleMouse();
  void stopBleMouse();
  bool isBleConnected() const;

  void pushTouchEvent(bool down);
  GestureMode judgeMode();
  void handleTouch();

  void sendClick(bool rightButton);
  void sendScrollStep(int step);

  void setStatus(const std::string& text, bool isError = false, unsigned long timeoutMs = 2400);
  void clearExpiredStatus();
  void render();
};
