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
extern bool soundLocalAlarm;

extern char appeui[];
extern char deveui[];
extern char appkey[];

extern float localAlarmThreshold;
extern int localAlarmFactor;

extern char localDeviceName[33];

extern long sendDataToMessengerEvery;
extern char telegramBotToken[50];
extern char telegramChatId[15];
extern bool sendLocalAlarmToMessenger;

extern long sendDataToMqttEvery;
extern char mqttServer[100];
extern int mqttPort;
extern char mqttUsername[30];
extern char mqttPassword[30];
extern char mqttChannelPrefix[50];
extern bool sendLocalAlarmToMqtt;

extern char ssid[];
extern IotWebConf iotWebConf;

void setup_webconf();

#endif // _WEBCONF_H_
