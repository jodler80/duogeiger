// Diese Datei hält Web-Konfigurationsparameter, Status-/Diagnoseseiten und
// die Anbindung des Update-Servers bereit.
// Web Configuration related code
// also: OTA updates

#include "log.h"
#include "speaker.h"
#include "transmission.h"
#include "tube.h"
#include "version.h"
#include "diag.h"
#include "clock.h"
#include <WiFi.h>

#include "IotWebConf.h"
#include "IotWebConfTParameter.h"
#include <IotWebConfESP32HTTPUpdateServer.h>
#include "userdefines.h"

// Checkboxes have 'selected' if checked, so we need 9 byte for this string.
#define CHECKBOX_LEN 9

bool speakerTick = SPEAKER_TICK;
bool playSound = PLAY_SOUND;
bool ledTick = LED_TICK;
bool showDisplay = SHOW_DISPLAY;
char speakerTick_c[CHECKBOX_LEN];
char playSound_c[CHECKBOX_LEN];
char ledTick_c[CHECKBOX_LEN];
char showDisplay_c[CHECKBOX_LEN];

bool sendToCommunity = SEND2SENSORCOMMUNITY;
bool sendToMadavi = SEND2MADAVI;
bool sendToLora = SEND2LORA;
bool sendToBle = SEND2BLE;
char sendToCommunity_c[CHECKBOX_LEN];
char sendToMadavi_c[CHECKBOX_LEN];
char sendToLora_c[CHECKBOX_LEN];
char sendToBle_c[CHECKBOX_LEN];

char appeui[17] = "";
char deveui[17] = "";
char appkey[IOTWEBCONF_WORD_LEN] = "";
static bool isLoraBoard;

bool soundLocalAlarm = LOCAL_ALARM_SOUND;
char soundLocalAlarm_c[CHECKBOX_LEN];
float localAlarmThreshold = LOCAL_ALARM_THRESHOLD;
int localAlarmFactor = (int)LOCAL_ALARM_FACTOR;

char telegramBotToken[50] = "";  // "XXXXXXXXXX:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
char telegramChatId[12] = "";  // "1234567890"
long sendDataToMessengerEvery = (long)SEND_DATA_TO_MESSENGER_EVERY;
bool sendLocalAlarmToMessenger = SEND_LOCAL_ALARM_TO_MESSENGER;
char sendLocalAlarmToMessenger_c[CHECKBOX_LEN];

char mqttServer[100] = "";
int mqttPort = 1883;  // default for MQTT: 1883
char mqttUsername[30] = "";
char mqttPassword[30] = "";
char mqttChannelPrefix[50] = "multigeiger";
long sendDataToMqttEvery = (long)SEND_DATA_TO_MQTT_EVERY;
bool sendLocalAlarmToMqtt = SEND_LOCAL_ALARM_TO_MQTT;
char sendLocalAlarmToMqtt_c[CHECKBOX_LEN];

char localDeviceName[33] = "MultiGeiger Device";
char ntpServer[64] = NTP_SERVER;  // own NTP server, tried first; "" = public servers only
// Fixed IP address (STA). Empty staticIp = DHCP. Netmask defaults to 255.255.255.0, DNS to the gateway.
char staticIp[16] = "";
char staticGateway[16] = "";
char staticNetmask[16] = "";
char staticDns[16] = "";

iotwebconf::ParameterGroup grpMisc = iotwebconf::ParameterGroup("misc", "Misc. Settings");
iotwebconf::CheckboxParameter startSoundParam = iotwebconf::CheckboxParameter("Start sound", "startSound", playSound_c, CHECKBOX_LEN, playSound);
iotwebconf::CheckboxParameter speakerTickParam = iotwebconf::CheckboxParameter("Speaker tick", "speakerTick", speakerTick_c, CHECKBOX_LEN, speakerTick);
iotwebconf::CheckboxParameter ledTickParam = iotwebconf::CheckboxParameter("LED tick", "ledTick", ledTick_c, CHECKBOX_LEN, ledTick);
iotwebconf::CheckboxParameter showDisplayParam = iotwebconf::CheckboxParameter("Show display", "showDisplay", showDisplay_c, CHECKBOX_LEN, showDisplay);

iotwebconf::ParameterGroup grpTransmission = iotwebconf::ParameterGroup("transmission", "Transmission Settings");
iotwebconf::CheckboxParameter sendToCommunityParam = iotwebconf::CheckboxParameter("Send to sensor.community", "send2Community", sendToCommunity_c, CHECKBOX_LEN, sendToCommunity);
iotwebconf::CheckboxParameter sendToMadaviParam = iotwebconf::CheckboxParameter("Send to madavi.de", "send2Madavi", sendToMadavi_c, CHECKBOX_LEN, sendToMadavi);
iotwebconf::CheckboxParameter sendToBleParam = iotwebconf::CheckboxParameter("Send to BLE (Reboot required!)", "send2ble", sendToBle_c, CHECKBOX_LEN, sendToBle);

// Keep LoRa settings to not disrupt on-device database. Not shown in webconfig
iotwebconf::ParameterGroup grpLoRa = iotwebconf::ParameterGroup("lora", "LoRa Settings");
iotwebconf::CheckboxParameter sendToLoraParam = iotwebconf::CheckboxParameter("LoRa/TTN ALWAYS DISABLED!", "send2lora", sendToLora_c, CHECKBOX_LEN, sendToLora);
iotwebconf::TextParameter deveuiParam = iotwebconf::TextParameter("DEVEUI", "deveui", deveui, 17);
iotwebconf::TextParameter appeuiParam = iotwebconf::TextParameter("APPEUI", "appeui", appeui, 17);
iotwebconf::TextParameter appkeyParam = iotwebconf::TextParameter("APPKEY", "appkey", appkey, IOTWEBCONF_WORD_LEN);

iotwebconf::ParameterGroup grpAlarm = iotwebconf::ParameterGroup("alarm", "Local Alarm Settings");
iotwebconf::CheckboxParameter soundLocalAlarmParam = iotwebconf::CheckboxParameter("Enable local alarm sound", "soundLocalAlarm", soundLocalAlarm_c, CHECKBOX_LEN, soundLocalAlarm);
iotwebconf::FloatTParameter localAlarmThresholdParam =
  iotwebconf::Builder<iotwebconf::FloatTParameter>("localAlarmThreshold").
  label("Local alarm threshold (µSv/h)").
  defaultValue(localAlarmThreshold).
  step(0.1).placeholder("e.g. 0.5").build();
iotwebconf::IntTParameter<int16_t> localAlarmFactorParam =
  iotwebconf::Builder<iotwebconf::IntTParameter<int16_t>>("localAlarmFactor").
  label("Factor of current dose rate vs. accumulated").
  defaultValue(localAlarmFactor).
  min(2).max(100).
  step(1).placeholder("2..100").build();

iotwebconf::ParameterGroup grpMessenger = iotwebconf::ParameterGroup("messenger", "Telegram Messenger Settings");
iotwebconf::IntTParameter<int32_t> sendDataToMessengerEveryParam =
  iotwebconf::Builder<iotwebconf::IntTParameter<int32_t>>("sendDataToMessengerEvery").
  label("Send data via Messenger every n sec\n(0=never,3600=1/h,86400=1/d,604800=1/week)").
  defaultValue(sendDataToMessengerEvery).
  min(0).max(31536000).
  step(1).placeholder("0..31536000").build();
iotwebconf::CheckboxParameter sendLocalAlarmToMessengerParam = iotwebconf::CheckboxParameter("Send local alarm via Messenger", "sendLocalAlarmToMessenger", sendLocalAlarmToMessenger_c, CHECKBOX_LEN, sendLocalAlarmToMessenger);
iotwebconf::PasswordParameter telegramBotTokenParam = iotwebconf::PasswordParameter("Telegram Bot Token (Reboot required!)", "telegramBotToken", telegramBotToken, 50);
iotwebconf::PasswordParameter telegramChatIdParam = iotwebconf::PasswordParameter("Telegram Chat ID", "telegramChatId", telegramChatId, 12);

iotwebconf::ParameterGroup grpMqtt = iotwebconf::ParameterGroup("mqtt", "MQTT Settings");
iotwebconf::IntTParameter<int32_t> sendDataToMqttEveryParam =
  iotwebconf::Builder<iotwebconf::IntTParameter<int32_t>>("sendDataToMqttEvery").
  label("Send data to MQTT every n sec\n(0=never,3600=1/h,86400=1/d,604800=1/week)").
  defaultValue(sendDataToMqttEvery).
  min(0).max(31536000).
  step(1).placeholder("0..31536000").build();
iotwebconf::CheckboxParameter sendLocalAlarmToMqttParam = iotwebconf::CheckboxParameter("Send local alarm to MQTT", "sendLocalAlarmToMqtt", sendLocalAlarmToMqtt_c, CHECKBOX_LEN, sendLocalAlarmToMqtt);
iotwebconf::TextTParameter<100> mqttServerParam =
  iotwebconf::Builder<iotwebconf::TextTParameter<100>>("mqttServer").
  label("MQTT Server (Reboot required!)").
  defaultValue(mqttServer).
  build();
iotwebconf::UIntTParameter<uint16_t> mqttPortParam =
  iotwebconf::Builder<iotwebconf::UIntTParameter<uint16_t>>("mqttPort").
  label("MQTT Server Port (default: 1883)").
  defaultValue(mqttPort).
  min(0).max(65535).
  step(1).placeholder("0..65535").build();
iotwebconf::TextTParameter<30> mqttUsernameParam =
  iotwebconf::Builder<iotwebconf::TextTParameter<30>>("mqttUsername").
  label("Username for Login").
  defaultValue(mqttUsername).
  build();
iotwebconf::PasswordParameter mqttPasswordParam = iotwebconf::PasswordParameter("Password for Login", "mqttPassword", mqttPassword, 30);
iotwebconf::TextTParameter<50> mqttChannelPrefixParam =
  iotwebconf::Builder<iotwebconf::TextTParameter<50>>("mqttChannelPrefix").
  label("MQTT Channel prefix").
  defaultValue(mqttChannelPrefix).
  build();

iotwebconf::ParameterGroup grpDevice = iotwebconf::ParameterGroup("device", "Generic Device Settings");
iotwebconf::TextTParameter<33> localDeviceNameParam =
  iotwebconf::Builder<iotwebconf::TextTParameter<33>>("localDeviceName").
  label("Name this specific device").
  defaultValue(localDeviceName).
  build();
// Appended as the LAST parameter on purpose: the stored configuration is read in order,
// so existing settings keep their place and CONFIG_VERSION can stay unchanged.
iotwebconf::TextTParameter<64> ntpServerParam =
  iotwebconf::Builder<iotwebconf::TextTParameter<64>>("ntpServer").
  label("NTP-Server (z.B. fritz.box; leer = nur pool.ntp.org/ptbtime1.ptb.de)").
  defaultValue(ntpServer).
  build();

// Fixed IP: again appended at the very end (after the NTP server), same reasoning as above.
iotwebconf::ParameterGroup grpNetwork = iotwebconf::ParameterGroup("network", "Netzwerk (feste IP-Adresse)");
iotwebconf::TextTParameter<16> staticIpParam =
  iotwebconf::Builder<iotwebconf::TextTParameter<16>>("staticIp").
  label("Feste IP-Adresse (leer = automatisch per DHCP)").
  defaultValue(staticIp).
  build();
iotwebconf::TextTParameter<16> staticGatewayParam =
  iotwebconf::Builder<iotwebconf::TextTParameter<16>>("staticGateway").
  label("Gateway (z.B. 192.168.30.1)").
  defaultValue(staticGateway).
  build();
iotwebconf::TextTParameter<16> staticNetmaskParam =
  iotwebconf::Builder<iotwebconf::TextTParameter<16>>("staticNetmask").
  label("Netzmaske (leer = 255.255.255.0)").
  defaultValue(staticNetmask).
  build();
iotwebconf::TextTParameter<16> staticDnsParam =
  iotwebconf::Builder<iotwebconf::TextTParameter<16>>("staticDns").
  label("DNS-Server (leer = Gateway)").
  defaultValue(staticDns).
  build();

// This only needs to be changed if the layout of the configuration is changed.
// Appending new variables does not require a new version number here.
// If this value is changed, ALL configuration variables must be re-entered,
// including the WiFi credentials.
// Project page with README (setup, status page, MQTT topics, diagnostics) — linked as "Hilfe".
#define DUOGEIGER_HELP_URL "https://github.com/jodler80/duogeiger#readme"

#define CONFIG_VERSION "016"

// Webserver für Status/Konfig, DNS für Captive-Portal und
// Update-Server für OTA.
DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;

// Vorwärtsdeklaration: buildSSID wird schon bei globaler Initialisierung
// des Hostnamens verwendet.
char *buildSSID(void);

// SSID == thingName
const char *theName = buildSSID();
char ssid[IOTWEBCONF_WORD_LEN];  // LEN == 33 (2020-01-13)

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "ESP32Geiger";

IotWebConf iotWebConf(theName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

// Drei MAC-Bytes werden zu einer kurzen 24-Bit-ID umgebaut,
// damit AP-Name und Hostname eindeutig und kurz bleiben.
unsigned long getESPchipID() {
  uint64_t espid = ESP.getEfuseMac();
  uint8_t *pespid = (uint8_t *)&espid;
  uint32_t id = 0;
  uint8_t *pid = (uint8_t *)&id;
  pid[0] = (uint8_t)pespid[5];
  pid[1] = (uint8_t)pespid[4];
  pid[2] = (uint8_t)pespid[3];
  log(INFO, "ID: %08X", id);
  log(INFO, "MAC: %04X%08X", (uint16_t)(espid >> 32), (uint32_t)espid);
  return id;
}

// Der zurückgegebene Puffer ist ein globales char-Feld und bleibt
// gültig, während das Programm läuft.
char *buildSSID() {
  // build SSID from ESP chip id
  uint32_t id = getESPchipID();
  sprintf(ssid, "duogeiger-%d", id);
  return ssid;
}

extern float accumulated_dose_rate;   // µSv/h, running average since boot (duogeiger.cpp)
extern float accumulated_count_rate;  // counts/s, running average since boot (duogeiger.cpp)

// Plain JSON with all diagnostic values — handy for curl/FHEM when MQTT is down.
void handleDiag(void) {
  // Statisch, damit der 800-Byte-Puffer nicht bei jedem Aufruf im Stack
  // angelegt wird.
  static char buf[800];
  int n = diag_json(buf, sizeof(buf), get_mqtt_state());
  if (n > 0 && (size_t)n + 24 < sizeof(buf))  // append backlog count: replace closing '}'
    snprintf(buf + n - 1, sizeof(buf) - n + 1, ",\"backlog\":%d}", get_mqtt_backlog_count());
  server.send(200, "application/json", buf);
}

// Erzeugt HTML-Diagnoseblock mit Reset-Ursachen, Heap-Fragmentierung
// und Netzwerk-/MQTT-Verbindungsdaten.
static void build_diag_block(char *buf, size_t len) {
  const DiagData &d = diag_get();
  snprintf(buf, len,
    "<h2>Diagnose</h2><table>"
    "<tr><td>Neustarts</td><td><b>%u</b></td></tr>"
    "<tr><td>Uhrzeit (NTP)</td><td><b>%s</b></td></tr>"
    "<tr><td>Letzter Reset</td><td><b>%s</b>%s%s%s</td></tr>"
    "<tr><td>Kanal / AP</td><td><b>%d</b> <span class='unit'>%s</span></td></tr>"
    "<tr><td>WLAN- / MQTT-Reconnects</td><td><b>%u / %u</b></td></tr>"
    "<tr><td>MQTT-Status</td><td><b>%d</b> <span class='unit'>(Puffer: %d)</span></td></tr>"
    "<tr><td>Heap frei / min / Block</td><td><b>%u</b> <span class='unit'>/ %u / %u</span></td></tr>"
    "<tr><td>Max. Schleife</td><td><b>%lu</b> <span class='unit'>ms</span></td></tr>"
    "<tr><td>HTTP-Fehler / Dauer</td><td><b>%u</b> <span class='unit'>/ %lu ms</span></td></tr>"
    "<tr><td>Messung Nr.</td><td><b>%u</b></td></tr>"
    "</table><p style='font-size:.75em;'><a href='/diag'>JSON</a></p>",
    d.boot_count,
    clock_is_synced() ? "synchron" : "<span style='color:#c00;'>nicht synchron</span>",
    d.reset_reason,
    d.restart_reason[0] ? " <span class='unit'>(" : "", d.restart_reason, d.restart_reason[0] ? ")</span>" : "",
    WiFi.channel(), WiFi.BSSIDstr().c_str(),
    d.wifi_reconnects, d.mqtt_reconnects,
    get_mqtt_state(), get_mqtt_backlog_count(),
    ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(),
    d.loop_max_ms,
    d.http_err, d.http_last_ms,
    d.seq);
}

// Statusseite baut HTML aus System-, WLAN-, MQTT-, Strahlungs- und
// Diagnoseblock.
void handleStatus(void) {
  const MeasurementData& m = get_last_measurement();

  // uptime
  unsigned long uptime_s = millis() / 1000;
  unsigned long days    = uptime_s / 86400;
  unsigned long hours   = (uptime_s % 86400) / 3600;
  unsigned long minutes = (uptime_s % 3600) / 60;
  unsigned long seconds = uptime_s % 60;
  static char uptime_str[32];
  if (days > 0)
    snprintf(uptime_str, sizeof(uptime_str), "%lud %02lu:%02lu:%02lu", days, hours, minutes, seconds);
  else
    snprintf(uptime_str, sizeof(uptime_str), "%02lu:%02lu:%02lu", hours, minutes, seconds);

  // MQTT status
  const char *mqtt_status;
  if (strlen(mqttServer) < 5)
    mqtt_status = "inaktiv";
  else if (is_mqtt_connected())
    mqtt_status = "<span style='color:#2a2;'>verbunden</span>";
  else
    mqtt_status = "<span style='color:#c00;'>getrennt</span>";

  // WiFi signal quality
  int rssi = WiFi.RSSI();
  const char *rssi_label;
  if      (rssi >= -50) rssi_label = "sehr gut";
  else if (rssi >= -60) rssi_label = "gut";
  else if (rssi >= -70) rssi_label = "mittel";
  else if (rssi >= -80) rssi_label = "schwach";
  else                  rssi_label = "sehr schwach";

  static char system_block[800];
  snprintf(system_block, sizeof(system_block),
    "<h2>System</h2><table>"
    "<tr><td>Version</td><td><b>%s</b></td></tr>"
    "<tr><td>R&ouml;hrentyp</td><td><b>%s</b></td></tr>"
    "<tr><td>Hostname</td><td><b>%s</b></td></tr>"
    "<tr><td>IP</td><td><b>%s</b> <span class='unit'>(%s)</span></td></tr>"
    "<tr><td>Laufzeit</td><td><b>%s</b></td></tr>"
    "<tr><td>WLAN</td><td><b>%d dBm</b> <span class='unit'>(%s)</span></td></tr>"
    "<tr><td>MQTT</td><td><b>%s</b></td></tr>"
    "</table>",
    VERSION_STR,
    tubes[TUBE_TYPE].type,
    iotWebConf.getThingName(),
    WiFi.localIP().toString().c_str(), staticIp[0] ? "fest" : "DHCP", uptime_str, rssi, rssi_label, mqtt_status);

  // Radiation status — thresholds/labels/colors come from shared helpers in transmission.cpp
  float dose = m.valid ? m.dose_nsvph : 0.0f;

  static char rad_block[512];
  snprintf(rad_block, sizeof(rad_block),
    "<div style='display:inline-block;margin:14px auto 6px;padding:10px 24px;"
    "background:%s;color:#fff;border-radius:6px;font-size:1.2em;font-weight:bold;'>"
    "%s</div>"
    "<div style='font-size:.75em;margin-bottom:8px;'>"
    "<a href='https://de.wikipedia.org/wiki/Radiologischer_Notfall#Dosis-Eckwerte' "
    "style='color:#16A1E7;'>Dosis-Eckwerte (Wikipedia)</a></div>",
    radiation_status_color(dose), radiation_status_label_html(dose));

  static char diag_block[1100];
  build_diag_block(diag_block, sizeof(diag_block));

  static char page[6000];
  // Solange die erste Messung nicht gültig ist, werden nur Platzhalter,
  // System und Links gezeigt.
  if (!m.valid) {
    snprintf(page, sizeof(page),
      "<!DOCTYPE html><html><head>"
      "<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'/>"
      "<meta http-equiv='refresh' content='15'/>"
      "<title>DuoGeiger Status</title>"
      "<style>body{text-align:center;font-family:verdana;padding:10px;}"
      "table{margin:auto;border-collapse:collapse;width:100%;max-width:380px;table-layout:fixed;}"
      "td{padding:7px 12px;text-align:left;border-bottom:1px solid #ddd;vertical-align:middle;}"
      "td:first-child{width:55%;}td:last-child{font-weight:bold;text-align:right;width:45%;}"
      "h2{color:#444;}.unit{font-weight:normal;font-size:.8em;color:#888;}"
      "a{color:#16A1E7;}</style></head><body>"
      "<h1>DuoGeiger</h1>"
      "%s"
      "<p>Warte auf erstes Messintervall (~2.5 min)...</p>"
      "%s"
      "%s"
      "<p><a href='/'>Home</a> &nbsp; <a href='/config'>Config</a> &nbsp; "
      "<a href='" DUOGEIGER_HELP_URL "' target='_blank'>Hilfe</a></p>"
      "</body></html>",
      rad_block, system_block, diag_block);
  } else {
    // Der Umgebungsblock erscheint nur, wenn gültige Umweltsensorwerte
    // vorhanden sind.
    static char thp_block[400]; thp_block[0] = '\0';
    if (m.have_thp) {
      snprintf(thp_block, sizeof(thp_block),
        "<h2>Umgebung</h2><table>"
        "<tr><td>Temperatur</td><td><b>%.1f</b> &deg;C</td></tr>"
        "<tr><td>Luftfeuchte</td><td><b>%.1f</b> %%</td></tr>"
        "<tr><td>Luftdruck</td><td><b>%.1f</b> hPa</td></tr>"
        "</table>",
        m.temperature, m.humidity, m.pressure / 100.0f);  // sensor delivers Pa
    }
    // Die Seite für gültige Messung nutzt nSv/h; accumulated_dose_rate
    // ist µSv/h und wird mit 1000 gewandelt.
    snprintf(page, sizeof(page),
      "<!DOCTYPE html><html><head>"
      "<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'/>"
      "<meta http-equiv='refresh' content='30'/>"
      "<title>DuoGeiger Status</title>"
      "<style>body{text-align:center;font-family:verdana;padding:10px;}"
      "table{margin:auto;border-collapse:collapse;width:100%;max-width:380px;table-layout:fixed;}"
      "td{padding:7px 12px;text-align:left;border-bottom:1px solid #ddd;vertical-align:middle;}"
      "td:first-child{width:55%;}"
      "td:last-child{font-weight:bold;text-align:right;width:45%;}"
      "h2{color:#444;}.unit{font-weight:normal;font-size:.8em;color:#888;}"
      "a{color:#16A1E7;}</style></head><body>"
      "<h1>DuoGeiger</h1>"
      "%s"
      "<h2>Strahlung</h2><table>"
      "<tr><td>CPM</td><td><b>%u</b> <span class='unit'>Z&auml;hlungen/min</span></td></tr>"
      "<tr><td>Dosisleistung</td><td><b>%.1f</b> <span class='unit'>nSv/h</span></td></tr>"
      "<tr><td>&Oslash; seit Start</td><td><b>%.1f</b> <span class='unit'>nSv/h</span></td></tr>"
      "<tr><td>Z&auml;hlungen</td><td><b>%u</b></td></tr>"
      "<tr><td>Messzeit</td><td><b>%.1f</b> <span class='unit'>s</span></td></tr>"
      "<tr><td>HV-Pulse</td><td><b>%u</b></td></tr>"
      "</table>"
      "%s"
      "%s"
      "%s"
      "<p style='margin-top:20px;font-size:.7em;color:#aaa;'>Auto-Refresh: 30s</p>"
      "<p><a href='/'>Home</a> &nbsp; <a href='/config'>Config</a> &nbsp; "
      "<a href='" DUOGEIGER_HELP_URL "' target='_blank'>Hilfe</a></p>"
      "</body></html>",
      rad_block,
      m.cpm, m.dose_nsvph, accumulated_dose_rate * 1000.0f,
      m.counts, m.sample_ms / 1000.0f, m.hv_pulses,
      thp_block, system_block, diag_block);
  }
  server.send(200, "text/html;charset=UTF-8", page);
}

void handleRoot(void) {  // Handle web requests to "/" path.
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal()) {
    // -- Captive portal requests were already served.
    return;
  }
  const char *index =
    "<!DOCTYPE html>"
    "<html lang='en'>"
    "<head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no' />"
    "<title>DuoGeiger</title>"
    "</head>"
    "<body>"
    "<h1>DuoGeiger</h1>"
    "<p>"
    "Go to the <a href='status'>status page</a> to see current measurements."
    "</p>"
    "<p>"
    "Go to the <a href='config'>config page</a> to change settings or update firmware."
    "</p>"
    "<p>"
    "Help and documentation: <a href='" DUOGEIGER_HELP_URL "' target='_blank'>DuoGeiger on GitHub</a>"
    "</p>"
    "</body>"
    "</html>\n";
  server.send(200, "text/html;charset=UTF-8", index);
  // looks like user wants to do some configuration or maybe flash firmware.
  // while accessing the flash, we need to turn ticking off to avoid exceptions.
  // user needs to save the config (or flash firmware + reboot) to turn it on again.
  // note: it didn't look like there is an easy way to put this call at the right place
  // (start of fw flash / start of config save) - this is why it is here.
  tick_enable(false);
}

static char lastWiFiSSID[IOTWEBCONF_WORD_LEN] = "";
static char lastIpConfig[70] = "";  // fixed-IP settings at the last load, to detect changes

// Übernimmt die Parameterwerte des Formulars in globale Betriebsvariablen
// und setzt die NTP-Server-Einstellung.
void loadConfigVariables(void) {
  // check if WiFi SSID has changed. If so, restart cpu. Otherwise, the program will not use the new SSID
  if ((strcmp(lastWiFiSSID, "") != 0) && (strcmp(lastWiFiSSID, iotWebConf.getWifiSsidParameter()->valueBuffer) != 0)) {
    diag_restart("WLAN-SSID geaendert");
  }
  strcpy(lastWiFiSSID, iotWebConf.getWifiSsidParameter()->valueBuffer);

  speakerTick = speakerTickParam.isChecked();
  playSound = startSoundParam.isChecked();
  ledTick = ledTickParam.isChecked();
  showDisplay = showDisplayParam.isChecked();

  sendToCommunity = sendToCommunityParam.isChecked();
  sendToMadavi = sendToMadaviParam.isChecked();
  sendToLora = sendToLoraParam.isChecked();
  sendToBle = sendToBleParam.isChecked();

  soundLocalAlarm = soundLocalAlarmParam.isChecked();
  localAlarmThreshold = localAlarmThresholdParam.value();
  localAlarmFactor = localAlarmFactorParam.value();

  sendDataToMessengerEvery = sendDataToMessengerEveryParam.value();
  sendLocalAlarmToMessenger = sendLocalAlarmToMessengerParam.isChecked();

  // memcpy mit fester Zahl kopiert den Stringinhalt in die vorgegebenen
  // char-Puffer; Längen müssen zu den Feldgrößen passen.
  memcpy(mqttServer, String(mqttServerParam.value()).c_str(), 100);
  mqttPort = mqttPortParam.value();
  memcpy(mqttUsername, String(mqttUsernameParam.value()).c_str(), 30);
  sendDataToMqttEvery = sendDataToMqttEveryParam.value();
  sendLocalAlarmToMqtt = sendLocalAlarmToMqttParam.isChecked();
  memcpy(mqttChannelPrefix, String(mqttChannelPrefixParam.value()).c_str(), 50);

  memcpy(localDeviceName, String(localDeviceNameParam.value()).c_str(), 33);
  memcpy(ntpServer, String(ntpServerParam.value()).c_str(), 64);
  clock_set_ntp_server(ntpServer);  // takes effect immediately if NTP is already running

  memcpy(staticIp, String(staticIpParam.value()).c_str(), 16);
  memcpy(staticGateway, String(staticGatewayParam.value()).c_str(), 16);
  memcpy(staticNetmask, String(staticNetmaskParam.value()).c_str(), 16);
  memcpy(staticDns, String(staticDnsParam.value()).c_str(), 16);
  // A changed IP configuration only takes effect on a new connection: restart cleanly.
  char ip_config[70];
  snprintf(ip_config, sizeof(ip_config), "%s|%s|%s|%s", staticIp, staticGateway, staticNetmask, staticDns);
  if (lastIpConfig[0] && strcmp(lastIpConfig, ip_config) != 0)
    diag_restart("IP-Einstellungen geaendert");
  strcpy(lastIpConfig, ip_config);

}

// Wird nach dem Speichern aufgerufen und aktualisiert globale Einstellungen
// aus den Parameterobjekten.
void configSaved(void) {
  log(INFO, "Config saved. ");
  loadConfigVariables();
  // Telegram-Melder wird deaktiviert, wenn Token oder Chat-ID
  // nicht die erwartete Mindestlänge haben.
  if ((strlen(telegramBotToken) < 40) || (strlen(telegramChatId) < 7)) {
    sendDataToMessengerEvery = 0;
    sendLocalAlarmToMessenger = false;
  }
  // MQTT-Melder wird deaktiviert, wenn kein brauchbarer Servername
  // gesetzt ist.
  if (strlen(mqttServer) < 5) {
    sendDataToMqttEvery = 0;
    sendLocalAlarmToMqtt = false;
  }
  tick_enable(true);
}

// Initialisiert IotWebConf, Update-Server, Parameter und Web-Handler.
// Empty the text if it is not terminated or contains non-printable bytes (fresh 0xFF memory).
static void sanitize_text_param(char *text, size_t size) {
  text[size - 1] = '\0';
  for (char *c = text; *c; c++) {
    if ((unsigned char)*c < 0x20 || (unsigned char)*c > 0x7E) {
      text[0] = '\0';
      return;
    }
  }
}

// Called before every WiFi.begin(): either set the fixed IP or switch (back) to DHCP.
// Returns true if a fixed IP is in use. Invalid input falls back to DHCP (with a log message).
bool apply_ip_config() {
  IPAddress ip, gateway, netmask(255, 255, 255, 0), dns;
  if (staticIp[0] == '\0') {
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);  // DHCP
    return false;
  }
  if (!ip.fromString(staticIp) || !gateway.fromString(staticGateway) ||
      (staticNetmask[0] && !netmask.fromString(staticNetmask)) ||
      (staticDns[0] && !dns.fromString(staticDns))) {
    log(ERROR, "Fixed IP: invalid settings (IP '%s', gateway '%s', netmask '%s', DNS '%s') - using DHCP",
        staticIp, staticGateway, staticNetmask, staticDns);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    return false;
  }
  if (staticDns[0] == '\0')
    dns = gateway;
  WiFi.config(ip, gateway, netmask, dns);
  log(INFO, "Fixed IP %s, gateway %s, netmask %s, DNS %s", ip.toString().c_str(),
      gateway.toString().c_str(), netmask.toString().c_str(), dns.toString().c_str());
  return true;
}

void setup_webconf() {
  iotWebConf.setConfigSavedCallback(&configSaved);
  // *INDENT-OFF*   <- for 'astyle' to not format the following 3 lines
  iotWebConf.setupUpdateServer(
    [](const char *updatePath) { httpUpdater.setup(&server, updatePath); },
    [](const char *userName, char *password) { httpUpdater.updateCredentials(userName, password); });
  // *INDENT-ON*
  // override the confusing default labels of IotWebConf:
  iotWebConf.getThingNameParameter()->label = "Hostname (Fallback AP-SSID)";
  iotWebConf.getApPasswordParameter()->label = "Admin Password (Fallback AP-Password)";
  iotWebConf.getWifiSsidParameter()->label = "WiFi client SSID";
  iotWebConf.getWifiPasswordParameter()->label = "WiFi client password (max 33 chars!)";

  // add the setting parameter
  grpMisc.addItem(&startSoundParam);
  grpMisc.addItem(&speakerTickParam);
  grpMisc.addItem(&ledTickParam);
  grpMisc.addItem(&showDisplayParam);
  iotWebConf.addParameterGroup(&grpMisc);

  grpTransmission.addItem(&sendToCommunityParam);
  grpTransmission.addItem(&sendToMadaviParam);
  grpTransmission.addItem(&sendToBleParam);
  iotWebConf.addParameterGroup(&grpTransmission);

  // if (isLoraBoard) {
  //   grpLoRa.addItem(&sendToLoraParam);
  //   grpLoRa.addItem(&deveuiParam);
  //   grpLoRa.addItem(&appeuiParam);
  //   grpLoRa.addItem(&appkeyParam);
  //   iotWebConf.addParameterGroup(&grpLoRa);
  // }

  grpAlarm.addItem(&soundLocalAlarmParam);
  grpAlarm.addItem(&localAlarmThresholdParam);
  grpAlarm.addItem(&localAlarmFactorParam);
  iotWebConf.addParameterGroup(&grpAlarm);

  grpMessenger.addItem(&sendDataToMessengerEveryParam);
  grpMessenger.addItem(&sendLocalAlarmToMessengerParam);
  grpMessenger.addItem(&telegramBotTokenParam);
  grpMessenger.addItem(&telegramChatIdParam);
  iotWebConf.addParameterGroup(&grpMessenger);

  grpMqtt.addItem(&sendDataToMqttEveryParam);
  grpMqtt.addItem(&sendLocalAlarmToMqttParam);
  grpMqtt.addItem(&mqttServerParam);
  grpMqtt.addItem(&mqttPortParam);
  grpMqtt.addItem(&mqttUsernameParam);
  grpMqtt.addItem(&mqttPasswordParam);
  grpMqtt.addItem(&mqttChannelPrefixParam);
  iotWebConf.addParameterGroup(&grpMqtt);

  grpDevice.addItem(&localDeviceNameParam);
  grpDevice.addItem(&ntpServerParam);
  iotWebConf.addParameterGroup(&grpDevice);

  grpNetwork.addItem(&staticIpParam);
  grpNetwork.addItem(&staticGatewayParam);
  grpNetwork.addItem(&staticNetmaskParam);
  grpNetwork.addItem(&staticDnsParam);
  iotWebConf.addParameterGroup(&grpNetwork);  // must stay the LAST group, see above

  // Connect to the home WiFi right away instead of first opening the own access point for the
  // "Startup delay" (default 30 s). The AP stays available as fallback: IotWebConf still starts in
  // AP mode if no WiFi/admin password is configured, and falls back to it if connecting fails.
  // (Befund docs/WLAN-START-BEFUND-2026-11-23.md)
  iotWebConf.skipApStartup();

  iotWebConf.init();

  // A configuration saved by an older firmware ends before the newer parameters: the ESP32
  // EEPROM emulation fills the new bytes with 0xFF. Treat anything non-printable as "empty".
  sanitize_text_param(ntpServerParam.value(), sizeof(ntpServerParam.value()));
  sanitize_text_param(staticIpParam.value(), sizeof(staticIpParam.value()));
  sanitize_text_param(staticGatewayParam.value(), sizeof(staticGatewayParam.value()));
  sanitize_text_param(staticNetmaskParam.value(), sizeof(staticNetmaskParam.value()));
  sanitize_text_param(staticDnsParam.value(), sizeof(staticDnsParam.value()));

  loadConfigVariables();

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/diag", handleDiag);
  server.on("/config", [] { iotWebConf.handleConfig(); });
  server.onNotFound([]() {
    iotWebConf.handleNotFound();
  });
}
