// Sammelt Laufzeit-, WLAN-/MQTT- und Neustartstatistik für Diagnoseansichten.
// runtime diagnostics, see diag.h

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_system.h>

#include "log.h"
#include "diag.h"
#include "clock.h"

// NVS-Namespace und Schlüssel für Boot-Zähler und geplanten Neustartgrund.
#define DIAG_NVS_NAMESPACE "diag"
#define DIAG_KEY_BOOTS "boots"
#define DIAG_KEY_REASON "rreason"

static DiagData d = {0, "", "", 0, 0, 0, 0, 0, 0, 0, 0};
static char restart_reason_buf[48] = "";
static uint32_t wifi_connects = 0, mqtt_connects = 0;

const char *diag_wifi_reason_name(uint8_t reason);

// Wandelt ESP-Resetgründe in kurze Namen für Logs/Diagnose-JSON.
static const char *reset_reason_str(esp_reset_reason_t r) {
  switch (r) {
  case ESP_RST_POWERON:   return "PowerOn";
  case ESP_RST_EXT:       return "ExternalPin";
  case ESP_RST_SW:        return "Software";
  case ESP_RST_PANIC:     return "Panic";
  case ESP_RST_INT_WDT:   return "InterruptWatchdog";
  case ESP_RST_TASK_WDT:  return "TaskWatchdog";
  case ESP_RST_WDT:       return "OtherWatchdog";
  case ESP_RST_DEEPSLEEP: return "DeepSleep";
  case ESP_RST_BROWNOUT:  return "Brownout";
  case ESP_RST_SDIO:      return "SDIO";
  default:                return "Unknown";
  }
}

// Initialisiert Diagnosezustand: Resetgrund, Boot-Zähler aus NVS, optionaler Neustartgrund.
// Called by the WiFi driver task on every disconnect: only store, no logging here
// (logging happens in the main task, see diag_wifi_connected()).
static void on_wifi_disconnected(arduino_event_id_t event, arduino_event_info_t info) {
  d.wifi_disc_reason = info.wifi_sta_disconnected.reason;
  d.wifi_disconnects++;
  // Also fires for every failed connection attempt -> visible on the serial console.
  // Serial output from the WiFi task is acceptable here (short, rare).
  Serial.printf("GEIGER: WiFi disconnected/failed: reason %u (%s), RSSI %d\n",
                d.wifi_disc_reason, diag_wifi_reason_name(d.wifi_disc_reason), WiFi.RSSI());
}

// Hardware check: scan once at boot and list every access point with its signal strength.
// Compare with another device at the same spot to judge the ESP32 antenna.
void diag_wifi_scan() {
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks(false, true);  // blocking, include hidden networks
  log(INFO, "WiFi scan: %d access points", n);
  for (int i = 0; i < n; i++)
    log(INFO, "  %-32s %s ch %2d RSSI %4d dBm", WiFi.SSID(i).c_str(), WiFi.BSSIDstr(i).c_str(),
        WiFi.channel(i), WiFi.RSSI(i));
  WiFi.scanDelete();
}

const char *diag_wifi_reason_name(uint8_t reason) {
  return reason ? WiFi.disconnectReasonName((wifi_err_reason_t)reason) : "-";
}

void setup_diag() {
  WiFi.onEvent(on_wifi_disconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  d.reset_reason = reset_reason_str(esp_reset_reason());

  Preferences prefs;
  if (prefs.begin(DIAG_NVS_NAMESPACE, false)) {
    d.boot_count = prefs.getUInt(DIAG_KEY_BOOTS, 0) + 1;
    prefs.putUInt(DIAG_KEY_BOOTS, d.boot_count);
    if (prefs.isKey(DIAG_KEY_REASON)) {
      prefs.getString(DIAG_KEY_REASON, restart_reason_buf, sizeof(restart_reason_buf));
      prefs.remove(DIAG_KEY_REASON);  // report only once
    }
    prefs.end();
  }
  d.restart_reason = restart_reason_buf;
  log(INFO, "Boot #%u, reset reason: %s, restart reason: %s",
      d.boot_count, d.reset_reason, restart_reason_buf[0] ? restart_reason_buf : "-");
}

// Speichert Grund für geplanten Neustart im NVS, damit er nach Reset sichtbar ist.
void diag_set_restart_reason(const char *reason) {
  Preferences prefs;
  if (prefs.begin(DIAG_NVS_NAMESPACE, false)) {
    prefs.putString(DIAG_KEY_REASON, reason);
    prefs.end();
  }
}

// Merkt sich Neustartgrund und startet ESP-Reset.
void diag_restart(const char *reason) {
  log(WARNING, "Restarting: %s", reason);
  diag_set_restart_reason(reason);
  delay(200);  // give serial/syslog a moment
  ESP.restart();
}

// Merkt sich die längste main loop Laufzeit in Millisekunden.
void diag_loop_duration(unsigned long ms) {
  if (ms > d.loop_max_ms)
    d.loop_max_ms = ms;
}

// Setzt die Maximalmessung für loop-Laufzeit zurück.
void diag_reset_loop_max() {
  d.loop_max_ms = 0;
}

// Zählt WLAN-Verbindungen; ab zweitem Erfolg werden Reconnects registriert.
void diag_wifi_connected() {
  if (++wifi_connects > 1) {
    d.wifi_reconnects = wifi_connects - 1;
    log(WARNING, "WiFi reconnected (#%u); last disconnect reason %u (%s), %u disconnects since boot",
        d.wifi_reconnects, d.wifi_disc_reason, diag_wifi_reason_name(d.wifi_disc_reason), d.wifi_disconnects);
  }
}

// Zählt MQTT-Verbindungen; ab zweitem Erfolg werden Reconnects registriert.
void diag_mqtt_connected() {
  if (++mqtt_connects > 1)
    d.mqtt_reconnects = mqtt_connects - 1;
}

// Protokolliert Dauer und Fehler bei HTTP-Übertragungen.
void diag_http_result(bool ok, unsigned long duration_ms) {
  d.http_last_ms = duration_ms;
  if (!ok)
    d.http_err++;
}

// Liefert die nächste laufende Messungsnummer (Feld "seq" im MQTT-JSON).
uint32_t diag_next_seq() {
  return ++d.seq;
}

// Gibt Diagnosewerte read-only als Referenz zurück.
const DiagData &diag_get() {
  return d;
}

// Erzeugt JSON-Statusobjekt mit Laufzeit, WLAN, MQTT und Speicherwerten.
int diag_json(char *buf, size_t len, int mqtt_state) {
  // WLAN-Details werden nur gelesen, wenn eine Verbindung besteht.
  bool wifi_up = (WiFi.status() == WL_CONNECTED);
  return snprintf(buf, len,
                  "{\"seq\":%u,\"uptime_s\":%lu,\"boot_count\":%u,\"ntp_ok\":%s,"
                  "\"reset_reason\":\"%s\",\"restart_reason\":\"%s\","
                  "\"rssi\":%d,\"channel\":%d,\"bssid\":\"%s\","
                  "\"wifi_reconnects\":%u,\"wifi_disconnects\":%u,\"wifi_disc_reason\":%u,"
                  "\"wifi_disc_reason_name\":\"%s\",\"mqtt_reconnects\":%u,\"mqtt_state\":%d,"
                  "\"heap_free\":%u,\"heap_min\":%u,\"heap_maxblock\":%u,"
                  "\"loop_max_ms\":%lu,\"http_err\":%u,\"http_last_ms\":%lu}",
                  d.seq, (unsigned long)(millis() / 1000), d.boot_count,
                  clock_is_synced() ? "true" : "false",
                  d.reset_reason, d.restart_reason,
                  wifi_up ? WiFi.RSSI() : 0, wifi_up ? WiFi.channel() : 0,
                  wifi_up ? WiFi.BSSIDstr().c_str() : "",
                  d.wifi_reconnects, d.wifi_disconnects, d.wifi_disc_reason,
                  diag_wifi_reason_name(d.wifi_disc_reason), d.mqtt_reconnects, mqtt_state,
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(),
                  d.loop_max_ms, d.http_err, d.http_last_ms);
}
