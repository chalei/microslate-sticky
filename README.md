# MicroSlate

A dedicated distraction-free writing firmware built for **ESP32-S3** e-paper device. Based on the original repo from Josh-Write. 
https://github.com/Josh-writes/microslate-firmware.

Pairs with any **Bluetooth LE (BLE)** keyboard, supports physical button navigation, and saves notes to MicroSD.

---

## Features

- **Bluetooth Keyboard** — BLE HID host, connects to standard wireless keyboards. Stores up to 4 paired keyboards; auto-reconnects on boot. Full menu navigation & text editing support.
- **Physical Controls** — Discrete 3-button control scheme (Up, Down, Confirm/Power, plus Up+Down back combo) for navigating without an external keyboard.
- **Note Management** — Browse, create, rename, and delete notes from an SD card.
- **Named Notes** — Each note has a title stored in the file; shown in the browser and editable without touching body text.
- **Text Editor** — Cursor navigation, word-wrap, fast e-paper refresh, and key repeat.
- **Writing Modes** — Three display modes to suit different writing styles:
  - *Scroll* — Standard scrolling editor (default).
  - *Typewriter* — Shows only the current line centered on a blank screen for focused single-line writing.
  - *Pagination* — Page-based display instead of continuous scrolling with clean page flips.
- **Auto-Save** — Silently saves to SD card after 10 seconds of idle or every 2 minutes during continuous typing. Every exit path (button combo, Esc, power button, sleep) also saves automatically.
- **Safe Writes** — Write-verify + `.bak` rotation pattern; failed or interrupted writes never destroy the previous version.
- **Clean Mode** — Hides all UI chrome while editing so only text is on screen (`Ctrl+Z` to toggle).
- **Dark Mode** — Inverted display for high contrast.
- **Display Orientation** — Portrait, Landscape Clockwise, Inverted, and Landscape Counter-Clockwise.
- **Battery Monitor** — Calibrated ADC voltage measurement on GPIO 9 with noise filtering and charge-state detection.
- **Power Management** — ESP-IDF tickless light sleep (CPU drops to 10MHz), BLE modem sleep, peripheral sleep, and deep sleep with wake on power button.
- **WiFi Sync** — One-button backup of all notes to your PC over WiFi. Saves network credentials for instant reconnect. Read-only server — nothing on the device can be modified over the network.
- **Settings Backup** — BLE pairing info, WiFi credentials, and UI preferences are backed up to the SD card as JSON and restored automatically across firmware updates.

---

## Hardware Specifications (ESP32-S3)

- **MCU:** ESP32-S3 (Dual-core Xtensa LX7 @ 240MHz, 16MB Flash, 320KB SRAM)
- **Display:** 800×480 E-Paper Display (SPI)
- **Storage:** MicroSD Card (SPI mode, shared with E-Paper)
- **Physical Buttons:**
  - **AI / Power Button:** `GPIO 4`
  - **Up Button:** `GPIO 5`
  - **Down Button:** `GPIO 6`
- **Power & Battery Subsystem:**
  - **PWR_HOLD:** `GPIO 45` (Latches system power on)
  - **PWR_LOCK:** `GPIO 46`
  - **BAT_ADC_PIN:** `GPIO 9` (ADC1 Channel 8, battery voltage monitor)
  - **CHARGE_STATE:** `GPIO 40` (Charge status indicator)
  - **EN_BAT_CHG:** `GPIO 39` (Battery charge enable)
  - **Battery Fuel Gauge (BFG):** `GPIO 0` (SCL), `GPIO 1` (SDA), `GPIO 7` (INT)
- **Display & Storage Pins:**
  - **SPI SCK / MOSI / MISO:** `GPIO 13`, `GPIO 14`, `GPIO 12`
  - **EPD Control (CS, DC, RST, BUSY, PWR):** `GPIO 15`, `GPIO 16`, `GPIO 17`, `GPIO 18`, `GPIO 47`
  - **SD Control (CS, DETECT, PWR):** `GPIO 8`, `GPIO 11`, `GPIO 10`

---

## Device Controls

### 1. Physical Buttons

| Button | Action | Function |
|--------|--------|----------|
| **Up (`GPIO 5`)** | Press | Move selection Up / Scroll up (with repeat in editor) |
| **Down (`GPIO 6`)** | Press | Move selection Down / Scroll down (with repeat in editor) |
| **Power (`GPIO 4`)** | Short Press (< 1s) | **Confirm / Select / Enter** |
| **Power (`GPIO 4`)** | Long Press (> 3s) | **Enter Deep Sleep** |
| **Up + Down Combo** | Press Together | **Back / Escape** (Save & return to previous menu) |

### 2. BLE Keyboard Navigation

When a Bluetooth keyboard is paired, you can navigate the entire interface and edit text:

#### Menus & Lists (Main Menu, File Browser, Settings)
| Key | Action |
|-----|--------|
| **Up / Down** | Move selection / Navigate list |
| **Enter** | Select / Open item |
| **Left / Right** | Cycle settings values / Secondary actions (Scan/Disconnect in BT) |
| **Esc** | Go back / Return to previous screen |

#### Text Editor
| Key | Action |
|-----|--------|
| **Arrow keys** | Move cursor (Up, Down, Left, Right) |
| **Home / End** | Jump to start / end of current line |
| **Backspace / Delete** | Remove characters |
| **Tab** | Cycle writing mode (Scroll → Typewriter → Pagination) |
| **Ctrl + S** | Manual save (auto-save is also active) |
| **Ctrl + N** | Edit note title |
| **Ctrl + Z** | Toggle Clean Mode (hides header/footer) |
| **Ctrl + T** | Toggle Typewriter mode |
| **Ctrl + P** | Toggle Pagination mode |
| **Ctrl + Left / Right** | Jump pages (Pagination mode only) |
| **Esc** | Save and return to File Browser |

---

## Building and Flashing

### Prerequisites

- [PlatformIO Core (CLI)](https://platformio.org/install/cli) or VS Code with the PlatformIO extension.
- USB-C cable to connect to the ESP32-S3 board.

### Build and Upload

```bash
# Clone the repository
git clone https://github.com/Josh-writes/microslate-firmware
cd microslate-firmware

# Build firmware for ESP32-S3
pio run -e microslate_s3

# Upload to the device (adjust port if needed, or let PlatformIO auto-detect)
pio run -e microslate_s3 -t upload

# Open serial monitor (115200 baud)
pio device monitor -b 115200
```

> **Note on macOS PlatformIO path:** If `pio` is not in your global system `PATH`, you can use:  
> `~/.platformio/penv/bin/pio run -e microslate_s3 -t upload`

---

## Bluetooth Keyboard Pairing

1. Boot the device to the **Main Menu**.
2. Navigate to **Settings → Bluetooth** using the Up/Down buttons (or keyboard) and press **Confirm (Power button / Enter)**.
3. Put your Bluetooth keyboard into pairing mode.
4. Press **Right** (or wait for the 5-second one-shot scan) to discover nearby BLE devices.
5. Select your keyboard from the list and press **Confirm / Enter**.
6. The device establishes connection, handles security, and stores the pairing in NVS and SD backup.
7. Future boots will automatically search for and connect to your paired keyboard.

---

## WiFi Sync (PC Backup)

Back up all notes from the device to your PC over your local network:

1. Select **Sync** from the Main Menu.
2. Select your WiFi network and enter your password.
3. On your PC (in the same WiFi network), run:
   ```bash
   python3 sync/microslate_sync.py
   ```
4. Notes are automatically backed up to `Documents/MicroSlate Notes/`.
5. The device shuts down WiFi automatically when done.

---

## Troubleshooting

- **BLE Keyboard connected but navigation not working:** Ensure the keyboard is in BLE mode (not 2.4GHz dongle mode or Classic Bluetooth). The firmware automatically subscribes to HID Report characteristics upon connection.
- **Battery percentage reading 0% or fluctuating:** On a fresh flash, the battery monitor seeds its initial calibration and EMA smoothing filter. Allow a moment for the ADC to settle.
- **Physical buttons not responding:** Ensure `PWR_HOLD` (`GPIO 45`) is asserted. Confirm button press with short clicks. For Back/Escape, press both **Up (`GPIO 5`)** and **Down (`GPIO 6`)** simultaneously.
- **Display not refreshing:** E-paper updates take ~400ms for fast refresh. The firmware prevents overlapping refreshes while the display BUSY line is active.

---

