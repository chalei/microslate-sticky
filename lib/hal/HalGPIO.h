#pragma once

#include <Arduino.h>
#include <BatteryMonitor.h>
#include <InputManager.h>

// ============================================================================
// MicroSlate S3 — Pin Definitions
// Organized by schematic subsystem for clarity
// ============================================================================

// --- Epaper Subsystem (Epaper.kicad_sch) ---
// E-Paper display SPI (shared bus with MicroSD)
#define EPD_SCLK  13   // EP_SCK  — SPI Clock (shared with SD_CLK)
#define EPD_MOSI  14   // EP_SDI  — SPI MOSI  (shared with SD_CMD/MOSI)
#define EPD_CS    15   // EP_CSn  — Chip Select
#define EPD_DC    16   // EP_DCn  — Data/Command
#define EPD_RST   17   // EP_RSTn — Reset
#define EPD_BUSY  18   // EP_BUSY — Busy

#define EP_PWR_EN 47   // EP_PWR_EN — E-Paper power enable

// Touch Panel (GT911)
#define TP_SCL     2   // TP_I2C_SCL
#define TP_SDA     3   // TP_I2C_SDA
#define TP_INT    21   // TP_INT   — Touch interrupt
#define TP_RST    41   // TP_RSTn  — Touch reset
#define TP_PWR_EN 42   // TP_PWR_EN — Touch panel power enable

// --- USB & MicroSD Subsystem (USB_MicroSD.kicad_sch) ---
// MicroSD card (SPI mode, shared bus with E-Paper)
#define SPI_MISO  12   // SD_D0/MISO — SPI MISO (shared between SD card and display)
#define SD_CS_PIN  8   // SD_D3/CS   — SD Card chip select
#define SD_DETECT 11   // SD_DETECT  — Card detect
#define SD_PWR_EN 10   // SD_PWR_EN  — SD card power enable

// USB / UART
#define USB_TXD   44   // USB_TXD
#define USB_RXD   43   // USB_RXD — Used for USB connection detection

// Battery ADC
#define BAT_ADC_PIN 9  // PWR_IN_VOLT — Battery voltage ADC input

// --- Power Subsystem (Power.kicad_sch) ---
#define PWR_BUTTON_PIN  4   // PWR_BUTTON  — Power button
#define PWR_HOLD       45   // PWR_HOLD    — Assert HIGH to latch power on
#define PWR_LOCK       46   // PWR_LOCK    — Power lock
#define EN_BAT_CHG     39   // EN_BAT_CHGn — Battery charge enable (shared with RTC_INTn)
#define CHARGE_STATE   40   // CHARGE_STATE — Charging state input (shared with RED_LED)
#define BFG_INT         7   // BFG_INT     — BFG interrupt (shared with 6D_INTn)
#define BFG_SCL         0   // BFG_I2C_SCL — BFG I2C clock (shared with MISC_I2C_SCL)
#define BFG_SDA         1   // BFG_I2C_SDA — BFG I2C data  (shared with MISC_I2C_SDA)

// --- Peripherals Subsystem (Peripherals.kicad_sch) ---
#define BUTTON_UP_PIN   5   // BUTTON_UP
#define BUTTON_DOWN_PIN 6   // BUTTON_DOWN
#define BUZZER_PWM     48   // BUZZER_PWM
#define PDM_CLK        19   // PDM_CLK
#define PDM_DATA       20   // PDM_DATA
#define PDM_EN         38   // PDM_EN

class HalGPIO {
#if CROSSPOINT_EMULATED == 0
  InputManager inputMgr;
#endif

 public:
  HalGPIO() = default;

  // Start button GPIO and setup SPI for screen and SD card
  void begin();

  // Button input methods
  void update();
  bool isPressed(uint8_t buttonIndex) const;
  bool wasPressed(uint8_t buttonIndex) const;
  bool wasAnyPressed() const;
  bool wasReleased(uint8_t buttonIndex) const;
  bool wasAnyReleased() const;
  unsigned long getHeldTime() const;

  // Setup wake up GPIO and enter deep sleep
  void startDeepSleep();

  // Get battery percentage (range 0-100)
  int getBatteryPercentage() const;

  // Check if USB is connected
  bool isUsbConnected() const;

  // Check if battery is charging
  bool isCharging() const;

  enum class WakeupReason { PowerButton, AfterFlash, AfterUSBPower, Other };

  WakeupReason getWakeupReason() const;

  // Button indices — matching InputManager
  static constexpr uint8_t BTN_UP    = InputManager::BTN_UP;
  static constexpr uint8_t BTN_DOWN  = InputManager::BTN_DOWN;
  static constexpr uint8_t BTN_POWER = InputManager::BTN_POWER;

  // Legacy button aliases for code that still references the old 7-button layout.
  // These map to the closest available button so existing switch/case blocks compile.
  static constexpr uint8_t BTN_BACK    = BTN_DOWN;   // Down doubles as Back
  static constexpr uint8_t BTN_CONFIRM = BTN_UP;     // Up doubles as Confirm
  static constexpr uint8_t BTN_LEFT    = BTN_DOWN;   // Down doubles as Left
  static constexpr uint8_t BTN_RIGHT   = BTN_UP;     // Up doubles as Right
};
