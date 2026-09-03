#pragma once

#include <Arduino.h>
#include <Wire.h>

/**
 * GT911 capacitive touch panel driver with gesture detection.
 *
 * Communicates with the GT911 via I2C and provides a high-level gesture API:
 * tap, long-press, and swipe (up/down/left/right). Raw touch coordinates are
 * also available for direct hit-testing if needed later.
 *
 * The GT911 I2C address is set to 0x14 during the reset sequence by holding
 * the INT pin LOW while releasing RESET.
 */
class TouchPanel {
 public:
  TouchPanel() = default;

  /**
   * Initialize the GT911: power on, reset sequence, verify product ID,
   * and start I2C on the configured pins.
   *
   * @param sdaPin  I2C SDA GPIO
   * @param sclPin  I2C SCL GPIO
   * @param intPin  GT911 interrupt GPIO
   * @param rstPin  GT911 reset GPIO
   * @return true if GT911 responded and product ID verified
   */
  bool begin(int sdaPin, int sclPin, int intPin, int rstPin);

  /**
   * Poll the GT911 for touch data and run the gesture state machine.
   * Call this once per main loop iteration.
   */
  void update();

  // --- Gesture queries (one-shot: return true once per gesture, then reset) ---

  /** True if a tap was detected since the last update() cycle. */
  bool hasTap();

  /** True if a long-press (>800ms hold) was detected since the last update() cycle. */
  bool hasLongPress();

  /**
   * True if a swipe was detected since the last update() cycle.
   * @param dx  horizontal displacement (positive = right)
   * @param dy  vertical displacement (positive = down)
   */
  bool hasSwipe(int* dx, int* dy);

  /** True if a finger is currently touching the screen. */
  bool isTouching() const { return _touching; }

  /** Get the last raw touch X coordinate (0..maxX). */
  int getRawX() const { return _rawX; }

  /** Get the last raw touch Y coordinate (0..maxY). */
  int getRawY() const { return _rawY; }

  /** True if the GT911 was successfully detected and initialized. */
  bool isInitialized() const { return _initialized; }

 private:
  // I2C address — auto-detected during begin() (0x14 or 0x5D)
  uint8_t _i2cAddr = 0x5D;

  // GT911 register addresses (16-bit)
  static constexpr uint16_t REG_PRODUCT_ID   = 0x8140;  // 4 bytes: "911\0" or "9111"
  static constexpr uint16_t REG_CONFIG_VER   = 0x8047;
  static constexpr uint16_t REG_X_RES_L      = 0x8048;
  static constexpr uint16_t REG_X_RES_H      = 0x8049;
  static constexpr uint16_t REG_Y_RES_L      = 0x804A;
  static constexpr uint16_t REG_Y_RES_H      = 0x804B;
  static constexpr uint16_t REG_STATUS       = 0x814E;  // bit7=ready, bits3:0=touch count
  static constexpr uint16_t REG_POINT1       = 0x814F;  // 8 bytes per point

  // Gesture detection thresholds
  static constexpr int SWIPE_MIN_PX        = 40;   // Minimum displacement for swipe
  static constexpr int SWIPE_CROSS_MAX_PX  = 30;   // Maximum cross-axis displacement
  static constexpr unsigned long SWIPE_MIN_MS = 80;
  static constexpr unsigned long SWIPE_MAX_MS = 600;
  static constexpr int TAP_MAX_PX          = 15;   // Maximum movement for tap
  static constexpr unsigned long TAP_MIN_MS  = 30;
  static constexpr unsigned long TAP_MAX_MS  = 400;
  static constexpr unsigned long LONGPRESS_MS = 800;

  // I2C helpers
  bool writeReg(uint16_t reg, uint8_t value);
  bool readReg(uint16_t reg, uint8_t* buf, uint8_t len);
  void logResolution();
  void clearStatus();

  // State
  bool _initialized = false;
  int _intPin = -1;
  int _rstPin = -1;

  // Current touch state
  bool _touching = false;
  int _rawX = 0;
  int _rawY = 0;

  // Gesture state machine
  enum GestureState { IDLE, TOUCH_DOWN, LONG_PRESS_FIRED };
  GestureState _gestureState = IDLE;
  int _touchStartX = 0;
  int _touchStartY = 0;
  unsigned long _touchStartMs = 0;
  bool _longPressFired = false;

  // One-shot gesture event flags (cleared after read)
  bool _tapEvent = false;
  bool _longPressEvent = false;
  bool _swipeEvent = false;
  int _swipeDx = 0;
  int _swipeDy = 0;
};
