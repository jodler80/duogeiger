// Verwaltet Systemzeit über NTP/manuelle Vorgabe und liefert UTC-Stempel.
#include <Arduino.h>
#include <sys/time.h>
#include <esp_sntp.h>

#include "userdefines.h"
#include "log.h"
#include "clock.h"

// volatile, weil SNTP-Task die Zeit synchronisiert.
static volatile bool time_synced = false;

static void ntp_sync_cb(struct timeval *tv) {
  // called from the lwIP/SNTP task on every successful sync (first and periodic)
  bool first = !time_synced;
  time_synced = true;
  if (first)
    log(INFO, "NTP: time synchronized (%s UTC)", utctime());
}

// Own NTP server; the SNTP client keeps a pointer to it, so it must stay valid.
static char ntp_server[64] = NTP_SERVER;
static bool ntp_running = false;

// Startet (oder startet neu) den SNTP-Client: eigener Server zuerst, dann die öffentlichen.
static void start_ntp() {
  sntp_set_time_sync_notification_cb(ntp_sync_cb);
  if (ntp_server[0] != '\0') {
    configTime(TZ_OFFSET, DST_OFFSET, ntp_server, NTP_FALLBACK_1, NTP_FALLBACK_2);
    log(INFO, "NTP: servers %s, %s, %s", ntp_server, NTP_FALLBACK_1, NTP_FALLBACK_2);
  } else {
    configTime(TZ_OFFSET, DST_OFFSET, NTP_FALLBACK_1, NTP_FALLBACK_2);
    log(INFO, "NTP: servers %s, %s", NTP_FALLBACK_1, NTP_FALLBACK_2);
  }
  ntp_running = true;
}

// Übernimmt den NTP-Server aus der Webkonfiguration; läuft NTP schon, wird sofort neu gestartet.
void clock_set_ntp_server(const char *server) {
  if (strncmp(ntp_server, server ? server : "", sizeof(ntp_server)) == 0)
    return;  // unchanged
  strncpy(ntp_server, server ? server : "", sizeof(ntp_server) - 1);
  ntp_server[sizeof(ntp_server) - 1] = '\0';
  if (ntp_running)
    start_ntp();  // configTime() stops and restarts the SNTP client
}

// Setzt feste Testzeit oder startet NTP mit Zeitzone/Fallback-Servern.
void config_time(time_t timestamp) {
  if (timestamp > 0) {
    // a specific timestamp was given, e.g. for testing purposes, set time:
    struct timeval tv = {timestamp, 0};
    settimeofday(&tv, nullptr);
    time_synced = true;
  } else {
    // timestamp == 0 means we shall use NTP to get the time.
    // please note that this just sets up NTP. the actual time sync might take some minutes...
    start_ntp();
  }
}

bool clock_is_synced(void) {
  return time_synced;
}

// Liefert aktuellen UTC-String; verwendet statischen internen Puffer.
char *utctime(void) {
  // return a pointer to a timestamp string like 2019-12-31T23:59:59
  time_t t;
  struct tm *ti;
  static char buffer[20];

  time(&t);
  ti = gmtime(&t);
  strftime(buffer, 20, "%Y-%m-%dT%H:%M:%S", ti);
  return buffer;
}


// Initialisiert Zeitquelle für Logs und Zeitstempel.
void setup_clock(time_t timestamp) {
  config_time(timestamp);
}
