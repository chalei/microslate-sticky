#include <HalGPIO.h>
#include <Preferences.h>
#include <SPI.h>
#include <esp_sleep.h>

void HalGPIO::begin() {
  // Assert PWR_HOLD HIGH immediately to latch power on
  pinMode(PWR_HOLD, OUTPUT);
  digitalWrite(PWR_HOLD, HIGH);

  // E-Paper power enable
  pinMode(EP_PWR_EN, OUTPUT);
  digitalWrite(EP_PWR_EN, HIGH);

  // Touch panel power enable
  pinMode(TP_PWR_EN, OUTPUT);
  digitalWrite(TP_PWR_EN, HIGH);

  // SD card power enable
  pinMode(SD_PWR_EN, OUTPUT);
  digitalWrite(SD_PWR_EN, HIGH);

  // SD card detect (active low with external pull-up typically)
  pinMode(SD_DETECT, INPUT);

  // Charge state input
  pinMode(CHARGE_STATE, INPUT);

  // Initialize button inputs
  inputMgr.begin();

  // Initialize shared SPI bus for E-Paper and SD card
  SPI.begin(EPD_SCLK, SPI_MISO, EPD_MOSI, EPD_CS);

  // BAT_ADC_PIN is configured for ADC via adc1_config_channel_atten in InputManager::begin()
  // — do NOT call pinMode() here as it reconfigures the pin as digital input in dual framework

  // USB RXD for connection detection
  pinMode(USB_RXD, INPUT);
}

void HalGPIO::update() {
  inputMgr.update();
}

bool HalGPIO::isPressed(uint8_t buttonIndex) const { return inputMgr.isPressed(buttonIndex); }

bool HalGPIO::wasPressed(uint8_t buttonIndex) const { return inputMgr.wasPressed(buttonIndex); }

bool HalGPIO::wasAnyPressed() const { return inputMgr.wasAnyPressed(); }

bool HalGPIO::wasReleased(uint8_t buttonIndex) const { return inputMgr.wasReleased(buttonIndex); }

bool HalGPIO::wasAnyReleased() const { return inputMgr.wasAnyReleased(); }

unsigned long HalGPIO::getHeldTime() const { return inputMgr.getHeldTime(); }

void HalGPIO::startDeepSleep() {
  // Ensure that the power button has been released to avoid immediately turning back on if you're holding it
  while (inputMgr.isPressed(BTN_POWER)) {
    delay(50);
    inputMgr.update();
  }

  // Power down peripherals before sleep
  digitalWrite(EP_PWR_EN, LOW);
  digitalWrite(TP_PWR_EN, LOW);
  digitalWrite(SD_PWR_EN, LOW);

  // ESP32-S3 uses ext0/ext1 wakeup instead of C3's gpio_wakeup
  // Wake on power button (active LOW)
  esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(PWR_BUTTON_PIN), 0);

  // Release power hold — the PMIC will cut power after deep sleep entry
  digitalWrite(PWR_HOLD, LOW);

  // Enter Deep Sleep
  esp_deep_sleep_start();
}

int HalGPIO::getBatteryPercentage() const {
  static const BatteryMonitor battery = BatteryMonitor(BAT_ADC_PIN);
  static int cachedPct = -1;
  static float smoothedMv = -1.0f;
  static unsigned long lastReadMs = 0;
  static bool adcSettled = false;

  // On first call, load the last persisted reading so we don't show a stale default
  if (cachedPct < 0) {
    Preferences prefs;
    prefs.begin("battery", true);  // read-only
    cachedPct = prefs.getInt("pct", -1);
    prefs.end();
  }

  unsigned long now = millis();

  const bool usbCharging = isUsbConnected();

  // ADC reads high for ~2 minutes after boot/wake — trust NVS cache during settling.
  // Skip this window when charging: voltage is actively changing and we want real readings.
  // Also skip if NVS has no cached value (fresh flash) — better to read ADC than show nothing.
  if (!adcSettled) {
    if (!usbCharging && now < 120000 && cachedPct >= 0) {
      // Still settling and we have a cached value — use it
      return cachedPct;
    }
    adcSettled = true;
  }

  // Battery voltage changes on a timescale of minutes — no need to read every frame
  if (cachedPct < 0 || (now - lastReadMs) >= 30000) {
    float rawMv = static_cast<float>(battery.readMillivolts());

    // EMA smoothing on millivolts (before the nonlinear polynomial).
    // Alpha=0.3 means ~70% weight on history — takes ~5 reads (~2.5 min) to converge,
    // which rejects brief voltage spikes from charging cycles / SPI / BLE noise.
    // When charging, use alpha=1.0 (no smoothing) so voltage tracks in real time.
    if (smoothedMv < 0) {
      smoothedMv = rawMv;  // seed with first real reading
    } else if (usbCharging) {
      smoothedMv = rawMv;  // no smoothing while charging
    } else {
      smoothedMv = 0.3f * rawMv + 0.7f * smoothedMv;
    }

    int newPct = BatteryMonitor::percentageFromMillivolts(static_cast<uint16_t>(smoothedMv));

    // Rate-limit drops only: max 2% decrease per read cycle (every 30s) on battery.
    // Rising is uncapped so the display catches up quickly after charging.
    // When charging via USB, drops are also uncapped (voltage actively rising anyway).
    if (cachedPct >= 0) {
      const int maxDrop = usbCharging ? 20 : 2;
      if (newPct < cachedPct - maxDrop) newPct = cachedPct - maxDrop;
    }

    if (newPct != cachedPct) {
      Preferences prefs;
      prefs.begin("battery", false);
      prefs.putInt("pct", newPct);
      prefs.end();
    }
    cachedPct = newPct;
    lastReadMs = now;
  }
  return cachedPct;
}

bool HalGPIO::isUsbConnected() const {
  // USB_RXD/GPIO43 reads HIGH when USB is connected
  return digitalRead(USB_RXD) == HIGH;
}

bool HalGPIO::isCharging() const {
  // CHARGE_STATE pin indicates charging status
  return digitalRead(CHARGE_STATE) == LOW;
}

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const {
  const bool usbConnected = isUsbConnected();
  const auto wakeupCause = esp_sleep_get_wakeup_cause();
  const auto resetReason = esp_reset_reason();

  if ((wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && !usbConnected) ||
      (wakeupCause == ESP_SLEEP_WAKEUP_EXT0 && resetReason == ESP_RST_DEEPSLEEP)) {
    return WakeupReason::PowerButton;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_UNKNOWN && usbConnected) {
    return WakeupReason::AfterFlash;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && usbConnected) {
    return WakeupReason::AfterUSBPower;
  }
  return WakeupReason::Other;
}
