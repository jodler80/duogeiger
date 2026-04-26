## DuoGeiger

### EN
This is a fork from [ecocurious's Multigeiger](https://github.com/ecocurious2/MultiGeiger) project:

The MultiGeiger is a radioactivity measurement device, based on an ESP32 microcontroller. 
The DuoGeiger is a modified version of the original Multigeiger:

* Removed LoRaWAN modules, Wifi/BLE radio support only
* Added MQTT transmission
* Added option to send messages and alerts to Telegram

Both additional modules can be configured in the web interface.

The documentation points to the original Multigeiger, but most of it is still valid for the DuoGeiger.

### DE

Der MultiGeiger ist ein Messgerät für Radioaktivität auf Basis eines ESP32-Mikrocontrollers. 
Der DuoGeiger ist eine modifizierte Version des ursprünglichen Multigeigers:

* LoRaWAN-Module entfernt, Funkübertragung nur noch via Wifi/BLE
* MQTT-Übertragung hinzugefügt
* Option zum Senden von Nachrichten und Warnungen an Telegram hinzugefügt

Beide zusätzlichen Module können in der Weboberfläche konfiguriert werden.

Die Dokumentation verweist auf den ursprünglichen Multigeiger, aber die meisten Informationen sind auch für den DuoGeiger gültig.


## Telegram - REQUIRES WIFI CONNECTION!

To communicate with the Telegram Messenger on your phone you need to create a bot.

Starting point: https://core.telegram.org/bots

You will get a Bot token, in this form: ```"XXXXXXXXXX:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"```

In order to get MultiGeiger messages to your specific chat, you need the ChatID in this form ```"123456789"```

Add the bot to a group together with @RawDataBot, which will print the ChatID, s. e.g. [here at stackoverflow](https://stackoverflow.com/questions/32423837/telegram-bot-how-to-get-a-group-chat-id)

Please provide the token and the ChatID via Web Config.

## MQTT - REQUIRES WIFI CONNECTION!

To communicate with MQTT broker, please provide server address and credentials via Web Config

MQTT channel/topic prefix can be defined in Web Config. 

Final channels/topics are
* ```<prefix>/esp32-<###>/radiation``` for radiation data (cpm, accumulated cpm, rate, accumulated rate in nSv/h)
* ```<prefix>/esp32-<###>/environmental``` for environmental data if installed (temperature, rel. humidity, air pressure)


## Documentation

https://multigeiger.readthedocs.org/  (english + deutsch)

There is our new, versioned and translated documentation (see the box at the
lower right there to switch languages and documentation versions)!

Dort ist unsere neue, versionierte und übersetzte Doku (siehe die Box
rechts unten, mit der man Sprache und Version umschalten kann)!

## Quicklinks

* Default Wifi-Password: "ESP32Geiger"
* Multigeiger map unfortunately offline / Karte leider inzwischen offline

* Übersichtsseite bei Ecocurious / Ecocurious project overview page (German):

  https://ecocurious.de/projekte/multigeiger-2/

