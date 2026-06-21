// measurements data transmission related code
// - via WiFi to internet servers
// - via LoRa to TTN (to internet servers)

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <UniversalTelegramBot.h>
#include <PubSubClient.h>

#include "log.h"
#include "display.h"
#include "userdefines.h"
#include "webconf.h"
#include "tube.h"

#include "transmission.h"

#include "ca_certs.h"

// Hosts for data delivery

// use http for now, could we use https?
#define MADAVI "http://api-rrd.madavi.de/data.php"

// use http for now, server operator tells there are performance issues with https.
#define SENSORCOMMUNITY "http://api.sensor.community/v1/push-sensor-data/"

// Send http(s) post requests to a custom server
// Note: Custom toilet URLs from https://ptsv2.com/ can be used for debugging
// and work with https and http.
#define CUSTOMSRV "https://ptsv2.com/t/xxxxx-yyyyyyyyyy/post"
// Get your own toilet URL and put it here before setting this to true.
#define SEND2CUSTOMSRV false

#define MADAVI_PREFIX_GEIGER "Radiation_"
#define MADAVI_PREFIX_THP "BME280_"

const char *json_format_radiation = R"=====(
{
 "software_version": "%s",
 "tube_type": "%s",
 "sensordatavalues": [
  {"value_type": "%scounts_per_minute", "value": "%d"},
  {"value_type": "%shv_pulses", "value": "%d"},
  {"value_type": "%scounts", "value": "%d"},
  {"value_type": "%ssample_time_ms", "value": "%d"}
 ]
}
)=====";

const char *json_format_thp = R"=====(
{
 "software_version": "%s",
 "sensordatavalues": [
  {"value_type": "%stemperature", "value": "%.2f"},
  {"value_type": "%shumidity", "value": "%.2f"},
  {"value_type": "%spressure", "value": "%.1f"}
 ]
}
)=====";

// Sent on every measurement interval
const char *json_format_radiation_mqtt = R"=====(
{
 "CPM": %u,
 "Dosisleistung_nSvh": %.1f,
 "Schnitt_nSvh": %.1f,
 "Zaehlungen": %u,
 "Messzeit_s": %.1f,
 "HV_Pulse": %u,
 "Strahlenstatus": "%s"
}
)=====";

const char *json_format_thp_mqtt = R"=====(
{
 "Temperatur_C": %.2f,
 "Luftfeuchte_pct": %.1f,
 "Luftdruck_hPa": %.1f
}
)=====";

// Sent once after boot, every 24h, and on IP change
const char *json_format_info_mqtt = R"=====(
{
 "Hostname": "%s",
 "IP": "%s",
 "Version": "%s",
 "TubeType": "%s"
}
)=====";

static String http_software_version;
static String chipID;
static MeasurementData last_measurement = {false};

const MeasurementData& get_last_measurement() {
  return last_measurement;
}

typedef struct https_client {
  WiFiClientSecure *wc;
  HTTPClient *hc;
} HttpsClient;

static HttpsClient c_madavi, c_sensorc, c_customsrv, c_telegram;
static WiFiClient c_mqtt;
UniversalTelegramBot *telegram_bot;
PubSubClient *mqtt_client;
void mqtt_callback(char* topic, byte *payload, unsigned int length);

bool is_mqtt_connected() {
  return mqtt_client && mqtt_client->connected();
}

// Radiation status thresholds in nSv/h (1 µSv/h = 1000 nSv/h)
const char* radiation_status_label(float dose_nsvph) {
  if      (dose_nsvph <   1500.0f) return "Normal";
  else if (dose_nsvph <  10000.0f) return "Erhoht";
  else if (dose_nsvph <  80000.0f) return "Deutlich erhoht";
  else if (dose_nsvph < 350000.0f) return "Hohe Strahlung";
  else                             return "STRAHLUNGSALARM";
}

const char* radiation_status_color(float dose_nsvph) {
  if      (dose_nsvph <   1500.0f) return "#2a7a2a";
  else if (dose_nsvph <  10000.0f) return "#c8a000";
  else if (dose_nsvph <  80000.0f) return "#c85000";
  else if (dose_nsvph < 350000.0f) return "#b00000";
  else                             return "#5a0000";
}

#define MQTT_RECONNECT_ATTEMPT_INTERVAL 30000  // ms

void setup_transmission(const char *version, const char *device_name) {
  chipID = String(device_name);

  http_software_version = String(version);

  c_madavi.wc = new WiFiClientSecure;
  c_madavi.wc->setCACert(ca_certs);
  c_madavi.hc = new HTTPClient;

  c_sensorc.wc = new WiFiClientSecure;
  c_sensorc.wc->setCACert(ca_certs);
  c_sensorc.hc = new HTTPClient;

  c_customsrv.wc = new WiFiClientSecure;
  c_customsrv.wc->setCACert(ca_certs);
  c_customsrv.hc = new HTTPClient;

  mqtt_client = new PubSubClient(c_mqtt);

  if (strlen(mqttServer) < 5) {
    sendDataToMqttEvery = 0;
    sendLocalAlarmToMqtt = false;
    set_status(STATUS_MQTT, ST_MQTT_OFF);
  } else {
    log(DEBUG, "Starting MQTT client...");
    mqtt_client->setServer(mqttServer, mqttPort);
    mqtt_client->setCallback(mqtt_callback);
    set_status(STATUS_MQTT, ST_MQTT_INIT);
  }

  c_telegram.wc = new WiFiClientSecure;
  c_telegram.wc->setCACert(TELEGRAM_CERTIFICATE_ROOT);
  c_telegram.hc = new HTTPClient;

  if ((strlen(telegramBotToken) < 40) || (strlen(telegramChatId) < 7)) {
    sendDataToMessengerEvery = 0;
    sendLocalAlarmToMessenger = false;
    set_status(STATUS_TELEGRAM, ST_TELEGRAM_OFF);
  } else {
    log(DEBUG, "Starting Telegram Bot...");
    telegram_bot = new UniversalTelegramBot(telegramBotToken, *(c_telegram.wc));
    set_status(STATUS_TELEGRAM, ST_TELEGRAM_INIT);
  }

  set_status(STATUS_SCOMM, sendToCommunity ? ST_SCOMM_INIT : ST_SCOMM_OFF);
  set_status(STATUS_MADAVI, sendToMadavi ? ST_MADAVI_INIT : ST_MADAVI_OFF);
  set_status(STATUS_TTN, ST_TTN_OFF);
  set_status(STATUS_MQTT, ((sendDataToMqttEvery != 0) || sendLocalAlarmToMqtt) ? ST_MQTT_INIT : ST_MQTT_OFF);
  set_status(STATUS_TELEGRAM, ((sendDataToMessengerEvery != 0) || sendLocalAlarmToMessenger) ? ST_TELEGRAM_INIT : ST_TELEGRAM_OFF);
}

void mqtt_callback(char* topic, byte *payload, unsigned int length) {
    char rx_buffer[length+1];
    log(INFO, "MQTT incoming msg!");
    memcpy(rx_buffer, (char *)payload, length);
    rx_buffer[length] = '\0';
    log(INFO, "MQTT msg on channel %s: %s", topic, rx_buffer);
}

static String mqtt_base_topic() {
  String t = mqttChannelPrefix;
  t.trim();
  if (t[0] == '/') t.remove(0, 1);
  if (!t.endsWith("/")) t += '/';
  return t + chipID + '/';
}

void poll_transmission(int wifi_status) {
  static unsigned long last_mqtt_reconnect = 0;
  static unsigned long last_info_send_ms = 0;
  static bool info_sent_on_boot = false;
  static String last_sent_ip = "";

  if (get_status(STATUS_MQTT) != ST_MQTT_OFF) {
    if (mqtt_client->connected()) {
      mqtt_client->loop();

      // Send info payload: on boot, every 24h, or when IP changes
      String current_ip = WiFi.localIP().toString();
      bool ip_changed = (current_ip != "0.0.0.0") && (current_ip != last_sent_ip);
      bool due_24h = info_sent_on_boot && ((millis() - last_info_send_ms) >= 86400000UL);

      if (!info_sent_on_boot || due_24h || ip_changed) {
        char info_payload[256];
        snprintf(info_payload, sizeof(info_payload), json_format_info_mqtt,
                 chipID.c_str(),
                 current_ip.c_str(),
                 http_software_version.c_str(),
                 last_measurement.tube_type);
        if (mqtt_client->publish((mqtt_base_topic() + "info").c_str(), info_payload, true)) {
          info_sent_on_boot = true;
          last_info_send_ms = millis();
          last_sent_ip = current_ip;
          log(INFO, "MQTT info sent (ip=%s)", current_ip.c_str());
        }
      }
    } else if (wifi_status == ST_WIFI_CONNECTED) {
      if (millis() - last_mqtt_reconnect > MQTT_RECONNECT_ATTEMPT_INTERVAL) {
        log(INFO, "MQTT Reconnect");
        last_mqtt_reconnect = millis();
        if (mqtt_client->connect(chipID.c_str(), mqttUsername, mqttPassword))
          set_status(STATUS_MQTT, ST_MQTT_IDLE);
        else
          set_status(STATUS_MQTT, ST_MQTT_ERROR);
      }
    }
  }
}

void prepare_http(HttpsClient *client, const char *host) {
  if (host[4] == 's')  // https
    client->hc->begin(*client->wc, host);
  else  // http
    client->hc->begin(host);
  client->hc->addHeader("Content-Type", "application/json; charset=UTF-8");
  client->hc->addHeader("Connection", "keep-alive");
  client->hc->addHeader("X-Sensor", chipID);
}

int send_http(HttpsClient *client, String body) {
  if (DEBUG_SERVER_SEND)
    log(DEBUG, "http request body: %s", body.c_str());

  int httpResponseCode = client->hc->POST(body);
  if (httpResponseCode > 0) {
    String response = client->hc->getString();
    if (DEBUG_SERVER_SEND) {
      log(DEBUG, "http code: %d", httpResponseCode);
      log(DEBUG, "http response: %s", response.c_str());
    }
  } else {
    log(ERROR, "Error on sending POST: %d", httpResponseCode);
  }
  client->hc->end();
  return httpResponseCode;
}

int send_http_geiger(HttpsClient *client, const char *host, const char *tube_type, int tube_nbr, unsigned int timediff, unsigned int hv_pulses,
                     unsigned int gm_counts, unsigned int cpm, int xpin, const char *prefix) {
  char body[1000];
  prepare_http(client, host);
  if (xpin != XPIN_NO_XPIN)
    client->hc->addHeader("X-PIN", String(xpin));
  snprintf(body, 1000, json_format_radiation,
           http_software_version.c_str(),
           tube_type,
           prefix, cpm,
           prefix, hv_pulses,
           prefix, gm_counts,
           prefix, timediff);
  return send_http(client, body);
}

int send_http_thp(HttpsClient *client, const char *host, float temperature, float humidity, float pressure, int xpin, const char *prefix) {
  char body[1000];
  prepare_http(client, host);
  if (xpin != XPIN_NO_XPIN)
    client->hc->addHeader("X-PIN", String(xpin));
  snprintf(body, 1000, json_format_thp,
           http_software_version.c_str(),
           prefix, temperature,
           prefix, humidity,
           prefix, pressure);
  return send_http(client, body);
}

// Send data to web servers for sensor data with predefined interval of MEASUREMENT, default 150 s / 2.5 min. No alarm handling.
void transmit_data_to_web(const char *tube_type, int tube_nbr, unsigned int dt, unsigned int hv_pulses, unsigned int gm_counts, unsigned int cpm,
                   int have_thp, float temperature, float humidity, float pressure, int wifi_status) {
  float dose_nsvph = (dt > 0) ? ((float)cpm / 60.0f) * tubes[TUBE_TYPE].cps_to_uSvph * 1000.0f : 0.0f;
  last_measurement = {true, cpm, gm_counts, hv_pulses, dt, dose_nsvph,
                      (bool)have_thp, temperature, humidity, pressure};
  strncpy(last_measurement.tube_type, tube_type ? tube_type : "", sizeof(last_measurement.tube_type) - 1);
  strncpy(last_measurement.version, http_software_version.c_str(), sizeof(last_measurement.version) - 1);

  if (wifi_status != ST_WIFI_CONNECTED)
    return;

  int rc1, rc2;

  #if SEND2CUSTOMSRV
  bool customsrv_ok;
  log(INFO, "Sending to CUSTOMSRV ...");
  rc1 = send_http_geiger(&c_customsrv, CUSTOMSRV, dt, hv_pulses, gm_counts, cpm, XPIN_NO_XPIN, "");
  rc2 = have_thp ? send_http_thp(&c_customsrv, CUSTOMSRV, temperature, humidity, pressure, XPIN_NO_XPIN, "") : 200;
  customsrv_ok = (rc1 == 200) && (rc2 == 200);
  log(INFO, "Sent to CUSTOMSRV, status: %s, http: %d %d", customsrv_ok ? "ok" : "error", rc1, rc2);
  #endif

  if (sendToMadavi) {
    bool madavi_ok;
    log(INFO, "Sending to Madavi ...");
    set_status(STATUS_MADAVI, ST_MADAVI_SENDING);
    rc1 = send_http_geiger(&c_madavi, MADAVI, tube_type, tube_nbr, dt, hv_pulses, gm_counts, cpm, XPIN_NO_XPIN, MADAVI_PREFIX_GEIGER);
    rc2 = have_thp ? send_http_thp(&c_madavi, MADAVI, temperature, humidity, pressure, XPIN_NO_XPIN, MADAVI_PREFIX_THP) : 200;
    delay(300);
    madavi_ok = (rc1 == 200) && (rc2 == 200);
    log(INFO, "Sent to Madavi, status: %s, http: %d %d", madavi_ok ? "ok" : "error", rc1, rc2);
    set_status(STATUS_MADAVI, madavi_ok ? ST_MADAVI_IDLE : ST_MADAVI_ERROR);
  }

  if (sendToCommunity) {
    bool scomm_ok;
    log(INFO, "Sending to sensor.community ...");
    set_status(STATUS_SCOMM, ST_SCOMM_SENDING);
    rc1 = send_http_geiger(&c_sensorc, SENSORCOMMUNITY, tube_type, tube_nbr, dt, hv_pulses, gm_counts, cpm, XPIN_RADIATION, "");
    rc2 = have_thp ? send_http_thp(&c_sensorc, SENSORCOMMUNITY, temperature, humidity, pressure, XPIN_BME280, "") : 201;
    delay(300);
    scomm_ok = (rc1 == 201) && (rc2 == 201);
    log(INFO, "Sent to sensor.community, status: %s, http: %d %d", scomm_ok ? "ok" : "error", rc1, rc2);
    set_status(STATUS_SCOMM, scomm_ok ? ST_SCOMM_IDLE : ST_SCOMM_ERROR);
  }
}

// Send data to user with interval configurable via Web Config, incl. alarm handling.
void transmit_data_to_telegram(const char *tube_type, int tube_nbr, float tube_factor, unsigned int cpm, unsigned int accu_cpm, float accu_rate,
                   int have_thp, float temperature, float humidity, float pressure, int wifi_status, bool alarm_status) {

  if (wifi_status != ST_WIFI_CONNECTED)
    return;

  if ((sendDataToMessengerEvery == 0) && !sendLocalAlarmToMessenger)
    return;

  if ((strlen(telegramBotToken) < 40) || (strlen(telegramChatId) < 7)) {
    sendDataToMessengerEvery = 0;
    sendLocalAlarmToMessenger = false;
    set_status(STATUS_TELEGRAM, ST_TELEGRAM_OFF);
    return;
  }

  char thp_text[60];
  bool telegram_ok;
  char message[200];

  if (have_thp) {
    sprintf(thp_text, "\nBME data: %.1fC %.1f%% %.1fhPa", temperature, humidity, pressure/100);
  }

  log(INFO, "Sending to Telegram messenger ...");
  set_status(STATUS_TELEGRAM, ST_TELEGRAM_SENDING);
  if (alarm_status) {
    if (tube_nbr > 0)
      sprintf(message, "<b>--- MULTIGEIGER ALERT ! ---</b>\n%s\n<code>%s</code> rate too high:\n%.2f nSv/h (accumulated: %.2f nSv/h)", localDeviceName, chipID.c_str(), cpm*tube_factor*1000/60, accu_rate*1000);
    else
      sprintf(message, "<b>--- MULTIGEIGER ALERT ! ---</b>\n%s\n<code>%s</code> rate too high:\n%d (accumulated: %d)", localDeviceName, chipID.c_str(), cpm, accu_cpm);
  } else {
    if (tube_nbr > 0)
      sprintf(message, "%s\nMultiGeiger <code>%s</code> rates:\n%.2f nSv/h (accumulated: %.2f nSv/h)%s", localDeviceName, chipID.c_str(), cpm*tube_factor*1000/60, accu_rate*1000, have_thp ? thp_text : "");
    else
      sprintf(message, "%s\nMultiGeiger <code>%s</code> CPM:\n%d (accumulated: %d)%s", localDeviceName, chipID.c_str(), cpm, accu_cpm, have_thp ? thp_text : "");
  }
  telegram_ok = telegram_bot->sendMessage(telegramChatId, message, "HTML", 0);
  log(INFO, "Sent to Telegram messenger, status: %s", telegram_ok ? "ok" : "error");
  set_status(STATUS_TELEGRAM, telegram_ok ? ST_TELEGRAM_IDLE : ST_TELEGRAM_ERROR);
}

// Send data to user with interval configurable via Web Config, incl. alarm handling.
void transmit_data_to_mqtt(const char *tube_type, int tube_nbr, float tube_factor, unsigned int cpm, unsigned int accu_cpm, float accu_rate,
                   int have_thp, float temperature, float humidity, float pressure, int wifi_status, bool alarm_status) {

  if (wifi_status != ST_WIFI_CONNECTED)
    return;

  if (get_status(STATUS_MQTT) == ST_MQTT_OFF)
    return;

  if (strlen(mqttServer) < 5) {
    sendDataToMqttEvery = 0;
    sendLocalAlarmToMqtt = false;
    set_status(STATUS_MQTT, ST_MQTT_OFF);
    return;
  }

  if (!mqtt_client->connected())
    return;

  String mqtt_topic = mqtt_base_topic();
  char mqtt_payload[1000];
  bool mqtt_ok;

  log(INFO, "Sending to MQTT server on %s", mqtt_topic.c_str());
  set_status(STATUS_MQTT, ST_MQTT_SENDING);
  float dose_nsvph = cpm * tube_factor * 1000.0f / 60.0f;
  snprintf(mqtt_payload, 1000, json_format_radiation_mqtt,
           cpm,
           dose_nsvph,
           accu_rate * 1000.0f,
           last_measurement.counts,
           last_measurement.sample_ms / 1000.0f,
           last_measurement.hv_pulses,
           radiation_status_label(dose_nsvph));
  mqtt_ok = mqtt_client->publish(String(mqtt_topic + "radiation").c_str(), mqtt_payload);

  if (mqtt_ok && have_thp) {
    snprintf(mqtt_payload, 1000, json_format_thp_mqtt,
            temperature,
            humidity,
            pressure);
    mqtt_ok = mqtt_client->publish(String(mqtt_topic + "environmental").c_str(), mqtt_payload);
  }

  log(INFO, "Sent to MQTT broker, status: %s", mqtt_ok ? "ok" : "error");
  set_status(STATUS_MQTT, mqtt_ok ? ST_MQTT_IDLE : ST_MQTT_ERROR);
}
