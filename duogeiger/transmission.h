// measurements data transmission related code
// - via WiFi to internet servers
// - via LoRa to TTN (to internet servers)

#ifndef _TRANSMISSION_H_
#define _TRANSMISSION_H_

// Sensor-PINS.
// They are called PIN, because in the first days of Feinstaub sensor they were
// really the CPU-Pins. Now they are 'virtual' pins to distinguish different sensors.
// Since we send to sensor.community, we have to use their numbers.
// PIN number 0 doesn't exist, so we use it to disable the X-PIN header.
#define XPIN_NO_XPIN 0
#define XPIN_RADIATION 19
#define XPIN_BME280 11

// Last completed measurement cycle — used by the /status web page.
struct MeasurementData {
  bool valid;
  unsigned int cpm;
  unsigned int counts;
  unsigned int hv_pulses;
  unsigned int sample_ms;
  float dose_nsvph;
  bool have_thp;
  float temperature;
  float humidity;
  float pressure;
};

const MeasurementData& get_last_measurement();
bool is_mqtt_connected();
int get_mqtt_state();          // PubSubClient::state(), -99 if no client
int get_mqtt_backlog_count();  // measurement records waiting to be re-sent

// Number of measurement records kept while MQTT is unreachable (RAM ring buffer).
// 48 x 150 s = 2 h. Each record needs ~70 bytes.
#ifndef MQTT_BACKLOG_SIZE
#define MQTT_BACKLOG_SIZE 48
#endif

// Radiation status helpers (radiation_status_level/_label/_label_html/_color),
// record type and formatting — pure logic, unit-tested on the PC.
#include "mqtt_logic.h"

// Startet konfigurierte Übertragungswege.
void setup_transmission(const char *version, const char *device_name);
// Sendet Messwerte über HTTP/WLAN an Webdienste.
void transmit_data_to_web(const char *tube_type, int tube_nbr, unsigned int dt, unsigned int hv_pulses, unsigned int gm_counts, unsigned int cpm,
                   int have_thp, float temperature, float humidity, float pressure, int wifi_status);
// Sendet Mess-/Alarmbericht an Telegram, wenn aktiv.
void transmit_data_to_telegram(const char *tube_type, int tube_nbr, float tube_factor, unsigned int cpm, unsigned int accu_cpm, float accu_rate,
                       int have_thp, float temperature, float humidity, float pressure, int wifi_status, bool alarm_status);
// Publiziert Messung über MQTT; nutzt Ausfallpuffer.
void transmit_data_to_mqtt(const char *tube_type, int tube_nbr, float tube_factor, unsigned int cpm, unsigned int accu_cpm, float accu_rate,
                       unsigned int dt, unsigned int hv_pulses, unsigned int gm_counts,
                       int have_thp, float temperature, float humidity, float pressure, int wifi_status, bool alarm_status);

// Periodic keep-alive functions for transmissions:
// - On Lora boards, the Arduino LMIC wants to be polled from loop().
// - MQTT, if active, requires a periodic keep-alive
void poll_transmission(int wifi_status);

#endif // _TRANSMISSION_H_
