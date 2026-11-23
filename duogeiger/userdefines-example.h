// User / hardware specific definitions - do NOT edit THIS (example) file.
//
// Copy this file to userdefines.h (which is git-ignored) and edit that.

// TUBE_TYPE values (predefined at sensor.community, DO NOT CHANGE):
#define TUBE_UNKNOWN 0
#define SBM20 1
#define SBM19 2
#define Si22G 3

// Your Geiger-Mueller counter tube:
#define TUBE_TYPE Si22G

// DEFAULT_LOG_LEVEL values (DO NOT CHANGE)
#include "log.h"

// Your log level:
#define DEFAULT_LOG_LEVEL INFO

// SERIAL_DEBUG values (DO NOT CHANGE)
#include "log_data.h"

// Your serial logging style:
#define SERIAL_DEBUG Serial_Logging

// Server transmission debugging:
#define DEBUG_SERVER_SEND false

// Speaker tick with every pulse?
#define SPEAKER_TICK true

// White LED flashing with every pulse?
#define LED_TICK true

// Enable OLED display?
#define SHOW_DISPLAY true

// Play a start sound at boot/reboot time?
#define PLAY_SOUND true

// Send to madavi.de for real-time visualisation?
#define SEND2MADAVI false

// Send to sensor.community for data archival?
#define SEND2SENSORCOMMUNITY false

// LoRa has been removed from DuoGeiger - always keep false.
#define SEND2LORA false

// Send data via BLE (Heart Rate Service)?
#define SEND2BLE false

// Play a local alarm sound when radiation level is too high?
#define LOCAL_ALARM_SOUND false

// Accumulated dose rate threshold to trigger the local alarm [µSv/h]
#define LOCAL_ALARM_THRESHOLD 0.500

// Factor of current vs. accumulated dose rate to trigger alarm
#define LOCAL_ALARM_FACTOR 3

// Send periodic data messages via Telegram Messenger?
// 0 = never; 3600 = once per hour; 86400 = once per day
// Requires Telegram Bot Token and Chat ID configured in web interface.
#define SEND_DATA_TO_MESSENGER_EVERY 0

// Send local alarm notifications via Telegram?
#define SEND_LOCAL_ALARM_TO_MESSENGER false

// Send periodic data to MQTT broker (seconds between publishes)?
// 150 = every 2.5 min (matches default measurement interval)
// 0 = never
// Requires MQTT server configured in web interface.
#define SEND_DATA_TO_MQTT_EVERY 150

// Send local alarm notifications to MQTT?
#define SEND_LOCAL_ALARM_TO_MQTT false

// ---- Diagnostics / connectivity (all optional; defaults apply if omitted) ----

// Own NTP server, tried first (e.g. "fritz.box" or the router IP). Empty = public only.
// Fallbacks pool.ntp.org and ptbtime1.ptb.de are always used if it does not answer.
#define NTP_SERVER ""

// Remote syslog (UDP), e.g. "192.168.178.10" — empty string = off.
#define SYSLOG_SERVER ""
#define SYSLOG_PORT 514

// Disable WiFi modem sleep for more reliable reception on weak WiFi (~+80 mA).
// Automatically ignored while BLE is active (coexistence needs modem sleep).
#define WIFI_NO_SLEEP true

// Time IotWebConf tries to (re)connect before falling back to its own AP [ms].
#define WIFI_CONNECTION_TIMEOUT_MS 120000

// Firmware update over the network (PlatformIO "espota", port 3232).
// Password = admin password of the web configuration.
#define ENABLE_ARDUINO_OTA true

// Measurement records kept in RAM while MQTT is unreachable (48 x 150 s = 2 h).
#define MQTT_BACKLOG_SIZE 48
