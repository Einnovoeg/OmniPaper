#pragma once

#include <cstdint>

enum class LauncherItemId : uint8_t {
  Reader = 0,
  Dashboard,
  Sensors,
  Weather,
  Network,
  Games,
  Images,
  Tools,
  Settings,
  Calculator,
  Count
};

enum class LauncherAction : uint8_t {
  None = 0,
  Reader,
  Dashboard,
  SensorsBuiltIn,
  SensorsExternal,
  SensorsI2CTools,
  SensorsUv,
  SensorsGpio,
  SensorsUvLogs,
  SensorsUartRx,
  SensorsEepromDump,
  Weather,
  NetworkWifiScan,
  NetworkWifiStatus,
  NetworkWifiTests,
  NetworkWifiChannels,
  NetworkBleScan,
  NetworkWebPortal,
  NetworkKeyboardHost,
  NetworkTrackpad,
  NetworkSshClient,
  GamePoodle,
  GameSudoku,
  ImagesViewer,
  ImagesDraw,
  ToolsNotes,
  ToolsFileManager,
  ToolsTime,
  ToolsSleepTimer,
  ToolsOtaUpdate,
  SettingsWifi,
  SettingsHardwareTest,
  Calculator
};
