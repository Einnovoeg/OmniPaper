#pragma once

#include <functional>
#include <vector>

#include "../Activity.h"
#include "LauncherActions.h"

class LauncherActivity final : public Activity {
 public:
  using ActionCallback = std::function<void(LauncherAction)>;

  struct MenuItem {
    LauncherItemId id;
    const char* label;
    bool hasSubmenu;
  };

  struct SubmenuItem {
    const char* label;
    LauncherAction action;
  };

  LauncherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                   const ActionCallback& onAction);

  void onEnter() override;
  void loop() override;

 private:
  enum class MenuState { Main, Sensors, Network, Games, Images, Tools, Settings };

  ActionCallback onAction;
  MenuState menuState = MenuState::Main;

  std::vector<MenuItem> visibleItems;
  int selectionIndex = 0;
  int submenuSelection = 0;
  bool needsRender = true;
  unsigned long lastOverlayRefreshMs = 0;

  void rebuildVisibleItems();
  void render();
  void renderMainMenu();
  void renderSubmenu(const char* title, const std::vector<SubmenuItem>& items);
  void handleInput();
  void handleMainInput();
  void handleSubmenuInput(const std::vector<SubmenuItem>& items);

  void drawCircle(int cx, int cy, int radius, bool fill) const;
  void drawIconSymbol(int cx, int cy, LauncherItemId id, bool selected) const;
  void drawLabel(int cx, int cy, const char* text, bool selected) const;
};
