// runtime diagnostics: boot counter, reset/restart reasons, connection and
// timing statistics. Published via MQTT (diag topic), /status and /diag.

#ifndef _DIAG_H_
#define _DIAG_H_

#include <Arduino.h>

struct DiagData {
  uint32_t boot_count;          // persistent in NVS, incremented on every boot
  const char *reset_reason;     // from esp_reset_reason(), e.g. "Brownout"
  const char *restart_reason;   // stored before the last self-initiated restart, "" if none
  uint32_t wifi_reconnects;     // successful WiFi connects after the first one
  uint32_t mqtt_reconnects;     // successful MQTT connects after the first one
  uint32_t http_err;            // failed uploads to sensor.community since boot
  unsigned long http_last_ms;   // duration of the last sensor.community upload
  unsigned long loop_max_ms;    // longest loop() since last diag_reset_loop_max()
  uint32_t seq;                 // number of the last measurement record (0 = none yet)
  uint32_t wifi_disconnects;    // WiFi disconnect events reported by the ESP32 since boot
  uint8_t wifi_disc_reason;     // reason code of the last disconnect (0 = none yet), e.g. 200 = beacon timeout
};

// Text for a WiFi disconnect reason code, e.g. "BEACON_TIMEOUT".
const char *diag_wifi_reason_name(uint8_t reason);

// Call as early as possible in setup().
void setup_diag();

// Scan once and log all access points with RSSI (hardware/antenna check at boot).
void diag_wifi_scan();

// Store a reason in NVS that will be reported after the next boot.
// Use before restarts initiated by the firmware or by libraries (OTA).
void diag_set_restart_reason(const char *reason);
// Store reason, then restart.
void diag_restart(const char *reason);

// Übernimmt die gemessene loop()-Dauer und merkt sich das Maximum (loop_max_ms).
void diag_loop_duration(unsigned long ms);
// Startet neues Messfenster für die längste loop()-Dauer.
void diag_reset_loop_max();
// Zählt WiFi-Reconnects; die erste Verbindung zählt nicht.
void diag_wifi_connected();
// Zählt MQTT-Reconnects; die erste Verbindung zählt nicht.
void diag_mqtt_connected();
// Markiert Uploadfehler und speichert Uploaddauer in ms.
void diag_http_result(bool ok, unsigned long duration_ms);
// Erzeugt fortlaufende Messungsnummer (seq).
uint32_t diag_next_seq();

const DiagData &diag_get();

// Format all diagnostic values as JSON. Returns snprintf() result.
int diag_json(char *buf, size_t len, int mqtt_state);

#endif // _DIAG_H_
