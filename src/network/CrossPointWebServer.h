#pragma once

#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiUdp.h>

#include <memory>
#include <string>
#include <vector>

// Structure to hold file information
struct FileInfo {
  String name;
  size_t size;
  bool isEpub;
  bool isDirectory;
};

class CrossPointWebServer {
 public:
  struct WsUploadStatus {
    bool inProgress = false;
    size_t received = 0;
    size_t total = 0;
    std::string filename;
    std::string lastCompleteName;
    size_t lastCompleteSize = 0;
    unsigned long lastCompleteAt = 0;
  };

  CrossPointWebServer();
  ~CrossPointWebServer();

  // Start the web server (call after WiFi is connected)
  void begin();

  // Stop the web server
  void stop();

  // Call this periodically to handle client requests
  void handleClient();

  // Check if server is running
  bool isRunning() const { return running; }

  void setReaderOpenCallback(std::function<bool(const std::string&)> callback) { readerOpenCallback = std::move(callback); }
  void setAppLaunchCallback(std::function<bool(const std::string&)> callback) { appLaunchCallback = std::move(callback); }

  WsUploadStatus getWsUploadStatus() const;

  // Get the port number
  uint16_t getPort() const { return port; }

 private:
  std::unique_ptr<WebServer> server = nullptr;
  std::unique_ptr<WebSocketsServer> wsServer = nullptr;
  bool running = false;
  bool apMode = false;  // true when running in AP mode, false for STA mode
  uint16_t port = 80;
  uint16_t wsPort = 81;  // WebSocket port
  WiFiUDP udp;
  bool udpActive = false;

  // WebSocket upload state
  void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
  static void wsEventCallback(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

  // File scanning
  void scanFiles(const char* path, const std::function<void(FileInfo)>& callback) const;
  String formatFileSize(size_t bytes) const;
  bool isEpubFile(const String& filename) const;

  // Request handlers
  void handleRoot() const;
  void handleNotFound() const;
  void handleStatus() const;
  void handleAppList() const;
  void handleLaunchApp() const;
  void handleOpenReader() const;
  void handleFileList() const;
  void handleFileListData() const;
  void handleDownload() const;
  void handleUpload() const;
  void handleUploadPost() const;
  void handleCreateFolder() const;
  void handleDelete() const;

  std::function<bool(const std::string&)> readerOpenCallback = nullptr;
  std::function<bool(const std::string&)> appLaunchCallback = nullptr;
};
