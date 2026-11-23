// Web Configuration related code
// also: OTA updates

#ifndef _WEBCONF_H_
#define _WEBCONF_H_

#include "IotWebConf.h"

extern bool speakerTick;
extern bool playSound;
extern bool ledTick;
extern bool showDisplay;

// Schaltet Web-, Netz- und Funk-Wege ein oder aus.
extern bool sendToCommunity;
extern bool sendToMadavi;
extern bool sendToLora;
extern bool sendToBle;

// LoRaWAN-Schlüssel (AppEUI/DevEUI: 16 Hex-Zeichen + Nullbyte); im DuoGeiger ungenutzt.
extern char appeui[17];
extern char deveui[17];
extern char appkey[IOTWEBCONF_WORD_LEN];

extern bool soundLocalAlarm;
extern float localAlarmThreshold;
extern int localAlarmFactor;

// Telegram-Zugangsdaten und Meldeintervall.
extern char telegramBotToken[50];
extern char telegramChatId[15];
extern long sendDataToMessengerEvery;
extern bool sendLocalAlarmToMessenger;

// MQTT-Serverdaten, Kanalpräfix und Sendetakt.
extern char mqttServer[100];
extern char ntpServer[64];
extern char staticIp[16];

// Fixed IP or DHCP according to the web configuration; call before WiFi.begin().
bool apply_ip_config();
extern int mqttPort;
extern char mqttUsername[30];
extern char mqttPassword[30];
extern char mqttChannelPrefix[50];
extern long sendDataToMqttEvery;
extern bool sendLocalAlarmToMqtt;

// Frei wählbarer Gerätename; steht in den Telegram-Nachrichten vor den Messwerten.
extern char localDeviceName[33];

extern char ssid[];
extern IotWebConf iotWebConf;

// Richtet Browser-Setup, Speichern und OTA-Schnittstelle ein.
void setup_webconf();

#endif // _WEBCONF_H_
