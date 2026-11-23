// Zeit- und NTP-Helfer für synchronisierte Zeitstempel.
#ifndef CLOCK_H
#define CLOCK_H

#include <time.h>
#include "userdefines.h"

// NTP servers. The ESP32 SNTP client rotates through up to 3 servers on its own:
// if a server does not answer, the next one is tried (and so on, round robin).
// NTP_SERVER (userdefines.h) is tried first, e.g. your router ("fritz.box" or its IP).
// Empty string = public servers only.
#ifndef NTP_SERVER
#define NTP_SERVER ""
#endif
#define NTP_FALLBACK_1 "pool.ntp.org"
#define NTP_FALLBACK_2 "ptbtime1.ptb.de"

#define TZ_OFFSET 0  // timezone offset from UTC [s]
#define DST_OFFSET 0  // DST offset from NONDST [s]

// timestamp == 0 -> NTP wanted, otherwise just set the clock.
void setup_clock(time_t timestamp);

// return a iso-8601-like utc timestamp
char *utctime(void);

// true once the clock has been set (NTP synced or set explicitly)
bool clock_is_synced(void);

// Set the own NTP server (from the web configuration). "" = public servers only.
// If NTP is already running, it is restarted with the new server list.
void clock_set_ntp_server(const char *server);

#endif
