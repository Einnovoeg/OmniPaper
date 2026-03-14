#include "NotesActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>

#include "MappedInputManager.h"
#include "PaperS3Ui.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"

namespace {
constexpr const char* kNotesDir = "/notes";
constexpr const char* kNotesFile = "/notes/quick_note.txt";

void drawWrappedText(GfxRenderer& renderer, const int fontId, const int x, const int y, const int maxWidth,
                     const std::string& text, const int maxLines) {
  const int lineHeight = renderer.getLineHeight(fontId);
  int lineY = y;
  int linesDrawn = 0;

  std::string line;
  std::string word;

  auto flushLine = [&]() {
    if (linesDrawn >= maxLines) {
      return;
    }
    if (!line.empty()) {
      renderer.drawText(fontId, x, lineY, line.c_str());
    }
    line.clear();
    lineY += lineHeight;
    linesDrawn++;
  };

  auto pushWord = [&]() {
    if (word.empty()) {
      return;
    }
    std::string candidate = line.empty() ? word : line + " " + word;
    if (renderer.getTextWidth(fontId, candidate.c_str()) <= maxWidth) {
      line = candidate;
    } else {
      flushLine();
      line = word;
    }
    word.clear();
  };

  for (char c : text) {
    if (c == '\n') {
      pushWord();
      flushLine();
      continue;
    }
    if (c == ' ') {
      pushWord();
      continue;
    }
    word.push_back(c);
  }
  pushWord();
  if (!line.empty() && linesDrawn < maxLines) {
    renderer.drawText(fontId, x, lineY, line.c_str());
  }
}
}  // namespace

NotesActivity::NotesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                             const std::function<void()>& onExit)
    : ActivityWithSubactivity("Notes", renderer, mappedInput), onExit(onExit) {}

void NotesActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
  loadNote();
  needsRender = true;
}

void NotesActivity::loop() {
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
      if (onExit) {
        onExit();
      }
      return;
    }
    if (PaperS3Ui::primaryActionRect(renderer).contains(tapX, tapY)) {
      startEdit();
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
    startEdit();
    return;
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void NotesActivity::loadNote() {
  noteText.clear();
  if (!SdMan.ready()) {
    return;
  }
  const String data = SdMan.readFile(kNotesFile);
  if (data.length() > 0) {
    noteText.assign(data.c_str());
  }
}

void NotesActivity::saveNote() {
  if (!SdMan.ready()) {
    return;
  }
  SdMan.ensureDirectoryExists(kNotesDir);
  SdMan.writeFile(kNotesFile, String(noteText.c_str()));
}

void NotesActivity::startEdit() {
  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, "Edit Note", noteText, 12, 0, false,
      [this](const std::string& text) {
        noteText = text;
        saveNote();
        exitActivity();
        needsRender = true;
      },
      [this]() {
        exitActivity();
        needsRender = true;
      }));
}

void NotesActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  PaperS3Ui::drawScreenHeader(renderer, "Notes");
  PaperS3Ui::drawBackButton(renderer);

  if (!SdMan.ready()) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "SD card not ready");
    PaperS3Ui::drawFooter(renderer, "SD card required");
    renderer.displayBuffer();
    return;
  }

  renderer.drawRect(24, 112, 492, 700);
  if (noteText.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "No notes yet");
  } else {
    const int left = 40;
    const int top = 132;
    const int maxWidth = renderer.getScreenWidth() - 80;
    const int maxLines = 26;
    drawWrappedText(renderer, UI_10_FONT_ID, left, top, maxWidth, noteText, maxLines);
  }

  PaperS3Ui::drawPrimaryActionButton(renderer, noteText.empty() ? "Add Note" : "Edit Note");
  PaperS3Ui::drawFooter(renderer, "Tap Edit to open the keyboard");
  renderer.displayBuffer();
}
