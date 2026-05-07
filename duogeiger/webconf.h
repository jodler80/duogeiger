// Web Configuration related code
// also: OTA updates

#ifndef _WEBCONF_H_
#define _WEBCONF_H_

#include "IotWebConf.h"

extern bool speakerTick;
extern bool playSound;
extern bool ledTick;
extern bool showDisplay;

extern bool sendToCommunity;
extern bool sendToMadavi;
extern bool sendToLora;
extern bool sendToBle;

extern char appeui[17];
extern char deveui[17];
extern char appkey[IOTWEBCONF_WORD_LEN];

extern bool soundLocalAlarm;
extern float localAlarmThreshold;
extern int localAlarmFactor;

extern char telegramBotToken[50];
extern char telegramChatId[15];
extern long sendDataToMessengerEvery;
extern bool sendLocalAlarmToMessenger;

extern char mqttServer[100];
extern int mqttPort;
extern char mqttUsername[30];
extern char mqttPassword[30];
extern char mqttChannelPrefix[50];
extern long sendDataToMqttEvery;
extern bool sendLocalAlarmToMqtt;

extern char localDeviceName[33];

extern char ssid[];
extern IotWebConf iotWebConf;

void setup_webconf();

#endif // _WEBCONF_H_
