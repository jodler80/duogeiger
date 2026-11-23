// low level log() call - outputs to serial/usb and, optionally, to a remote syslog server (UDP)

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "log.h"
#include "clock.h"
#include "userdefines.h"

// Remote syslog: set SYSLOG_SERVER in userdefines.h to an IP/hostname, e.g. "192.168.178.10".
// Empty string = off.
#ifndef SYSLOG_SERVER
#define SYSLOG_SERVER ""
#endif
#ifndef SYSLOG_PORT
#define SYSLOG_PORT 514
#endif

// the GEIGER: prefix is is to easily differentiate our output from other esp32 output (e.g. wifi messages)
#define LOG_PREFIX_FORMAT "GEIGER: %s "
#define LOG_PREFIX_LEN (7+1+19+1)  // chars, without the terminating \0

static int log_level = NOLOG;  // messages at level >= log_level will be output

static char syslog_hostname[33] = "duogeiger";
static WiFiUDP syslog_udp;
static SemaphoreHandle_t syslog_mutex = nullptr;

// Setzt Hostname für Syslog-Nachrichten.
void log_set_hostname(const char *hostname) {
  strncpy(syslog_hostname, hostname, sizeof(syslog_hostname) - 1);
  syslog_hostname[sizeof(syslog_hostname) - 1] = '\0';
}

// Sendet Lognachricht als UDP-Syslog, falls aktiviert und WLAN verbunden.
static void syslog_send(int level, const char *msg) {
  if (SYSLOG_SERVER[0] == '\0' || WiFi.status() != WL_CONNECTED || !syslog_mutex)
    return;
  // log() may be called from other tasks (e.g. BLE): never block, just skip if busy
  if (xSemaphoreTake(syslog_mutex, 0) != pdTRUE)
    return;
  static const int severity[] = {7, 6, 4, 3, 2};  // DEBUG, INFO, WARNING, ERROR, CRITICAL
  int sev = (level >= DEBUG && level <= CRITICAL) ? severity[level] : 6;
  int pri = 16 * 8 + sev;  // facility local0
  if (syslog_udp.beginPacket(SYSLOG_SERVER, SYSLOG_PORT)) {
    syslog_udp.printf("<%d>%s duogeiger: %s", pri, syslog_hostname, msg);
    syslog_udp.endPacket();
  }
  xSemaphoreGive(syslog_mutex);
}

// Filtert nach Level, formatiert Prefix/Zeit und sendet seriell plus Syslog.
void log(int level, const char *format, ...) {
  if (level < log_level)
    return;

  va_list args;
  va_start(args, format);
  // vsnprintf(NULL, 0) zählt nur die Ausgabezeichen.
  char buf[vsnprintf(NULL, 0, format, args) + 1 + LOG_PREFIX_LEN];
  va_end(args);
  // args wurde durch vsnprintf verbraucht; va_start muss erneut laufen.
  va_start(args, format);
  sprintf(buf, LOG_PREFIX_FORMAT, utctime());
  vsprintf(buf + LOG_PREFIX_LEN, format, args);
  va_end(args);
  Serial.println(buf);
  syslog_send(level, buf + LOG_PREFIX_LEN);
}

// Initialisiert Serial, Syslog-Mutex und aktiviert Log-Level.
void setup_log(int level) {
  Serial.begin(115200);
  while (!Serial) {};
  syslog_mutex = xSemaphoreCreateMutex();
  log(NOLOG, "Logging initialized at level %d.", level);  // this will always be output
  log_level = level;
}
