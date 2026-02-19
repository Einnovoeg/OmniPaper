#include "SshClientActivity.h"

#include <WiFi.h>
#include <libssh_esp32.h>

#include <libssh/libssh.h>

#include <GfxRenderer.h>
#include <SDCardManager.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"

namespace {
constexpr const char* kProfileDir = "/ssh";
constexpr const char* kProfileFile = "/ssh/session.cfg";
constexpr size_t kOutputLimit = 1800;

// SSH session/profile workflow adapted from:
// https://github.com/SUB0PT1MAL/M5Cardputer_Interactive_SSH_Client
// Transport layer uses vendored LibSSH-ESP32:
// https://github.com/ewpa/LibSSH-ESP32

std::string trimForUi(const std::string& value, size_t maxLen) {
  if (value.size() <= maxLen) {
    return value;
  }
  if (maxLen <= 3) {
    return value.substr(0, maxLen);
  }
  return value.substr(0, maxLen - 3) + "...";
}

std::string maskedPassword(const std::string& value) {
  if (value.empty()) {
    return "(empty)";
  }
  return std::string(value.size(), '*');
}

std::vector<std::string> splitLines(const String& data) {
  std::vector<std::string> lines;
  std::string current;
  for (size_t i = 0; i < data.length(); i++) {
    const char c = data[i];
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      lines.push_back(current);
      current.clear();
      continue;
    }
    current.push_back(c);
  }
  lines.push_back(current);
  return lines;
}

void drawWrappedText(GfxRenderer& renderer, int fontId, int x, int y, int maxWidth, int maxLines,
                     const std::string& text) {
  const int lineHeight = renderer.getLineHeight(fontId);
  int lineY = y;
  int drawnLines = 0;

  std::string line;
  std::string word;

  auto flushLine = [&]() {
    if (drawnLines >= maxLines) {
      return;
    }
    renderer.drawText(fontId, x, lineY, line.c_str());
    line.clear();
    lineY += lineHeight;
    drawnLines++;
  };

  auto pushWord = [&]() {
    if (word.empty()) {
      return;
    }

    std::string candidate = line.empty() ? word : line + " " + word;
    if (renderer.getTextWidth(fontId, candidate.c_str()) <= maxWidth) {
      line = candidate;
    } else {
      if (!line.empty()) {
        flushLine();
      }
      line = word;
    }
    word.clear();
  };

  for (char c : text) {
    if (drawnLines >= maxLines) {
      break;
    }

    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      pushWord();
      if (!line.empty()) {
        flushLine();
      } else {
        lineY += lineHeight;
        drawnLines++;
      }
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(c))) {
      pushWord();
      continue;
    }
    word.push_back(c);
  }

  pushWord();
  if (!line.empty() && drawnLines < maxLines) {
    renderer.drawText(fontId, x, lineY, line.c_str());
  }
}
}  // namespace

SshClientActivity::SshClientActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                     const std::function<void()>& onExit)
    : ActivityWithSubactivity("SshClient", renderer, mappedInput), onExitCb(onExit) {}

void SshClientActivity::onEnter() {
  ActivityWithSubactivity::onEnter();

  profile.host = "";
  profile.user = "";
  profile.password = "";
  profile.command = "uname -a";
  profile.port = 22;

  selectionIndex = 0;
  lastOutput.clear();
  statusMessage.clear();
  statusError = false;
  statusExpiresAtMs = 0;

  loadProfile();
  needsRender = true;
}

void SshClientActivity::launchWifiSelection() {
  enterNewActivity(new WifiSelectionActivity(renderer, mappedInput,
                                             [this](bool connected) {
                                               exitActivity();
                                               setStatus(connected ? "WiFi connected" : "WiFi setup cancelled",
                                                         !connected, 2200);
                                               needsRender = true;
                                             }));
}

void SshClientActivity::editTextField(const char* title, std::string* field, const size_t maxLen,
                                      const bool passwordField) {
  if (field == nullptr) {
    return;
  }
  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, title, *field, 22, maxLen, passwordField,
      [this, field](const std::string& value) {
        *field = value;
        exitActivity();
        setStatus("Updated", false, 1200);
        needsRender = true;
      },
      [this]() {
        exitActivity();
        needsRender = true;
      }));
}

void SshClientActivity::editPortField() {
  std::string portText = std::to_string(profile.port);
  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, "SSH Port", portText, 22, 5, false,
      [this](const std::string& value) {
        const int parsed = atoi(value.c_str());
        if (parsed <= 0 || parsed > 65535) {
          setStatus("Invalid port", true, 2500);
        } else {
          profile.port = static_cast<uint16_t>(parsed);
          setStatus("Port updated", false, 1200);
        }
        exitActivity();
        needsRender = true;
      },
      [this]() {
        exitActivity();
        needsRender = true;
      }));
}

void SshClientActivity::loadProfile() {
  if (!SdMan.ready()) {
    setStatus("SD card not ready", true, 3000);
    return;
  }

  const String data = SdMan.readFile(kProfileFile);
  if (data.isEmpty()) {
    return;
  }

  const auto lines = splitLines(data);
  if (lines.size() >= 1) {
    profile.host = lines[0];
  }
  if (lines.size() >= 2) {
    const int parsedPort = atoi(lines[1].c_str());
    if (parsedPort > 0 && parsedPort <= 65535) {
      profile.port = static_cast<uint16_t>(parsedPort);
    }
  }
  if (lines.size() >= 3) {
    profile.user = lines[2];
  }
  if (lines.size() >= 4) {
    profile.password = lines[3];
  }
  if (lines.size() >= 5) {
    profile.command = lines[4];
  }

  setStatus("Profile loaded", false, 1200);
}

void SshClientActivity::saveProfile() {
  if (!SdMan.ready()) {
    setStatus("SD card not ready", true, 3000);
    return;
  }

  SdMan.ensureDirectoryExists(kProfileDir);

  String data;
  data += profile.host.c_str();
  data += "\n";
  data += String(profile.port);
  data += "\n";
  data += profile.user.c_str();
  data += "\n";
  data += profile.password.c_str();
  data += "\n";
  data += profile.command.c_str();
  data += "\n";

  if (!SdMan.writeFile(kProfileFile, data)) {
    setStatus("Save failed", true, 2600);
    return;
  }

  setStatus("Profile saved", false, 1500);
}

void SshClientActivity::appendOutput(const char* chunk, const size_t len) {
  if (chunk == nullptr || len == 0) {
    return;
  }

  for (size_t i = 0; i < len; i++) {
    if (chunk[i] == '\r') {
      continue;
    }
    lastOutput.push_back(chunk[i]);
  }

  if (lastOutput.size() > kOutputLimit) {
    const size_t toErase = lastOutput.size() - kOutputLimit;
    lastOutput.erase(0, toErase);
  }
}

bool SshClientActivity::runCommand() {
  if (WiFi.status() != WL_CONNECTED) {
    setStatus("Connect WiFi first", true, 2400);
    return false;
  }

  if (profile.host.empty() || profile.user.empty() || profile.password.empty()) {
    setStatus("Host/user/password required", true, 3000);
    return false;
  }

  if (profile.command.empty()) {
    setStatus("Set a command first", true, 2200);
    return false;
  }

  if (!libsshInitialized) {
    libssh_begin();
    libsshInitialized = true;
  }

  lastOutput.clear();
  setStatus("Running command...", false, 0);
  needsRender = true;
  render();

  ssh_session session = ssh_new();
  if (session == nullptr) {
    setStatus("SSH session allocation failed", true, 3200);
    return false;
  }

  const int verbosity = SSH_LOG_NOLOG;
  unsigned int port = profile.port;
  long timeoutSeconds = 10;

  ssh_options_set(session, SSH_OPTIONS_HOST, profile.host.c_str());
  ssh_options_set(session, SSH_OPTIONS_USER, profile.user.c_str());
  ssh_options_set(session, SSH_OPTIONS_PORT, &port);
  ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeoutSeconds);
  ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);

  int rc = ssh_connect(session);
  if (rc != SSH_OK) {
    setStatus(std::string("Connect failed: ") + ssh_get_error(session), true, 4200);
    ssh_free(session);
    return false;
  }

  rc = ssh_userauth_password(session, nullptr, profile.password.c_str());
  if (rc != SSH_AUTH_SUCCESS) {
    setStatus(std::string("Auth failed: ") + ssh_get_error(session), true, 4200);
    ssh_disconnect(session);
    ssh_free(session);
    return false;
  }

  ssh_channel channel = ssh_channel_new(session);
  if (channel == nullptr) {
    setStatus("Channel allocation failed", true, 3200);
    ssh_disconnect(session);
    ssh_free(session);
    return false;
  }

  rc = ssh_channel_open_session(channel);
  if (rc != SSH_OK) {
    setStatus(std::string("Channel open failed: ") + ssh_get_error(session), true, 4200);
    ssh_channel_free(channel);
    ssh_disconnect(session);
    ssh_free(session);
    return false;
  }

  rc = ssh_channel_request_exec(channel, profile.command.c_str());
  if (rc != SSH_OK) {
    setStatus(std::string("Exec failed: ") + ssh_get_error(session), true, 4200);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    ssh_disconnect(session);
    ssh_free(session);
    return false;
  }

  char buffer[256];
  while (!ssh_channel_is_closed(channel)) {
    int stdoutRead = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 0);
    if (stdoutRead > 0) {
      appendOutput(buffer, static_cast<size_t>(stdoutRead));
    }

    int stderrRead = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 1);
    if (stderrRead > 0) {
      appendOutput(buffer, static_cast<size_t>(stderrRead));
    }

    if (stdoutRead < 0 || stderrRead < 0) {
      break;
    }

    if (stdoutRead == 0 && stderrRead == 0) {
      if (ssh_channel_is_eof(channel)) {
        break;
      }
      delay(10);
    }
  }

  const int exitCode = ssh_channel_get_exit_status(channel);
  if (lastOutput.empty()) {
    lastOutput = "(no output)";
  }

  ssh_channel_send_eof(channel);
  ssh_channel_close(channel);
  ssh_channel_free(channel);
  ssh_disconnect(session);
  ssh_free(session);

  setStatus("Command finished, exit code " + std::to_string(exitCode), false, 3500);
  needsRender = true;
  return true;
}

void SshClientActivity::setStatus(const std::string& text, const bool isError, const unsigned long timeoutMs) {
  statusMessage = text;
  statusError = isError;
  statusExpiresAtMs = timeoutMs == 0 ? 0 : millis() + timeoutMs;
}

void SshClientActivity::clearExpiredStatus() {
  if (!statusMessage.empty() && statusExpiresAtMs != 0 && millis() >= statusExpiresAtMs) {
    statusMessage.clear();
    statusError = false;
    statusExpiresAtMs = 0;
    needsRender = true;
  }
}

void SshClientActivity::loop() {
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

  constexpr int kMenuCount = 9;
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
        launchWifiSelection();
        break;
      case 1:
        editTextField("SSH Host", &profile.host, 96, false);
        break;
      case 2:
        editPortField();
        break;
      case 3:
        editTextField("SSH User", &profile.user, 48, false);
        break;
      case 4:
        editTextField("SSH Password", &profile.password, 96, true);
        break;
      case 5:
        editTextField("SSH Command", &profile.command, 160, false);
        break;
      case 6:
        runCommand();
        break;
      case 7:
        saveProfile();
        break;
      case 8:
        loadProfile();
        break;
      default:
        break;
    }
    needsRender = true;
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void SshClientActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "SSH Client");

  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  const String wifiLabel = wifiConnected ? ("WiFi: " + WiFi.SSID()) : String("WiFi: not connected");
  renderer.drawText(UI_10_FONT_ID, 24, 40, trimForUi(wifiLabel.c_str(), 42).c_str());

  const int splitX = renderer.getScreenWidth() / 2 - 6;
  renderer.drawLine(splitX, 54, splitX, renderer.getScreenHeight() - 36, true);

  const std::string menuLines[] = {
      std::string("WiFi Connect"),
      std::string("Host: ") + trimForUi(profile.host.empty() ? "(unset)" : profile.host, 24),
      std::string("Port: ") + std::to_string(profile.port),
      std::string("User: ") + trimForUi(profile.user.empty() ? "(unset)" : profile.user, 24),
      std::string("Password: ") + trimForUi(maskedPassword(profile.password), 20),
      std::string("Command: ") + trimForUi(profile.command.empty() ? "(unset)" : profile.command, 21),
      std::string("Run Command"),
      std::string("Save Profile"),
      std::string("Reload Profile"),
  };

  int y = 66;
  for (int i = 0; i < 9; i++) {
    const bool selected = i == selectionIndex;
    if (selected) {
      renderer.fillRect(14, y - 5, splitX - 24, 22, true);
      renderer.drawText(UI_10_FONT_ID, 20, y, menuLines[i].c_str(), false);
    } else {
      renderer.drawText(UI_10_FONT_ID, 20, y, menuLines[i].c_str());
    }
    y += 24;
  }

  renderer.drawText(UI_10_FONT_ID, splitX + 16, 64, "Output:");
  if (lastOutput.empty()) {
    renderer.drawText(UI_10_FONT_ID, splitX + 16, 86, "(none yet)");
  } else {
    const int outputTop = 86;
    const int outputWidth = renderer.getScreenWidth() - splitX - 28;
    const int outputLines = (renderer.getScreenHeight() - outputTop - 64) / renderer.getLineHeight(UI_10_FONT_ID);
    drawWrappedText(renderer, UI_10_FONT_ID, splitX + 16, outputTop, outputWidth, std::max(1, outputLines), lastOutput);
  }

  if (!statusMessage.empty()) {
    const std::string shown = trimForUi(statusMessage, 82);
    if (statusError) {
      renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 48, ("Error: " + shown).c_str());
    } else {
      renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 48, shown.c_str());
    }
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24,
                            "Up/Down: Select   Confirm: Edit/Run   Back: Menu");
  renderer.displayBuffer();
}
