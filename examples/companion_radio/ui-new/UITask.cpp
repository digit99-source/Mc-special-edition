#include <helpers/ui/buzzer.h>
#include <NonBlockingRtttl.h>

#ifdef PIN_BUZZER
//NonBlockingRtttl rtttl;
#endif

#ifndef LED_STATE_ON
  #define LED_STATE_ON HIGH
#endif

#ifndef LED_STATE_OFF
  #define LED_STATE_OFF LOW
#endif

#include "UITask.h"
#include <helpers/TxtDataHelpers.h>
#include "../MyMesh.h"
#include "target.h"

// ✅ PROMENA 1: Auto off = 1 minuta (60000ms)
#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS     60000   // 60 seconds = 1 minute
#endif

#define BOOT_SCREEN_MILLIS   3000

#ifdef PIN_STATUS_LED
#define LED_ON_MILLIS     20
#define LED_ON_MSG_MILLIS 400
#define LED_CYCLE_MILLIS  4000
#endif

#define LONG_PRESS_MILLIS   1200

#ifndef UI_RECENT_LIST_SIZE
  #define UI_RECENT_LIST_SIZE 4
#endif

#if UI_HAS_JOYSTICK
  #define PRESS_LABEL "press Enter"
#else
  #define PRESS_LABEL "long press"
#endif

#include "icons.h"

class SplashScreen : public UIScreen {
  UITask* _task;
  unsigned long dismiss_after;
  char _version_info[12];

public:
  SplashScreen(UITask* task) : _task(task) {
    const char *ver = FIRMWARE_VERSION;
    const char *dash = strchr(ver, '-');
    int len = dash ? dash - ver : strlen(ver);
    if (len >= sizeof(_version_info)) len = sizeof(_version_info) - 1;
    memcpy(_version_info, ver, len);
    _version_info[len] = 0;
    dismiss_after = millis() + BOOT_SCREEN_MILLIS;
  }

  int render(DisplayDriver& display) override {
    display.setColor(DisplayDriver::LIGHT);
    int logoWidth = 128;
    display.drawXbm((display.width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);
    display.setTextSize(2);
    display.drawTextCentered(display.width()/2, 22, _version_info);
    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, 42, FIRMWARE_BUILD_DATE);
    return 1000;
  }

  void poll() override {
    if (millis() >= dismiss_after) {
      _task->gotoHomeScreen();
    }
  }
};

class HomeScreen : public UIScreen {
  enum HomePage {
    FIRST,
    RECENT,
    RADIO,
    SOUND,
    ADVERT,
    TEMP_PRESSURE,  // ✅ NOVO: BMP280 temperature and pressure display
    BATTERY,
    NOISE_FLOOR,    // ✅ NOVO: Noise floor display with graph
#if ENV_INCLUDE_GPS == 1
    GPS,
#endif
#if UI_SENSORS_PAGE == 1
    SENSORS,
#endif
    SHUTDOWN,
    Count
  };

  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  uint8_t _page;
  bool _shutdown_init;
  AdvertPath recent[UI_RECENT_LIST_SIZE];

  // ✅ NOVO: Noise floor history for graph
  #define NOISE_FLOOR_HISTORY_SIZE 32
  int noise_floor_history[NOISE_FLOOR_HISTORY_SIZE];
  int noise_floor_index = 0;
  unsigned long next_noise_floor_update = 0;

  void renderBatteryIndicator(DisplayDriver& display, uint16_t batteryMilliVolts) {
    float voltage = batteryMilliVolts / 1000.0f;
    char voltageStr[16];
    snprintf(voltageStr, sizeof(voltageStr), "%.2fV", voltage);
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    int textWidth = display.getTextWidth(voltageStr);
    int x = display.width() - textWidth - 2;
    display.setCursor(x, 0);
    display.print(voltageStr);
  }

  CayenneLPP sensors_lpp;
  int sensors_nb = 0;
  bool sensors_scroll = false;
  int sensors_scroll_offset = 0;
  int next_sensors_refresh = 0;

  void refresh_sensors() {
    if (millis() > next_sensors_refresh) {
      sensors_lpp.reset();
      sensors_nb = 0;
      sensors_lpp.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      sensors.querySensors(0xFF, sensors_lpp);
      LPPReader reader (sensors_lpp.getBuffer(), sensors_lpp.getSize());
      uint8_t channel, type;
      while(reader.readHeader(channel, type)) {
        reader.skipData(type);
        sensors_nb ++;
      }
      sensors_scroll = sensors_nb > UI_RECENT_LIST_SIZE;
      // ✅ FIX: Refresh rate = 1000ms (1 sekunda) za brže ažuriranje temperature
      next_sensors_refresh = millis() + 1000;
    }
  }

  // ✅ NOVO: Update noise floor history for graph
  void updateNoiseFloorHistory() {
    if (millis() > next_noise_floor_update) {
      int current_noise = radio_driver.getNoiseFloor();
      noise_floor_history[noise_floor_index] = current_noise;
      noise_floor_index = (noise_floor_index + 1) % NOISE_FLOOR_HISTORY_SIZE;
      next_noise_floor_update = millis() + 1000;
    }
  }

public:
  HomeScreen(UITask* task, mesh::RTCClock* rtc, SensorManager* sensors, NodePrefs* node_prefs)
     : _task(task), _rtc(rtc), _sensors(sensors), _node_prefs(node_prefs), _page(0),
       _shutdown_init(false), sensors_lpp(200) {
    // Initialize noise floor history
    for (int i = 0; i < NOISE_FLOOR_HISTORY_SIZE; i++) {
      noise_floor_history[i] = -120;
    }
    noise_floor_index = 0;
    next_noise_floor_update = 0;
    next_sensors_refresh = 0;  // ✅ FIX: Inicijalizujemo na 0 da bi se senzori odmah učitali
  }

  void poll() override {
    if (_shutdown_init && !_task->isButtonPressed()) {
      _task->shutdown();
    }
  }

  int render(DisplayDriver& display) override {
    char tmp[80];
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    char filtered_name[sizeof(_node_prefs->node_name)];
    display.translateUTF8ToBlocks(filtered_name, _node_prefs->node_name, sizeof(filtered_name));
    display.setCursor(0, 0);
    display.print(filtered_name);

    renderBatteryIndicator(display, _task->getBattMilliVolts());

    int y = 14;
    int x = display.width() / 2 - 5 * (HomePage::Count-1);
    for (uint8_t i = 0; i < HomePage::Count; i++, x += 10) {
      if (i == _page) {
        display.fillRect(x-1, y-1, 3, 3);
      } else {
        display.fillRect(x, y, 1, 1);
      }
    }

    if (_page == HomePage::FIRST) {
      display.setTextSize(2);
      display.setColor(DisplayDriver::LIGHT);
      sprintf(tmp, "MSG: %d", _task->getMsgCount());
      display.drawTextCentered(display.width() / 2, 20, tmp);

      if (_task->hasConnection()) {
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, 43, "< Connected >");
      } else if (the_mesh.getBLEPin() != 0) {
        display.setTextSize(2);
        sprintf(tmp, "Pin:%d", the_mesh.getBLEPin());
        display.drawTextCentered(display.width() / 2, 43, tmp);
      }
    } else if (_page == HomePage::RECENT) {
      the_mesh.getRecentlyHeard(recent, UI_RECENT_LIST_SIZE);
      display.setColor(DisplayDriver::LIGHT);
      int y = 20;
      for (int i = 0; i < UI_RECENT_LIST_SIZE; i++, y += 11) {
        auto a = &recent[i];
        if (a->name[0] == 0) continue;
        int secs = _rtc->getCurrentTime() - a->recv_timestamp;
        if (secs < 60) {
          sprintf(tmp, "%ds", secs);
        } else if (secs < 60*60) {
          sprintf(tmp, "%dm", secs / 60);
        } else {
          sprintf(tmp, "%dh", secs / (60*60));
        }

        int timestamp_width = display.getTextWidth(tmp);
        int max_name_width = display.width() - timestamp_width - 1;

        char filtered_recent_name[sizeof(a->name)];
        display.translateUTF8ToBlocks(filtered_recent_name, a->name, sizeof(filtered_recent_name));
        display.drawTextEllipsized(0, y, max_name_width, filtered_recent_name);
        display.setCursor(display.width() - timestamp_width - 1, y);
        display.print(tmp);
      }
    } else if (_page == HomePage::RADIO) {
      display.setTextSize(1);
      display.setColor(DisplayDriver::LIGHT);
      display.setCursor(0, 20);
      sprintf(tmp, "FQ: %06.3f   SF: %d", _node_prefs->freq, _node_prefs->sf);
      display.print(tmp);
      display.setCursor(0, 31);
      sprintf(tmp, "BW: %03.2f     CR: %d", _node_prefs->bw, _node_prefs->cr);
      display.print(tmp);
      display.setCursor(0, 42);
      sprintf(tmp, "TX: %ddBm", _node_prefs->tx_power_dbm);
      display.print(tmp);
      display.setCursor(0, 53);
      sprintf(tmp, "Noise floor: %d", radio_driver.getNoiseFloor());
      display.print(tmp);
    } else if (_page == HomePage::SOUND) {
      display.setColor(DisplayDriver::LIGHT);
      display.drawXbm((display.width() - 32) / 2, 18,
          _node_prefs->buzzer_quiet ? sound_off : sound_on,
          32, 32);
      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, 64 - 11, "toggle: " PRESS_LABEL);
    } else if (_page == HomePage::ADVERT) {
      display.setColor(DisplayDriver::LIGHT);
      display.drawXbm((display.width() - 32) / 2, 18, advert_icon, 32, 32);
      display.drawTextCentered(display.width() / 2, 64 - 11, "advert: " PRESS_LABEL);
    } else if (_page == HomePage::TEMP_PRESSURE) {
      // ✅ NOVO: BMP280 Temperature and Pressure display
      display.setColor(DisplayDriver::LIGHT);
      refresh_sensors();

      float temperature = 0.0f;
      float pressure = 0.0f;
      bool has_temp = false;
      bool has_pressure = false;

      LPPReader reader(sensors_lpp.getBuffer(), sensors_lpp.getSize());
      uint8_t channel, type;
      while (reader.readHeader(channel, type)) {
        if (type == LPP_TEMPERATURE) {
          reader.readTemperature(temperature);
          has_temp = true;
        } else if (type == LPP_BAROMETRIC_PRESSURE) {
          reader.readPressure(pressure);
          has_pressure = true;
        } else {
          reader.skipData(type);
        }
      }

      // ✅ Temperature display - TOP (BIG font) -3.5°C correction
      display.setTextSize(2);
      display.setColor(DisplayDriver::LIGHT);
      char tempStr[16];
      if (has_temp) {
        float correctedTemp = temperature - 3.5;  // ✅ Korekcija -3.5°C
        sprintf(tempStr, "%.1f\C", correctedTemp);  // ✅ \xF7 = ° simbol
      } else {
        strcpy(tempStr, "--.-\C");
      }
      display.drawTextCentered(display.width() / 2, 22, tempStr);

      // ✅ Pressure display - BOTTOM (BIG font)
      display.setTextSize(2);
      display.setColor(DisplayDriver::LIGHT);
      char pressStr[16];
      if (has_pressure) {
        sprintf(pressStr, "%.0fhPa", pressure);
      } else {
        strcpy(pressStr, "---hPa");
      }
      display.drawTextCentered(display.width() / 2, 46, pressStr);
    } else if (_page == HomePage::BATTERY) {
      display.setColor(DisplayDriver::LIGHT);
      uint16_t batteryMilliVolts = _task->getBattMilliVolts();
      float batteryVoltage = batteryMilliVolts / 1000.0f;
      int batteryPercent = (int)(((batteryVoltage - 3.0f) / 1.2f) * 100.0f);
      if (batteryPercent < 0) batteryPercent = 0;
      if (batteryPercent > 100) batteryPercent = 100;

      display.setTextSize(2);
      char voltageStr[16];
      sprintf(voltageStr, "%.2fV", batteryVoltage);
      display.drawTextCentered(display.width() / 2, 22, voltageStr);

      char percentStr[8];
      sprintf(percentStr, "%d%%", batteryPercent);
      display.setTextSize(2);
      display.drawTextLeftAlign(10, 46, percentStr);

      int iconX = 70;
      int iconY = 44;
      int iconWidth = 38;
      int iconHeight = 16;
      int tipWidth = 4;
      int tipHeight = 5;

      display.drawRect(iconX, iconY, iconWidth, iconHeight);
      display.fillRect(iconX + iconWidth, iconY + (iconHeight - tipHeight) / 2, tipWidth, tipHeight);

      int fillWidth = (iconWidth - 2) * batteryPercent / 100;
      if (fillWidth > 0) {
        display.fillRect(iconX + 1, iconY + 1, fillWidth, iconHeight - 2);
      }
    } else if (_page == HomePage::NOISE_FLOOR) {
      // ✅ NOVO: Noise Floor display with graph
      display.setColor(DisplayDriver::LIGHT);
      updateNoiseFloorHistory();
      int current_noise = radio_driver.getNoiseFloor();

      display.setTextSize(2);
      char noiseStr[16];
      sprintf(noiseStr, "%ddB", current_noise);
      display.drawTextCentered(display.width() / 2, 18, noiseStr);

      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, 36, "NOISE FLOOR");

      int graphX = 2;
      int graphY = 44;
      int graphWidth = display.width() - 4;
      int graphHeight = 18;

      int min_noise = 0;
      int max_noise = -120;
      for (int i = 0; i < NOISE_FLOOR_HISTORY_SIZE; i++) {
        if (noise_floor_history[i] < min_noise) min_noise = noise_floor_history[i];
        if (noise_floor_history[i] > max_noise) max_noise = noise_floor_history[i];
      }
      if (max_noise <= min_noise) {
        max_noise = min_noise + 10;
      }
      int noise_range = max_noise - min_noise;
      if (noise_range < 5) noise_range = 5;

      display.drawRect(graphX, graphY, graphWidth, graphHeight);

      for (int i = 0; i < NOISE_FLOOR_HISTORY_SIZE - 1; i++) {
        int idx1 = (noise_floor_index + i) % NOISE_FLOOR_HISTORY_SIZE;
        int idx2 = (noise_floor_index + i + 1) % NOISE_FLOOR_HISTORY_SIZE;

        if (noise_floor_history[idx1] != -120 && noise_floor_history[idx2] != -120) {
          int x1 = graphX + (i * (graphWidth - 2)) / (NOISE_FLOOR_HISTORY_SIZE - 1);
          int y1 = graphY + graphHeight - 1 - ((noise_floor_history[idx1] - min_noise) * (graphHeight - 2)) / noise_range;
          int x2 = graphX + ((i + 1) * (graphWidth - 2)) / (NOISE_FLOOR_HISTORY_SIZE - 1);
          int y2 = graphY + graphHeight - 1 - ((noise_floor_history[idx2] - min_noise) * (graphHeight - 2)) / noise_range;

          if (x1 == x2) {
            int y_start = (y1 < y2) ? y1 : y2;
            int line_height = abs(y2 - y1) + 1;
            display.fillRect(x1, y_start, 1, line_height);
          } else if (y1 == y2) {
            int x_start = (x1 < x2) ? x1 : x2;
            int line_width = abs(x2 - x1) + 1;
            display.fillRect(x_start, y1, line_width, 1);
          } else {
            display.fillRect(x1, y1, 1, 1);
            display.fillRect(x2, y2, 1, 1);
          }
        }
      }
#if ENV_INCLUDE_GPS == 1
    } else if (_page == HomePage::GPS) {
      LocationProvider* nmea = sensors.getLocationProvider();
      char buf[50];
      int y = 18;
      bool gps_state = _task->getGPSState();
#ifdef PIN_GPS_SWITCH
      bool hw_gps_state = digitalRead(PIN_GPS_SWITCH);
      if (gps_state != hw_gps_state) {
        strcpy(buf, gps_state ? "gps off(hw)" : "gps off(sw)");
      } else {
        strcpy(buf, gps_state ? "gps on" : "gps off");
      }
#else
      strcpy(buf, gps_state ? "gps on" : "gps off");
#endif
      display.drawTextLeftAlign(0, y, buf);
      if (nmea == NULL) {
        y = y + 12;
        display.drawTextLeftAlign(0, y, "Can't access GPS");
      } else {
        strcpy(buf, nmea->isValid()?"fix":"no fix");
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "sat");
        sprintf(buf, "%d", nmea->satellitesCount());
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "pos");
        sprintf(buf, "%.4f %.4f",
           nmea->getLatitude()/1000000., nmea->getLongitude()/1000000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "alt");
        sprintf(buf, "%.2f", nmea->getAltitude()/1000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
      }
#endif
#if UI_SENSORS_PAGE == 1
    } else if (_page == HomePage::SENSORS) {
      int y = 18;
      refresh_sensors();
      char buf[30];
      char name[30];
      LPPReader r(sensors_lpp.getBuffer(), sensors_lpp.getSize());

      for (int i = 0; i < sensors_scroll_offset; i++) {
        uint8_t channel, type;
        r.readHeader(channel, type);
        r.skipData(type);
      }

      for (int i = 0; i < (sensors_scroll?UI_RECENT_LIST_SIZE:sensors_nb); i++) {
        uint8_t channel, type;
        if (!r.readHeader(channel, type)) {
          r.reset();
          r.readHeader(channel, type);
        }

        display.setCursor(0, y);
        float v;
        switch (type) {
          case LPP_GPS:
            float lat, lon, alt;
            r.readGPS(lat, lon, alt);
            strcpy(name, "gps"); sprintf(buf, "%.4f %.4f", lat, lon);
            break;
          case LPP_VOLTAGE:
            r.readVoltage(v);
            strcpy(name, "voltage"); sprintf(buf, "%6.2f", v);
            break;
          case LPP_CURRENT:
            r.readCurrent(v);
            strcpy(name, "current"); sprintf(buf, "%.3f", v);
            break;
          case LPP_TEMPERATURE:
            r.readTemperature(v);
            strcpy(name, "temperature"); sprintf(buf, "%.2f", v);
            break;
          case LPP_RELATIVE_HUMIDITY:
            r.readRelativeHumidity(v);
            strcpy(name, "humidity"); sprintf(buf, "%.2f", v);
            break;
          case LPP_BAROMETRIC_PRESSURE:
            r.readPressure(v);
            strcpy(name, "pressure"); sprintf(buf, "%.2f", v);
            break;
          case LPP_ALTITUDE:
            r.readAltitude(v);
            strcpy(name, "altitude"); sprintf(buf, "%.0f", v);
            break;
          case LPP_POWER:
            r.readPower(v);
            strcpy(name, "power"); sprintf(buf, "%6.2f", v);
            break;
          default:
            r.skipData(type);
            strcpy(name, "unk"); sprintf(buf, "");
        }
        display.setCursor(0, y);
        display.print(name);
        display.setCursor(
          display.width()-display.getTextWidth(buf)-1, y
        );
        display.print(buf);
        y = y + 12;
      }
      if (sensors_scroll) sensors_scroll_offset = (sensors_scroll_offset+1)%sensors_nb;
      else sensors_scroll_offset = 0;
#endif
    } else if (_page == HomePage::SHUTDOWN) {
      display.setTextSize(1);
      display.setColor(DisplayDriver::LIGHT);
      if (_shutdown_init) {
        display.drawTextCentered(display.width() / 2, 34, "hibernating...");
      } else {
        display.drawXbm((display.width() - 32) / 2, 18, power_icon, 32, 32);
        display.drawTextCentered(display.width() / 2, 64 - 11, "hibernate:" PRESS_LABEL);
      }
    }
    return 5000;
  }

  bool handleInput(char c) override {
    if (c == KEY_LEFT || c == KEY_PREV) {
      _page = (_page + HomePage::Count - 1) % HomePage::Count;
      return true;
    }
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      _page = (_page + 1) % HomePage::Count;
      if (_page == HomePage::RECENT) {
        _task->showAlert("Recent adverts", 800);
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::SOUND) {
      _task->toggleBuzzer();
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::ADVERT) {
      _task->notify(UIEventType::ack);
      if (the_mesh.advert()) {
        _task->showAlert("Advert sent!", 1000);
      } else {
        _task->showAlert("Advert failed..", 1000);
      }
      return true;
    }
#if ENV_INCLUDE_GPS == 1
    if (c == KEY_ENTER && _page == HomePage::GPS) {
      _task->toggleGPS();
      return true;
    }
#endif
#if UI_SENSORS_PAGE == 1
    if (c == KEY_ENTER && _page == HomePage::SENSORS) {
      _task->toggleGPS();
      next_sensors_refresh=0;
      return true;
    }
#endif
    if (c == KEY_ENTER && _page == HomePage::SHUTDOWN) {
      _shutdown_init = true;
      return true;
    }
    return false;
  }
};

class MsgPreviewScreen : public UIScreen {
  UITask* _task;
  mesh::RTCClock* _rtc;

  struct MsgEntry {
    uint32_t timestamp;
    char origin[62];
    char msg[78];
  };
  #define MAX_UNREAD_MSGS   32
  int num_unread;
  MsgEntry unread[MAX_UNREAD_MSGS];

public:
  MsgPreviewScreen(UITask* task, mesh::RTCClock* rtc) : _task(task), _rtc(rtc) { num_unread = 0; }

  void addPreview(uint8_t path_len, const char* from_name, const char* msg) {
    if (num_unread >= MAX_UNREAD_MSGS) return;
    auto p = &unread[num_unread++];
    p->timestamp = _rtc->getCurrentTime();
    if (path_len == 0xFF) {
      sprintf(p->origin, "(D) %s:", from_name);
    } else {
      sprintf(p->origin, "(%d) %s:", (uint32_t) path_len, from_name);
    }
    StrHelper::strncpy(p->msg, msg, sizeof(p->msg));
  }

  int render(DisplayDriver& display) override {
    char tmp[16];
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    sprintf(tmp, "Unread: %d", num_unread);
    display.print(tmp);

    auto p = &unread[0];
    int secs = _rtc->getCurrentTime() - p->timestamp;
    if (secs < 60) {
      sprintf(tmp, "%ds", secs);
    } else if (secs < 60*60) {
      sprintf(tmp, "%dm", secs / 60);
    } else {
      sprintf(tmp, "%dh", secs / (60*60));
    }
    display.setCursor(display.width() - display.getTextWidth(tmp) - 2, 0);
    display.print(tmp);

    display.drawRect(0, 11, display.width(), 1);

    display.setCursor(0, 14);
    char filtered_origin[sizeof(p->origin)];
    display.translateUTF8ToBlocks(filtered_origin, p->origin, sizeof(filtered_origin));
    display.print(filtered_origin);

    display.setCursor(0, 25);
    char filtered_msg[sizeof(p->msg)];
    display.translateUTF8ToBlocks(filtered_msg, p->msg, sizeof(filtered_msg));
    display.printWordWrap(filtered_msg, display.width());
#if AUTO_OFF_MILLIS==0
    return 10000;
#else
    return 1000;
#endif
  }

  bool handleInput(char c) override {
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      num_unread--;
      if (num_unread == 0) {
        _task->gotoHomeScreen();
      } else {
        for (int i = 0; i < num_unread; i++) {
          unread[i] = unread[i + 1];
        }
      }
      return true;
    }
    if (c == KEY_ENTER) {
      num_unread = 0;
      _task->gotoHomeScreen();
      return true;
    }
    return false;
  }
};

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _auto_off = millis() + AUTO_OFF_MILLIS;

#if defined(PIN_USER_BTN)
  user_btn.begin();
#endif
#if defined(PIN_USER_BTN_ANA)
  analog_btn.begin();
#endif

  _node_prefs = node_prefs;
  if (_display != NULL) {
    _display->turnOn();
  }

#ifdef PIN_BUZZER
  buzzer.begin();
  buzzer.quiet(_node_prefs->buzzer_quiet);
#endif

#ifdef PIN_VIBRATION
  vibration.begin();
#endif

  ui_started_at = millis();
  _alert_expiry = 0;

  splash = new SplashScreen(this);
  home = new HomeScreen(this, &rtc_clock, sensors, node_prefs);
  msg_preview = new MsgPreviewScreen(this, &rtc_clock);
  setCurrScreen(splash);
}

void UITask::showAlert(const char* text, int duration_millis) {
  strcpy(_alert, text);
  _alert_expiry = millis() + duration_millis;
}

void UITask::notify(UIEventType t) {
#if defined(PIN_BUZZER)
switch(t){
  case UIEventType::contactMessage:
    buzzer.play("MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
    break;
  case UIEventType::channelMessage:
    buzzer.play("kerplop:d=16,o=6,b=120:32g#,32c#");
    break;
  case UIEventType::ack:
    buzzer.play("ack:d=32,o=8,b=120:c");
    break;
  case UIEventType::roomMessage:
  case UIEventType::newContactMessage:
  case UIEventType::none:
  default:
    break;
}
#endif

#ifdef PIN_VIBRATION
  if (t != UIEventType::none) {
    vibration.trigger();
  }
#endif
}

void UITask::msgRead(int msgcount) {
  _msgcount = msgcount;
  if (msgcount == 0) {
    gotoHomeScreen();
  }
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) {
  _msgcount = msgcount;
  ((MsgPreviewScreen *) msg_preview)->addPreview(path_len, from_name, text);
  setCurrScreen(msg_preview);
  if (_display != NULL) {
    if (!_display->isOn()) _display->turnOn();
    _auto_off = millis() + AUTO_OFF_MILLIS;
    _next_refresh = 100;
  }
}

void UITask::userLedHandler() {
#ifdef PIN_STATUS_LED
  int cur_time = millis();
  if (cur_time > next_led_change) {
    if (led_state == 0) {
      led_state = 1;
      if (_msgcount > 0) {
        last_led_increment = LED_ON_MSG_MILLIS;
      } else {
        last_led_increment = LED_ON_MILLIS;
      }
      next_led_change = cur_time + last_led_increment;
    } else {
      led_state = 0;
      next_led_change = cur_time + LED_CYCLE_MILLIS - last_led_increment;
    }
    digitalWrite(PIN_STATUS_LED, led_state == LED_STATE_ON);
  }
#endif
}

void UITask::setCurrScreen(UIScreen* c) {
  curr = c;
  _next_refresh = 100;
}

void UITask::shutdown(bool restart){
#ifdef PIN_BUZZER
  buzzer.shutdown();
  uint32_t buzzer_timer = millis();
  while (buzzer.isPlaying() && (millis() - 2500) < buzzer_timer)
    buzzer.loop();
#endif
  if (restart) {
    _board->reboot();
  } else {
    _display->turnOff();
    radio_driver.powerOff();
    _board->powerOff();
  }
}

bool UITask::isButtonPressed() const {
#ifdef PIN_USER_BTN
  return user_btn.isPressed();
#else
  return false;
#endif
}

void UITask::loop() {
  char c = 0;
#if UI_HAS_JOYSTICK
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  }
  ev = joystick_left.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_LEFT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_LEFT);
  }
  ev = joystick_right.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_RIGHT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_RIGHT);
  }
  ev = back_btn.check();
  if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#elif defined(PIN_USER_BTN)
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_NEXT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_PREV);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#endif
#if defined(PIN_USER_BTN_ANA)
  if (abs(millis() - _analogue_pin_read_millis) > 10) {
    ev = analog_btn.check();
    if (ev == BUTTON_EVENT_CLICK) {
      c = checkDisplayOn(KEY_NEXT);
    } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      c = handleLongPress(KEY_ENTER);
    } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
      c = handleDoubleClick(KEY_PREV);
    } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
      c = handleTripleClick(KEY_SELECT);
    }
    _analogue_pin_read_millis = millis();
  }
#endif
#if defined(BACKLIGHT_BTN)
  if (millis() > next_backlight_btn_check) {
    bool touch_state = digitalRead(PIN_BUTTON2);
#if defined(DISP_BACKLIGHT)
    digitalWrite(DISP_BACKLIGHT, !touch_state);
#elif defined(EXP_PIN_BACKLIGHT)
    expander.digitalWrite(EXP_PIN_BACKLIGHT, !touch_state);
#endif
    next_backlight_btn_check = millis() + 300;
  }
#endif

  if (c != 0 && curr) {
    curr->handleInput(c);
    _auto_off = millis() + AUTO_OFF_MILLIS;
    _next_refresh = 100;
  }

  userLedHandler();

#ifdef PIN_BUZZER
  if (buzzer.isPlaying())  buzzer.loop();
#endif

  if (curr) curr->poll();

  if (_display != NULL && _display->isOn()) {
    if (millis() >= _next_refresh && curr) {
      _display->startFrame();
      int delay_millis = curr->render(*_display);
      if (millis() < _alert_expiry) {
        _display->setTextSize(1);
        int y = _display->height() / 3;
        int p = _display->height() / 32;
        _display->setColor(DisplayDriver::DARK);
        _display->fillRect(p, y, _display->width() - p*2, y);
        _display->setColor(DisplayDriver::LIGHT);
        _display->drawRect(p, y, _display->width() - p*2, y);
        _display->drawTextCentered(_display->width() / 2, y + p*3, _alert);
        _next_refresh = _alert_expiry;
      } else {
        _next_refresh = millis() + delay_millis;
      }
      _display->endFrame();
    }
#if AUTO_OFF_MILLIS > 0
    if (millis() > _auto_off) {
      _display->turnOff();
    }
#endif
  }

#ifdef PIN_VIBRATION
  vibration.loop();
#endif

#ifdef AUTO_SHUTDOWN_MILLIVOLTS
  if (millis() > next_batt_chck) {
    uint16_t milliVolts = getBattMilliVolts();
    if (milliVolts > 0 && milliVolts < AUTO_SHUTDOWN_MILLIVOLTS) {
       #if defined(THINKNODE_M1) || defined(LILYGO_TECHO)
       if (_display != NULL) {
         _display->startFrame();
         _display->setTextSize(2);
         _display->setColor(DisplayDriver::LIGHT);
         _display->drawTextCentered(_display->width() / 2, 20, "Low Battery.");
         _display->drawTextCentered(_display->width() / 2, 40, "Shutting Down!");
         _display->endFrame();
       }
       #endif
       shutdown();
      }
    next_batt_chck = millis() + 8000;
  }
#endif
}

char UITask::checkDisplayOn(char c) {
  if (_display != NULL) {
    if (!_display->isOn()) {
      _display->turnOn();
      c = 0;
    }
    _auto_off = millis() + AUTO_OFF_MILLIS;
    _next_refresh = 0;
  }
  return c;
}

char UITask::handleLongPress(char c) {
  if (millis() - ui_started_at < 8000) {
    the_mesh.enterCLIRescue();
    c = 0;
  }
  return c;
}

char UITask::handleDoubleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: double click - navigating to previous screen");
  checkDisplayOn(c);
  // ✅ Dupli klik služi samo za navigaciju nazad (KEY_PREV)
  // Nemoj menjati buzzer ovde!
  return c;
}

char UITask::handleTripleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: triple click - toggling buzzer globally");
  checkDisplayOn(c);
  // ✅ Triple click menja buzyer globalno (sa bilo kog ekrana)
  toggleBuzzer();
  c = 0;  // Poništi dalje handling - ne šalji u handleInput
  return c;
}

bool UITask::getGPSState() {
  if (_sensors != NULL) {
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        return !strcmp(_sensors->getSettingValue(i), "1");
      }
    }
  }

  return false;
}

void UITask::toggleGPS() {
    if (_sensors != NULL) {
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        if (strcmp(_sensors->getSettingValue(i), "1") == 0) {
          _sensors->setSettingValue("gps", "0");
          notify(UIEventType::ack);
          showAlert("GPS: Disabled", 800);
        } else {
          _sensors->setSettingValue("gps", "1");
          notify(UIEventType::ack);
          showAlert("GPS: Enabled", 800);
        }
        _next_refresh = 0;
        break;
      }
    }
  }
}

void UITask::toggleBuzzer() {
#ifdef PIN_BUZZER
    if (buzzer.isQuiet()) {
      buzzer.quiet(false);
      notify(UIEventType::ack);
      showAlert("Buzzer: ON", 1500);
    } else {
      buzzer.quiet(true);
      showAlert("Buzzer: OFF", 1500);
    }
    _node_prefs->buzzer_quiet = buzzer.isQuiet();
    the_mesh.savePrefs();
    _next_refresh = 0;
#endif
}
