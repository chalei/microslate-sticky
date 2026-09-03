#include "TouchPanel.h"

bool TouchPanel::begin(int sdaPin, int sclPin, int intPin, int rstPin) {
  _intPin = intPin;
  _rstPin = rstPin;

  // --- Initialize I2C first (before reset, so bus is ready) ---
  Wire.begin(sdaPin, sclPin, 400000);  // 400kHz fast mode

  // --- GT911 reset sequence ---
  // The I2C address is latched during reset based on the INT pin state:
  //   INT LOW  during RST rising edge → address 0x14
  //   INT HIGH during RST rising edge → address 0x5D

  pinMode(_rstPin, OUTPUT);
  pinMode(_intPin, OUTPUT);

  // Try address 0x5D first (INT HIGH during reset)
  digitalWrite(_intPin, LOW);
  digitalWrite(_rstPin, LOW);
  delay(20);
  digitalWrite(_intPin, HIGH);  // INT HIGH → address 0x5D
  delay(5);
  digitalWrite(_rstPin, HIGH);
  delay(10);
  pinMode(_intPin, INPUT);
  delay(100);  // GT911 needs ~100ms to boot firmware

  // Try to detect GT911 at 0x5D
  _i2cAddr = 0x5D;
  uint8_t productId[4] = {0};
  if (readReg(REG_PRODUCT_ID, productId, 4) &&
      productId[0] == '9' && productId[1] == '1' && productId[2] == '1') {
    if (Serial) Serial.printf("[%lu] [Touch] GT911 found at 0x5D, ID: %c%c%c%c\n",
                              millis(), productId[0], productId[1], productId[2], productId[3]);
    _initialized = true;
    logResolution();
    clearStatus();
    return true;
  }

  // Not at 0x5D — try 0x14 (INT LOW during reset)
  if (Serial) Serial.printf("[%lu] [Touch] Not at 0x5D, trying 0x14...\n", millis());

  pinMode(_intPin, OUTPUT);
  digitalWrite(_intPin, LOW);
  digitalWrite(_rstPin, LOW);
  delay(20);
  // INT stays LOW → address 0x14
  digitalWrite(_rstPin, HIGH);
  delay(10);
  pinMode(_intPin, INPUT);
  delay(100);

  _i2cAddr = 0x14;
  memset(productId, 0, sizeof(productId));
  if (readReg(REG_PRODUCT_ID, productId, 4) &&
      productId[0] == '9' && productId[1] == '1' && productId[2] == '1') {
    if (Serial) Serial.printf("[%lu] [Touch] GT911 found at 0x14, ID: %c%c%c%c\n",
                              millis(), productId[0], productId[1], productId[2], productId[3]);
    _initialized = true;
    logResolution();
    clearStatus();
    return true;
  }

  // Last resort: scan I2C bus
  if (Serial) {
    Serial.printf("[%lu] [Touch] GT911 not found at 0x14 or 0x5D. Scanning I2C bus...\n", millis());
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.printf("[%lu] [Touch]   Found device at 0x%02X\n", millis(), addr);
      }
    }
  }

  _initialized = false;
  return false;
}

void TouchPanel::logResolution() {
  uint8_t resData[4] = {0};
  if (readReg(REG_X_RES_L, resData, 4)) {
    uint16_t xRes = resData[0] | (resData[1] << 8);
    uint16_t yRes = resData[2] | (resData[3] << 8);
    if (Serial) Serial.printf("[%lu] [Touch] GT911 resolution: %dx%d\n", millis(), xRes, yRes);
  }
}

void TouchPanel::clearStatus() {
  writeReg(REG_STATUS, 0x00);
}

void TouchPanel::update() {
  if (!_initialized) return;

  // Clear one-shot event flags
  _tapEvent = false;
  _longPressEvent = false;
  _swipeEvent = false;

  // Read status register
  uint8_t status = 0;
  if (!readReg(REG_STATUS, &status, 1)) return;

  bool bufferReady = (status & 0x80) != 0;
  uint8_t touchCount = status & 0x0F;

  if (!bufferReady) {
    // No new data — but still run gesture state machine for long-press detection
    if (_gestureState == TOUCH_DOWN && _touching) {
      unsigned long held = millis() - _touchStartMs;
      int dx = _rawX - _touchStartX;
      int dy = _rawY - _touchStartY;
      bool withinTapRadius = (abs(dx) < TAP_MAX_PX && abs(dy) < TAP_MAX_PX);

      if (withinTapRadius && held >= LONGPRESS_MS && !_longPressFired) {
        _longPressEvent = true;
        _longPressFired = true;
        _gestureState = LONG_PRESS_FIRED;
      }
    }
    return;
  }

  // Read touch point data (only first point — single-touch for navigation)
  bool wasTouching = _touching;

  if (touchCount > 0 && touchCount <= 5) {
    uint8_t pointData[8] = {0};
    if (readReg(REG_POINT1, pointData, 8)) {
      // Point data format: trackId, x_low, x_high, y_low, y_high, size_low, size_high, reserved
      _rawX = pointData[1] | (pointData[2] << 8);
      _rawY = pointData[3] | (pointData[4] << 8);
      _touching = true;
    }
  } else {
    _touching = false;
  }

  // Clear the status register so GT911 knows we've read the data
  writeReg(REG_STATUS, 0x00);

  // --- Gesture state machine ---
  unsigned long now = millis();

  if (_touching && !wasTouching) {
    // Finger just touched down
    _gestureState = TOUCH_DOWN;
    _touchStartX = _rawX;
    _touchStartY = _rawY;
    _touchStartMs = now;
    _longPressFired = false;
  }

  if (_touching && _gestureState == TOUCH_DOWN) {
    // Still holding — check for long press
    unsigned long held = now - _touchStartMs;
    int dx = _rawX - _touchStartX;
    int dy = _rawY - _touchStartY;
    bool withinTapRadius = (abs(dx) < TAP_MAX_PX && abs(dy) < TAP_MAX_PX);

    if (withinTapRadius && held >= LONGPRESS_MS && !_longPressFired) {
      _longPressEvent = true;
      _longPressFired = true;
      _gestureState = LONG_PRESS_FIRED;
    }
  }

  if (!_touching && wasTouching) {
    // Finger just lifted — classify the gesture
    unsigned long duration = now - _touchStartMs;
    int dx = _rawX - _touchStartX;
    int dy = _rawY - _touchStartY;
    int absDx = abs(dx);
    int absDy = abs(dy);

    if (_gestureState == LONG_PRESS_FIRED) {
      // Long press already fired — don't also fire tap/swipe
    } else if (absDx >= SWIPE_MIN_PX || absDy >= SWIPE_MIN_PX) {
      // Check for swipe
      if (duration >= SWIPE_MIN_MS && duration <= SWIPE_MAX_MS) {
        if (absDy > absDx && absDx < SWIPE_CROSS_MAX_PX) {
          // Vertical swipe
          _swipeEvent = true;
          _swipeDx = 0;
          _swipeDy = dy;
        } else if (absDx > absDy && absDy < SWIPE_CROSS_MAX_PX) {
          // Horizontal swipe
          _swipeEvent = true;
          _swipeDx = dx;
          _swipeDy = 0;
        }
      }
    } else if (absDx < TAP_MAX_PX && absDy < TAP_MAX_PX) {
      // Check for tap
      if (duration >= TAP_MIN_MS && duration <= TAP_MAX_MS) {
        _tapEvent = true;
      }
    }

    _gestureState = IDLE;
  }
}

bool TouchPanel::hasTap() {
  if (_tapEvent) {
    _tapEvent = false;
    return true;
  }
  return false;
}

bool TouchPanel::hasLongPress() {
  if (_longPressEvent) {
    _longPressEvent = false;
    return true;
  }
  return false;
}

bool TouchPanel::hasSwipe(int* dx, int* dy) {
  if (_swipeEvent) {
    _swipeEvent = false;
    if (dx) *dx = _swipeDx;
    if (dy) *dy = _swipeDy;
    return true;
  }
  return false;
}

// --- I2C helpers ---

bool TouchPanel::writeReg(uint16_t reg, uint8_t value) {
  Wire.beginTransmission(_i2cAddr);
  Wire.write(reg >> 8);      // Register address high byte
  Wire.write(reg & 0xFF);    // Register address low byte
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool TouchPanel::readReg(uint16_t reg, uint8_t* buf, uint8_t len) {
  Wire.beginTransmission(_i2cAddr);
  Wire.write(reg >> 8);      // Register address high byte
  Wire.write(reg & 0xFF);    // Register address low byte
  if (Wire.endTransmission(false) != 0) return false;  // false = repeated start

  uint8_t received = Wire.requestFrom(_i2cAddr, len);
  if (received != len) return false;

  for (uint8_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  return true;
}
