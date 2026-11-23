// Pure logic used by the MQTT transmission, kept free of Arduino/ESP32 dependencies
// so it can be unit-tested on a PC (see test/host/).
//
// - radiation status classification (thresholds, labels, colors)
// - measurement record + ring buffer for the offline backlog
// - JSON formatting of a measurement record
// - MQTT base topic construction
//
// Everything that depends on the hardware clock is passed in as a parameter
// (now_ms = millis(), now_epoch = time(nullptr)).

#ifndef _MQTT_LOGIC_H_
#define _MQTT_LOGIC_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

// ---------------------------------------------------------------------------
// Radiation status
// ---------------------------------------------------------------------------

// Thresholds in nSv/h (1 µSv/h = 1000 nSv/h), single source of truth.
// Reference: https://de.wikipedia.org/wiki/Radiologischer_Notfall#Dosis-Eckwerte
inline int radiation_status_level(float dose_nsvph) {
  if      (dose_nsvph <   1500.0f) return 0;
  else if (dose_nsvph <  10000.0f) return 1;
  else if (dose_nsvph <  80000.0f) return 2;
  else if (dose_nsvph < 350000.0f) return 3;
  else                             return 4;
}

// plain ASCII text, e.g. "Erhoht" (MQTT)
inline const char *radiation_status_label(float dose_nsvph) {
  static const char *const label[5] = {
    "Normal", "Erhoht", "Deutlich erhoht", "Hohe Strahlung", "STRAHLUNGSALARM"
  };
  return label[radiation_status_level(dose_nsvph)];
}

// HTML with entities, e.g. "Erh&ouml;ht" (status page)
inline const char *radiation_status_label_html(float dose_nsvph) {
  static const char *const label[5] = {
    "Normal", "Erh&ouml;ht", "Deutlich erh&ouml;ht", "Hohe Strahlung", "&#9762; STRAHLUNGSALARM"
  };
  return label[radiation_status_level(dose_nsvph)];
}

// CSS hex color string
inline const char *radiation_status_color(float dose_nsvph) {
  static const char *const color[5] = {
    "#2a7a2a", "#c8a000", "#c85000", "#b00000", "#5a0000"
  };
  return color[radiation_status_level(dose_nsvph)];
}

// ---------------------------------------------------------------------------
// Measurement record and backlog ring buffer
// ---------------------------------------------------------------------------

struct MqttRecord {
  // seq: fortlaufende Nummer; hilft, verlorene MQTT-Meldungen zu erkennen.
  uint32_t seq;
  unsigned long ms;        // millis() when the record was created
  time_t ts;               // UTC epoch when created, 0 if clock was not synced yet
  // counts: Zählrohr-Impulse im Messfenster; cpm = Counts per Minute.
  unsigned int cpm, counts, hv_pulses;
  // dose_nsvph: Momentanwert; accu_nsvph: Schnitt/Akkumulationsmittelwert; sample_s: Messfenster.
  float dose_nsvph, accu_nsvph, sample_s;
  // have_thp: false, wenn Temperatur/Feuchte/Druck nicht gültig gemessen wurden.
  bool have_thp;
  float temperature, humidity, pressure_hpa;
};

// Fixed-size FIFO. When full, push() overwrites the oldest entry.
template <typename T, int N>
class RingBuffer {
 public:
  // Returns true if the oldest entry had to be dropped to make room.
  bool push(const T &item) {
    bool dropped = false;
    if (count_ == N) {
      head_ = (head_ + 1) % N;
      count_--;
      dropped_++;
      dropped = true;
    }
    // Modulo hält den Schreibindex im festen Array (Ringpuffer-Logik).
    items_[(head_ + count_) % N] = item;
    count_++;
    return dropped;
  }
  bool empty() const { return count_ == 0; }
  int count() const { return count_; }
  uint32_t dropped() const { return dropped_; }
  // Oldest entry. Only valid if !empty().
  const T &front() const { return items_[head_]; }
  // Remove the oldest entry (no-op if empty).
  void pop() {
    if (count_ == 0)
      return;
    head_ = (head_ + 1) % N;
    count_--;
  }

 private:
  // Fester Array statt dynamischem Speicher: vermeidet Fragmentierung auf ESP32.
  T items_[N];
  int head_ = 0;   // index of the oldest entry
  int count_ = 0;
  // Zählt verworfene Backlog-Einträge; nützlich für Fehlerstatistik.
  uint32_t dropped_ = 0;
};

// ---------------------------------------------------------------------------
// JSON formatting
// ---------------------------------------------------------------------------

// Epochs before this are "clock not set" (Sep 2020).
const time_t CLOCK_VALID_AFTER = 1600000000;

inline bool clock_is_valid(time_t now_epoch) {
  return now_epoch > CLOCK_VALID_AFTER;
}

// Format a record as JSON for the .../radiation (is_backlog = false) or
// .../backlog (is_backlog = true) topic. Backlog records additionally carry
// "alter_s" (age in seconds) and the environmental values.
// "ts" is the UTC time of the measurement, or null if unknown. If the clock was
// not synced when the record was taken but is now, ts is derived from its age.
// Returns the snprintf()-style length (>= len means truncated).
inline int format_radiation_record(char *buf, size_t len, const MqttRecord &r, bool is_backlog,
                                   unsigned long now_ms, time_t now_epoch) {
  unsigned long age_s = (now_ms - r.ms) / 1000;  // unsigned: correct across millis() overflow
  time_t ts = r.ts;
  if (!ts && clock_is_valid(now_epoch))
    ts = now_epoch - (time_t)age_s;
  char ts_field[32] = "null";
  // gmtime_r/strftime: Unix-Zeit -> ISO-8601-Zeitstring für JSON.
  if (ts) {
    struct tm ti;
    gmtime_r(&ts, &ti);
    strftime(ts_field, sizeof(ts_field), "\"%Y-%m-%dT%H:%M:%SZ\"", &ti);
  }
  // snprintf schreibt JSON-Teil; Rückgabe n = benötigte bzw. geschriebene Länge.
  int n = snprintf(buf, len,
                   "{\"seq\":%u,\"ts\":%s,\"CPM\":%u,\"Dosisleistung_nSvh\":%.1f,\"Schnitt_nSvh\":%.1f,"
                   "\"Zaehlungen\":%u,\"Messzeit_s\":%.1f,\"HV_Pulse\":%u,\"Strahlenstatus\":\"%s\"",
                   (unsigned)r.seq, ts_field, r.cpm, r.dose_nsvph, r.accu_nsvph,
                   r.counts, r.sample_s, r.hv_pulses, radiation_status_label(r.dose_nsvph));
  // Weitergeschrieben nur wenn Platz; snprintf meldet benötigte Länge.
  if (is_backlog && n > 0 && (size_t)n < len) {
    n += snprintf(buf + n, len - n, ",\"alter_s\":%lu", age_s);
    if (r.have_thp && (size_t)n < len)
      n += snprintf(buf + n, len - n, ",\"Temperatur_C\":%.2f,\"Luftfeuchte_pct\":%.1f,\"Luftdruck_hPa\":%.1f",
                    r.temperature, r.humidity, r.pressure_hpa);
  }
  if (n > 0 && (size_t)n < len)
    n += snprintf(buf + n, len - n, "}");
  return n;
}

// ---------------------------------------------------------------------------
// MQTT topic
// ---------------------------------------------------------------------------

// Build "<prefix>/<device>/" from the configured prefix:
// surrounding whitespace and one leading '/' are removed, exactly one '/' separates
// prefix and device. An empty prefix gives "<device>/".
// Returns the snprintf()-style length (>= len means truncated).
inline int build_base_topic(char *buf, size_t len, const char *prefix, const char *device) {
  // Ternärer Ausdruck ersetzt NULL-Prefix durch leeren String (strlen/isspace-Sicherheit).
  const char *start = prefix ? prefix : "";
  while (*start && isspace((unsigned char)*start))
    start++;
  if (*start == '/')
    start++;
  size_t plen = strlen(start);
  while (plen > 0 && isspace((unsigned char)start[plen - 1]))
    plen--;
  if (plen > 0 && start[plen - 1] == '/')
    plen--;
  if (plen == 0)
    return snprintf(buf, len, "%s/", device);
  // Mit %.*s schneidet snprintf den Prefix-Ausschnitt auf plen Zeichen zu.
  return snprintf(buf, len, "%.*s/%s/", (int)plen, start, device);
}

#endif // _MQTT_LOGIC_H_
