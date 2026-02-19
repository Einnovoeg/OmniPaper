# Webserver Endpoints

This document describes all HTTP and WebSocket endpoints available on the OmniPaper webserver.

- [Webserver Endpoints](#webserver-endpoints)
  - [Overview](#overview)
  - [HTTP Endpoints](#http-endpoints)
    - [GET `/` - Home Page](#get----home-page)
    - [GET `/files` - File Browser Page](#get-files---file-browser-page)
    - [GET `/api/status` - Device Status](#get-apistatus---device-status)
    - [GET `/api/apps` - Web Launchable Apps](#get-apiapps---web-launchable-apps)
    - [POST `/api/apps/launch` - Launch App](#post-apiappslaunch---launch-app)
    - [POST `/api/reader/open` - Open EPUB in Reader](#post-apireaderopen---open-epub-in-reader)
    - [GET `/api/files` - List Files](#get-apifiles---list-files)
    - [POST `/upload` - Upload File](#post-upload---upload-file)
    - [POST `/mkdir` - Create Folder](#post-mkdir---create-folder)
    - [POST `/delete` - Delete File or Folder](#post-delete---delete-file-or-folder)
  - [WebSocket Endpoint](#websocket-endpoint)
    - [Port 81 - Fast Binary Upload](#port-81---fast-binary-upload)
  - [Network Modes](#network-modes)
    - [Station Mode (STA)](#station-mode-sta)
    - [Access Point Mode (AP)](#access-point-mode-ap)
  - [Notes](#notes)


## Overview

The OmniPaper exposes a webserver for file management, device monitoring, and web-triggered app launch:

- **HTTP Server**: Port 80
- **WebSocket Server**: Port 81 (for fast binary uploads)

---

## HTTP Endpoints

### GET `/` - Home Page

Serves the home page HTML interface.

**Request:**
```bash
curl http://omnipaper.local/
```

**Response:** HTML page (200 OK)

---

### GET `/files` - File Browser Page

Serves the file browser HTML interface.

**Request:**
```bash
curl http://omnipaper.local/files
```

**Response:** HTML page (200 OK)

---

### GET `/api/status` - Device Status

Returns JSON with device status information.

**Request:**
```bash
curl http://omnipaper.local/api/status
```

**Response (200 OK):**
```json
{
  "version": "1.0.0",
  "ip": "192.168.1.100",
  "mode": "STA",
  "rssi": -45,
  "freeHeap": 123456,
  "uptime": 3600
}
```

| Field      | Type   | Description                                               |
| ---------- | ------ | --------------------------------------------------------- |
| `version`  | string | CrossPoint firmware version                               |
| `ip`       | string | Device IP address                                         |
| `mode`     | string | `"STA"` (connected to WiFi) or `"AP"` (access point mode) |
| `rssi`     | number | WiFi signal strength in dBm (0 in AP mode)                |
| `freeHeap` | number | Free heap memory in bytes                                 |
| `uptime`   | number | Seconds since device boot                                 |
| `appLaunchAvailable` | boolean | `true` when app quick-launch callback is enabled |
| `readerOpenAvailable` | boolean | `true` when reader-open callback is enabled |

---

### GET `/api/apps` - Web Launchable Apps

Returns app IDs exposed in the web quick-launch panel.

**Request:**
```bash
curl http://omnipaper.local/api/apps
```

**Response (200 OK):**
```json
{
  "apps": [
    {"id": "dashboard", "name": "Dashboard"},
    {"id": "weather", "name": "Weather"},
    {"id": "calculator", "name": "Calculator"}
  ],
  "count": 3
}
```

_Example response shown with shortened app list._

Common IDs currently exposed include:
- `dashboard`
- `sensors-built-in`
- `sensors-external`
- `weather`
- `wifi-status`
- `wifi-scan`
- `ble-scan`
- `web-portal`
- `host-keyboard`
- `trackpad`
- `ssh-client`
- `image-viewer`
- `drawing`
- `notes`
- `calculator`
- `poodle`
- `sudoku`
- `file-manager`
- `hardware-test`

---

### POST `/api/apps/launch` - Launch App

Queues an app launch in the launcher.

**Request:**
```bash
curl -X POST -d "id=weather" http://omnipaper.local/api/apps/launch
```

**Form Parameters:**

| Parameter | Required | Description |
| --------- | -------- | ----------- |
| `id`      | Yes      | App ID from `/api/apps` |

**Response (202 Accepted):**
```
Launch queued: weather
```

---

### POST `/api/reader/open` - Open EPUB in Reader

Queues direct reader launch for an EPUB path.

**Request:**
```bash
curl -X POST -d "path=/books/mybook.epub" http://omnipaper.local/api/reader/open
```

**Form Parameters:**

| Parameter | Required | Description |
| --------- | -------- | ----------- |
| `path`    | Yes      | Absolute SD path to a `.epub` file |

**Response (202 Accepted):**
```
Reader open queued: /books/mybook.epub
```

---

### GET `/api/files` - List Files

Returns a JSON array of files and folders in the specified directory.

**Request:**
```bash
# List root directory
curl http://omnipaper.local/api/files

# List specific directory
curl "http://omnipaper.local/api/files?path=/Books"
```

**Query Parameters:**

| Parameter | Required | Default | Description            |
| --------- | -------- | ------- | ---------------------- |
| `path`    | No       | `/`     | Directory path to list |

**Response (200 OK):**
```json
[
  {"name": "MyBook.epub", "size": 1234567, "isDirectory": false, "isEpub": true},
  {"name": "Notes", "size": 0, "isDirectory": true, "isEpub": false},
  {"name": "document.pdf", "size": 54321, "isDirectory": false, "isEpub": false}
]
```

| Field         | Type    | Description                              |
| ------------- | ------- | ---------------------------------------- |
| `name`        | string  | File or folder name                      |
| `size`        | number  | Size in bytes (0 for directories)        |
| `isDirectory` | boolean | `true` if the item is a folder           |
| `isEpub`      | boolean | `true` if the file has `.epub` extension |

**Notes:**
- Hidden files (starting with `.`) are automatically filtered out
- System folders (`System Volume Information`, `XTCache`) are hidden

---

### POST `/upload` - Upload File

Uploads a file to the SD card via multipart form data.

**Request:**
```bash
# Upload to root directory
curl -X POST -F "file=@mybook.epub" http://omnipaper.local/upload

# Upload to specific directory
curl -X POST -F "file=@mybook.epub" "http://omnipaper.local/upload?path=/Books"
```

**Query Parameters:**

| Parameter | Required | Default | Description                     |
| --------- | -------- | ------- | ------------------------------- |
| `path`    | No       | `/`     | Target directory for the upload |

**Response (200 OK):**
```
File uploaded successfully: mybook.epub
```

**Error Responses:**

| Status | Body                                            | Cause                       |
| ------ | ----------------------------------------------- | --------------------------- |
| 400    | `Failed to create file on SD card`              | Cannot create file          |
| 400    | `Failed to write to SD card - disk may be full` | Write error during upload   |
| 400    | `Failed to write final data to SD card`         | Error flushing final buffer |
| 400    | `Upload aborted`                                | Client aborted the upload   |
| 400    | `Unknown error during upload`                   | Unspecified error           |

**Notes:**
- Existing files with the same name will be overwritten
- Uses a 4KB buffer for efficient SD card writes

---

### POST `/mkdir` - Create Folder

Creates a new folder on the SD card.

**Request:**
```bash
curl -X POST -d "name=NewFolder&path=/" http://omnipaper.local/mkdir
```

**Form Parameters:**

| Parameter | Required | Default | Description                  |
| --------- | -------- | ------- | ---------------------------- |
| `name`    | Yes      | -       | Name of the folder to create |
| `path`    | No       | `/`     | Parent directory path        |

**Response (200 OK):**
```
Folder created: NewFolder
```

**Error Responses:**

| Status | Body                          | Cause                         |
| ------ | ----------------------------- | ----------------------------- |
| 400    | `Missing folder name`         | `name` parameter not provided |
| 400    | `Folder name cannot be empty` | Empty folder name             |
| 400    | `Folder already exists`       | Folder with same name exists  |
| 500    | `Failed to create folder`     | SD card error                 |

---

### POST `/delete` - Delete File or Folder

Deletes a file or folder from the SD card.

**Request:**
```bash
# Delete a file
curl -X POST -d "path=/Books/mybook.epub&type=file" http://omnipaper.local/delete

# Delete an empty folder
curl -X POST -d "path=/OldFolder&type=folder" http://omnipaper.local/delete
```

**Form Parameters:**

| Parameter | Required | Default | Description                      |
| --------- | -------- | ------- | -------------------------------- |
| `path`    | Yes      | -       | Path to the item to delete       |
| `type`    | No       | `file`  | Type of item: `file` or `folder` |

**Response (200 OK):**
```
Deleted successfully
```

**Error Responses:**

| Status | Body                                          | Cause                         |
| ------ | --------------------------------------------- | ----------------------------- |
| 400    | `Missing path`                                | `path` parameter not provided |
| 400    | `Cannot delete root directory`                | Attempted to delete `/`       |
| 400    | `Folder is not empty. Delete contents first.` | Non-empty folder              |
| 403    | `Cannot delete system files`                  | Hidden file (starts with `.`) |
| 403    | `Cannot delete protected items`               | Protected system folder       |
| 404    | `Item not found`                              | Path does not exist           |
| 500    | `Failed to delete item`                       | SD card error                 |

**Protected Items:**
- Files/folders starting with `.`
- `System Volume Information`
- `XTCache`

---

## WebSocket Endpoint

### Port 81 - Fast Binary Upload

A WebSocket endpoint for high-speed binary file uploads. More efficient than HTTP multipart for large files.

**Connection:**
```
ws://omnipaper.local:81/
```

**Protocol:**

1. **Client** sends TEXT message: `START:<filename>:<size>:<path>`
2. **Server** responds with TEXT: `READY`
3. **Client** sends BINARY messages with file data chunks
4. **Server** sends TEXT progress updates: `PROGRESS:<received>:<total>`
5. **Server** sends TEXT when complete: `DONE` or `ERROR:<message>`

**Example Session:**

```
Client -> "START:mybook.epub:1234567:/Books"
Server -> "READY"
Client -> [binary chunk 1]
Client -> [binary chunk 2]
Server -> "PROGRESS:65536:1234567"
Client -> [binary chunk 3]
...
Server -> "PROGRESS:1234567:1234567"
Server -> "DONE"
```

**Error Messages:**

| Message                           | Cause                              |
| --------------------------------- | ---------------------------------- |
| `ERROR:Failed to create file`     | Cannot create file on SD card      |
| `ERROR:Invalid START format`      | Malformed START message            |
| `ERROR:No upload in progress`     | Binary data received without START |
| `ERROR:Write failed - disk full?` | SD card write error                |

**Example with `websocat`:**
```bash
# Interactive session
websocat ws://omnipaper.local:81

# Then type:
START:mybook.epub:1234567:/Books
# Wait for READY, then send binary data
```

**Notes:**
- Progress updates are sent every 64KB or at completion
- Disconnection during upload will delete the incomplete file
- Existing files with the same name will be overwritten

---

## Network Modes

The device can operate in two network modes:

### Station Mode (STA)
- Device connects to an existing WiFi network
- IP address assigned by router/DHCP
- `mode` field in `/api/status` returns `"STA"`
- `rssi` field shows signal strength

### Access Point Mode (AP)
- Device creates its own WiFi hotspot
- Default IP is typically `192.168.4.1`
- `mode` field in `/api/status` returns `"AP"`
- `rssi` field returns `0`
- Used for launcher idle hotspot web UI when `Settings -> System -> Idle Hotspot Web UI` is enabled

---

## Notes

- These examples use `omnipaper.local`. If your network does not support mDNS or the address does not resolve, replace it with the specific **IP Address** displayed on your device screen (e.g., `http://192.168.1.102/`).
- All paths on the SD card start with `/`
- Trailing slashes are automatically stripped (except for root `/`)
- The webserver uses chunked transfer encoding for file listings
