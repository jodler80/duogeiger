// Project: Simple Multi-Geiger
// (c) 2019,2020 by the authors, see AUTHORS file in toplevel directory.
// Licensed under the GPL v3 (or later), see LICENSE file in toplevel directory.

#include <Arduino.h>

#include "version.h"
#include "log.h"
#include "log_data.h"
#include "userdefines.h"
#include "thp_sensor.h"
#include "tube.h"
#include "switches.h"
#include "speaker.h"
#include "webconf.h"
#include "display.h"
#include "transmission.h"
#include "ble.h"
#include "clock.h"

// Measurement interval (default 2.5min) [sec]
#define MEASUREMENT_INTERVAL 150

// Max time the greeting display will be on. [sec]
#define AFTERSTART 5

// Basic heartbeat for serial log, BLE and OLED refresh [sec]
#define HEARTBEAT_INTERVAL 10

// Target loop duration [ms]
// slow down the arduino main loop so it spins about once per LOOP_DURATION -
#define LOOP_DURATION 1000

// DIP switches
static Switches switches;
float accumulated_count_rate = 0.0, accumulated_dose_rate = 0.0;

void setup() {
  setup_log(DEFAULT_LOG_LEVEL);
  setup_display();
  setup_switches();
  switches = read_switches();  // only read DIP switches once at boot time
  setup_thp_sensor();
  setup_webconf();
  setup_speaker(playSound, ledTick && switches.led_on, speakerTick && switches.speaker_on);
  setup_transmission(VERSION_STR, iotWebConf.getThingName());
  setup_ble(ssid, sendToBle && switches.ble_on);
  setup_log_data(SERIAL_DEBUG);
  setup_tube();
}

void setup_ntp(int wifi_status) {
  static bool clock_configured = false;
  if (clock_configured)
    return;

  // this is called from loop() because we do not want to block in setup().
  // it might either take a while until WiFi is ready or it might even never
  // be ready, e.g. in LoRa-based deployments or on the road using BLE.

  // a bug in arduino-esp32 1.0.4 crashes the esp32 if the wifi is not
  // configured yet and one tries to configure for NTP:
  if (wifi_status != ST_WIFI_CONNECTED)
    return;

  setup_clock(0);  // 0 == do NTP!
  // please note that time is not necessarily NTP-correct below here.
  // it might take some minutes until we have the correct time.
  // if we do not have a connection to NTP servers, it will just count up from 1970.

  clock_configured = true;
}

int update_wifi_status(void) {
  int st;
  switch (iotWebConf.getState()) {
  case iotwebconf::Connecting:
    st = ST_WIFI_CONNECTING;
    break;
  case iotwebconf::OnLine:
    st = ST_WIFI_CONNECTED;
    break;
  case iotwebconf::ApMode:
    st = ST_WIFI_AP;
    break;
  default:
    st = ST_WIFI_OFF;
    break;
  }
  set_status(STATUS_WIFI, st);
  return st;
}

int update_ble_status(void) {  // currently no error detection
  int st;
  if (sendToBle && switches.ble_on)
    st = is_ble_connected() ? ST_BLE_CONNECTED : ST_BLE_CONNECTABLE;
  else
    st = ST_BLE_OFF;
  set_status(STATUS_BLE, st);
  return st;
}

void read_THP(unsigned long current_ms,
              bool *have_thp, float *temperature, float *humidity, float *pressure, int *iaq) {
  static unsigned long last_timestamp = 0;
  // first call: immediately query thp sensor
  // subsequent calls: only query every BME680_BSEC_LP_READOUT_INTERVAL (BSEC low-power cadence, ~3s)
  if (!last_timestamp || ((current_ms - last_timestamp) >= BME680_BSEC_LP_READOUT_INTERVAL)) {
    *have_thp = read_thp_sensor(temperature, humidity, pressure, iaq);
    last_timestamp = current_ms;
  }
}

void process_GMC(unsigned long current_ms, unsigned long current_counts, unsigned long gm_count_timestamp, unsigned long current_hv_pulses,
                 bool have_thp, float temperature, float humidity, float pressure, int iaq, int wifi_status) {
  struct GM_State {
    unsigned long timestamp;
    unsigned long counts;
    unsigned long last_count_timestamp;
    unsigned long hv_pulses;
  };

  // Events
  // 1. add new incremental status const for new event
  const byte HEARTBEAT = 0;
  const byte MEASUREMENT = 1;
  const byte ONE_MINUTE = 2;
  const byte TELEGRAM_DATA = 3;
  const byte MQTT_DATA = 4;

  // 2. adjust the total number of events resp. saved states
  const int savedstates_count = 5;

  // 3. add interval in ms to the end of interval array - 4. see below
  static GM_State saved_state[savedstates_count];
  long event_interval[savedstates_count] = {
    HEARTBEAT_INTERVAL * 1000,  // Basic heartbeat interval
    MEASUREMENT_INTERVAL * 1000,  // Send measurements to server interval
    60 * 1000,  // 60 sec for ONE_MINUTE log
    sendDataToMessengerEvery * 1000,  // Send to Telegram messenger interval (configurable)
    sendDataToMqttEvery * 1000,  // Send to MQTT interval (configurable)
  };

  // millis(), counts or hv_pulses overflow?
  for (int i = 0; i < savedstates_count; i++) {
    if (current_ms < saved_state[i].timestamp) saved_state[i].timestamp = current_ms;
    if (current_counts < saved_state[i].counts) saved_state[i].counts = current_counts;
    if (gm_count_timestamp < saved_state[i].last_count_timestamp) saved_state[i].last_count_timestamp = gm_count_timestamp;
    if (current_hv_pulses < saved_state[i].hv_pulses) saved_state[i].hv_pulses = current_hv_pulses;
  }

  static unsigned int accumulated_GMC_counts = 0;
  // 64-bit so this doesn't wrap at 2^32 ms (~49.7 d uptime) and cause a huge
  // bogus accumulated_count_rate / accumulated_dose_rate -> spurious local alarm.
  static uint64_t accumulated_time = 0;

  // Startup
  if ((gm_count_timestamp == 0) && (saved_state[HEARTBEAT].last_count_timestamp == 0)) {
    // If there were no pulses after AFTERSTART msecs after boot, clear display anyway and show 0 counts.
    static unsigned long boot_timestamp = millis();
    static unsigned long afterStartTime = AFTERSTART * 1000;
    if (afterStartTime && ((current_ms - boot_timestamp) >= afterStartTime)) {
      afterStartTime = 0;
      update_bledata(0, 0, 0, 0, 0);
      display_GMC(0, 0, 0, (showDisplay && switches.display_on));
    }
    return;
  }

  // Heartbeat; all other events snap to the next heartbeat interval
  if ((current_ms - saved_state[HEARTBEAT].timestamp) >= event_interval[HEARTBEAT]) {
    unsigned long counts = current_counts - saved_state[HEARTBEAT].counts;
    int dt = gm_count_timestamp - saved_state[HEARTBEAT].last_count_timestamp;
    int hv_pulses = current_hv_pulses - saved_state[HEARTBEAT].hv_pulses;
    accumulated_time += dt;
    accumulated_GMC_counts = current_counts;
    float GMC_factor_uSvph = tubes[TUBE_TYPE].cps_to_uSvph;

    // calculate the current count rate and dose rate
    float count_rate = (dt != 0) ? (float)counts * 1000.0 / (float)dt : 0.0;
    unsigned int current_cpm = (int)(count_rate * 60);
    float dose_rate = count_rate * GMC_factor_uSvph;

    // calculate the count rate and dose rate over the complete time from start
    accumulated_count_rate = (accumulated_time != 0) ? (float)accumulated_GMC_counts * 1000.0 / (float)accumulated_time : 0.0;
    accumulated_dose_rate = accumulated_count_rate * GMC_factor_uSvph;

    // Serial logging
    if (Serial_Print_Mode == Serial_Logging) {
      log_data(counts, dt, count_rate, dose_rate, hv_pulses,
               accumulated_GMC_counts, (unsigned long)(accumulated_time / 1000), accumulated_count_rate, accumulated_dose_rate,
               temperature, humidity, pressure, iaq);
    }

    // Update data on display and notify via BLE
    display_GMC((unsigned int)(accumulated_time / 1000), (int)(accumulated_dose_rate * 1000), (int)(count_rate * 60),
                (showDisplay && switches.display_on));
    update_bledata((unsigned int)(count_rate * 60), temperature, humidity, pressure, iaq);

    // Sound local alarm?
    if ((soundLocalAlarm || sendLocalAlarmToMessenger) && GMC_factor_uSvph > 0) {
      bool isAlarm = false;
      if (accumulated_dose_rate > localAlarmThreshold) {
        log(WARNING, "Local alarm: Accumulated dose of %.3f µSv/h above threshold at %.3f µSv/h", accumulated_dose_rate, localAlarmThreshold);
        isAlarm = true;
      }
      if (dose_rate > (accumulated_dose_rate * localAlarmFactor)) {
        log(WARNING, "Local alarm: Current dose of %.3f > %d x accumulated dose of %.3f µSv/h", dose_rate, localAlarmFactor, accumulated_dose_rate);
        isAlarm = true;
      }
      if (isAlarm) {
        if (soundLocalAlarm)
          alarm();
        if (sendLocalAlarmToMessenger)
          transmit_data_to_telegram(tubes[TUBE_TYPE].type, tubes[TUBE_TYPE].nbr, tubes[TUBE_TYPE].cps_to_uSvph,
                                    (unsigned int)(count_rate * 60), (unsigned int)(accumulated_count_rate * 60), accumulated_dose_rate,
                                    have_thp, temperature, humidity, pressure, wifi_status, true);
      }
    }
    saved_state[HEARTBEAT].timestamp = current_ms;
    saved_state[HEARTBEAT].counts = current_counts;
    saved_state[HEARTBEAT].last_count_timestamp = gm_count_timestamp;
    saved_state[HEARTBEAT].hv_pulses = current_hv_pulses;

    // Handle all the other events
    for (int st = 1; st < savedstates_count; st++) {
      if ((current_ms - saved_state[st].timestamp) < event_interval[st])
        continue;
      // adapt difference data to event's last saved state
      counts = current_counts - saved_state[st].counts;
      dt = gm_count_timestamp - saved_state[st].last_count_timestamp;
      hv_pulses = current_hv_pulses - saved_state[st].hv_pulses;
      count_rate = (dt != 0) ? (float)counts * 1000.0 / (float)dt : 0.0;
      current_cpm = (int)(count_rate * 60);
      dose_rate = count_rate * GMC_factor_uSvph;

      // 4. (s. above) add commands for new event to switch case structure. That's it.
      switch (st) {
      case MEASUREMENT:
        log(DEBUG, "Measured GM: cpm= %d HV=%d", current_cpm, hv_pulses);
        transmit_data_to_web(tubes[TUBE_TYPE].type, tubes[TUBE_TYPE].nbr, dt, hv_pulses, counts, current_cpm,
                             have_thp, temperature, humidity, pressure, wifi_status);
        // if (sendToLora)
        //   transmit_data_to_ttn(tubes[TUBE_TYPE].type, tubes[TUBE_TYPE].nbr, dt, hv_pulses, counts, current_cpm,
        //                        have_thp, temperature, humidity, pressure);
        break;
      case ONE_MINUTE:
        if (Serial_Print_Mode == Serial_One_Minute_Log)
          log_data_one_minute((current_ms / 1000), current_cpm, counts);
        break;
      case TELEGRAM_DATA:
        if (sendDataToMessengerEvery > 0) {
          log(DEBUG, "Sending data to Telegram messenger");
          transmit_data_to_telegram(tubes[TUBE_TYPE].type, tubes[TUBE_TYPE].nbr, tubes[TUBE_TYPE].cps_to_uSvph,
                                    current_cpm, (unsigned int)(accumulated_count_rate * 60), accumulated_dose_rate,
                                    have_thp, temperature, humidity, pressure, wifi_status, false);
        }
        break;
      case MQTT_DATA:
        if (sendDataToMqttEvery > 0) {
          log(DEBUG, "Sending to MQTT");
          transmit_data_to_mqtt(tubes[TUBE_TYPE].type, tubes[TUBE_TYPE].nbr, tubes[TUBE_TYPE].cps_to_uSvph,
                                current_cpm, (unsigned int)(accumulated_count_rate * 60), accumulated_dose_rate,
                                have_thp, temperature, humidity, pressure, wifi_status, false);
        }
        break;
      default:
        break;
      }
      saved_state[st].timestamp = current_ms;
      saved_state[st].counts = current_counts;
      saved_state[st].last_count_timestamp = gm_count_timestamp;
      saved_state[st].hv_pulses = current_hv_pulses;
    }
  }
}

void loop() {
  static bool hv_error = false;  // true means a HV capacitor charging issue

  static bool have_thp = false;
  static float temperature = 0.0, humidity = 0.0, pressure = 0.0;
  static int iaq = 0;

  unsigned long current_ms = millis();  // to save multiple calls to millis()

  // this is the always increasing HV pulse master counter.
  // main program: all other hv pulse counter values shall be derived from it.
  static unsigned long hv_pulses = 0;

  // this is the always increasing geiger mueller master counter.
  // main program: all other counter values for misc. purposes shall be derived from it.
  // ISR code: counters and other values there should be only short-lived and only be
  //           used to update values in main program.
  static unsigned long gm_counts = 0;

  // this is the master timestamp of the last geiger mueller event [ms]
  // main program: all other timestamp bookkeeping values shall be derived from it.
  // ISR code: only one timestamp shall be kept/updated there with the only purpose
  //           of being used to update the master timestamp.
  static unsigned long gm_count_timestamp;

  // time between last 2 geiger mueller events [us]
  unsigned int gm_count_time_between;

  update_ble_status();
  int wifi_status = update_wifi_status();
  setup_ntp(wifi_status);

  read_hv(&hv_error, &hv_pulses);
  set_status(STATUS_HV, hv_error ? ST_HV_ERROR : ST_HV_OK);

  read_GMC(&gm_counts, &gm_count_timestamp, &gm_count_time_between);
  read_THP(current_ms, &have_thp, &temperature, &humidity, &pressure, &iaq);

  process_GMC(current_ms, gm_counts, gm_count_timestamp, hv_pulses, have_thp, temperature, humidity, pressure, iaq, wifi_status);

  // Debug log for last interval between counts, 1x per LOOP_DURATION
  if (Serial_Print_Mode == Serial_Statistics_Log) {
    static unsigned long last_counts = 0;
    if (gm_counts != last_counts) {
      log_data_statistics(gm_count_time_between);
      last_counts = gm_counts;
    }
  }

  // do any other periodic updates for LoRaWAN(tm) uplinks
  poll_transmission(wifi_status);

  long current_loop_duration;
  current_loop_duration = millis() - current_ms;
  iotWebConf.delay((current_loop_duration < LOOP_DURATION) ? (LOOP_DURATION - current_loop_duration) : 0);
}
