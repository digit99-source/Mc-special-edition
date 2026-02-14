#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>
#include <esp_sleep.h>

class XiaoS3WIOBoard : public ESP32Board {
public:
  XiaoS3WIOBoard() { }

  const char* getManufacturerName() const override {
    return "Xiao S3 WIO";
  }

  void powerOff() override {
    // PRAVI DEEP SLEEP SA WAKE-UP NA USER DUGME

    #ifdef PIN_USER_BTN
      // 1. Sačekaj da se dugme otpusti (ako je pritisnuto)
      while(digitalRead(PIN_USER_BTN) == LOW) {
        delay(10);
      }
      delay(200); // Debounce

      // 2. Konfiguriši wake-up na user button (PIN 21)
      // LOW = buđenje kada se pritisne dugme
      esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_USER_BTN, LOW);

      // 3. Sada sigurno možemo ući u deep sleep
      // ESP32 će se resetovati kada se probudi
      esp_deep_sleep_start();

      // Kod se nikada ne vraća ovde jer deep sleep pravi reset
    #else
      // Ako ne postoji user button, samo restartuj
      esp_restart();
    #endif

    // Never reached
    while(1) { delay(1000); }
  }
};