# DuoGeiger

**GitHub:** https://github.com/jodler80/duogeiger

Fork von [ecocurious/MultiGeiger](https://github.com/ecocurious2/MultiGeiger) — Geigerzähler auf ESP32-Basis mit MQTT, Telegram und erweiterter Statusseite.

## Unterschiede zum Original

| Feature | MultiGeiger | DuoGeiger |
|---------|------------|-----------|
| LoRaWAN | ✓ | ✗ entfernt |
| WiFi / BLE | ✓ | ✓ |
| MQTT | ✗ | ✓ |
| Telegram | ✗ | ✓ |
| Statusseite `/status` | einfach | erweitert (Strahlenstatus, System, Ø-Dosis, Diagnose, Hilfe-Link) |
| Diagnose (MQTT `…/diag`, `/diag`) | ✗ | ✓ Neustarts/-gründe, WLAN-/MQTT-Abbrüche mit Grundcode, Heap, Laufzeiten |
| Offline-Puffer + Last Will | ✗ | ✓ Messungen werden nachgesendet, `…/status` online/offline |
| NTP-Server, feste IP-Adresse | fest einkompiliert / DHCP | in der Webkonfiguration einstellbar |
| Firmware-Update über WLAN | Browser | Browser + PlatformIO (espota) |
| Start bis WLAN/IP | ~30–90 s (AP-Modus zuerst) | ~7–8 s |
| Remote-Syslog | ✗ | ✓ (optional) |
| Tests auf dem PC | ✗ | ✓ `make -C test/host` |
| Quelltext-Kommentare | englisch, knapp | zusätzlich deutsche Erklärungen |
| Default-Hostname | `ESP32-<ID>` | `duogeiger-<ID>` |

---

## Hardware

**Heltec WiFi LoRa 32 V2**
- Chip: ESP32-D0WDQ6, 240 MHz, 320 KB RAM, 8 MB Flash
- Kristall: 26 MHz
- Onboard-LED: GPIO 25
- Boot-Modus: GPIO 0 (PRG-Taste)
- Reset: EN (RST-Taste)

Andere ESP32-Boards (z. B. LilyGO T3) sind **nicht** pinkompatibel zur Geiger-Platine: andere Stiftleisten-Belegung,
und u. a. GPIO 22/23 sind dort mit OLED bzw. LoRa belegt (hier: Hochspannungs-Steuerung).

---

## Erstkonfiguration

### 1. Gerät starten

Nach dem ersten Flash (noch keine WLAN-Daten) erscheint ein WLAN-Accesspoint. Später öffnet das Gerät ihn nur noch,
wenn die Verbindung zum Heim-WLAN scheitert:

| Parameter | Wert |
|-----------|------|
| SSID | `duogeiger-<ChipID>` |
| Passwort | **`ESP32Geiger`** |

### 2. Webkonfiguration öffnen

Nach Verbindung mit dem AP im Browser aufrufen:

```
http://192.168.4.1
```

Login für `/config`:
- **Benutzername:** `admin`
- **Passwort:** `ESP32Geiger` (= das AP-Passwort, beim ersten Start ändern!)

### 3. Im eingebauten WLAN

Sobald das Gerät mit dem Heimnetz verbunden ist, ist die Oberfläche unter seiner IP erreichbar:

```
http://<IP-Adresse>/        → Startseite
http://<IP-Adresse>/status  → Messwerte, Systemstatus & Diagnose
http://<IP-Adresse>/diag    → Diagnosewerte als JSON (z. B. für curl/FHEM)
http://<IP-Adresse>/config  → Konfiguration
```

Die aktuelle IP steht auf der Statusseite und wird per MQTT im `info`-Topic übertragen.
Die Statusseite verlinkt unten auf `/config` und auf diese Anleitung („Hilfe“). Das Firmware-Update (`/firmware`)
ist ganz unten auf `/config` verlinkt.

---

## Statusseite `/status`

Die Statusseite zeigt:

**Strahlenstatus** (farbiger Badge, direkt nach dem Titel):

| Farbe | Status | Schwelle |
|-------|--------|---------|
| Grün | Normal | < 1,5 µSv/h |
| Gelb | Erhöht | 1,5 – 10 µSv/h |
| Orange | Deutlich erhöht | 10 – 80 µSv/h |
| Rot | Hohe Strahlung | 80 – 350 µSv/h |
| Dunkelrot | ☢ STRAHLUNGSALARM | ≥ 350 µSv/h |

Referenz: [Wikipedia – Radiologischer Notfall: Dosis-Eckwerte](https://de.wikipedia.org/wiki/Radiologischer_Notfall#Dosis-Eckwerte)

**Strahlung:**
- CPM, Dosisleistung (nSv/h), Ø seit Start (nSv/h), Zählungen, Messzeit, HV-Pulse

**Umgebung** (nur mit BME280): Temperatur, Luftfeuchte, Luftdruck (hPa)

**System:**
- Version, Röhrentyp, Hostname, IP („fest“ oder „DHCP“), Laufzeit, WLAN-Qualität (dBm), MQTT-Status

**Diagnose:**
- Neustarts, Uhrzeit (NTP) synchron?, letzter Reset-Grund, WLAN-Kanal/AP, Reconnects, MQTT-Status und Puffer, Heap,
  längste Schleife, HTTP-Fehler, Messungsnummer; Link auf `/diag` (JSON)

---

## MQTT

Benötigt WiFi-Verbindung. Server und Zugangsdaten in der Webkonfiguration eintragen.

### Topics

Basis-Topic: `<Prefix>/<Hostname>/`

| Topic | Inhalt | Intervall |
|-------|--------|-----------|
| `.../radiation` | seq, ts, CPM, Dosisleistung_nSvh, Schnitt_nSvh, Zaehlungen, Messzeit_s, HV_Pulse, Strahlenstatus | konfigurierbar in der Webkonfiguration (Standard: 150 s, `SEND_DATA_TO_MQTT_EVERY`) |
| `.../environmental` | Temperatur_C, Luftfeuchte_pct, Luftdruck_hPa | wie radiation (nur wenn Sensor vorhanden) |
| `.../diag` | siehe unten | wie radiation |
| `.../backlog` | wie radiation + alter_s + Umweltwerte | nach Reconnect, für gepufferte Messungen |
| `.../status` | `online` / `offline` | beim Verbinden; `offline` setzt der Broker (Last Will, ~45 s nach Verbindungsverlust) — retained |
| `.../info` | Hostname, IP, Version, TubeType | beim Boot, alle 24 h, bei IP-Änderung (retained) |

`info` und `status` werden mit `retain=true` gesendet — ein neu verbundener Client bekommt sofort den letzten Stand.

**seq / ts:** Jede Messung bekommt eine laufende Nummer `seq` (ab Boot) und einen UTC-Zeitstempel `ts` (`null`, solange NTP noch nicht synchronisiert ist). Lücken in `seq` zeigen, dass Messungen verloren gingen (Puffer übergelaufen).

**Puffer (backlog):** Ist WLAN oder MQTT beim Senden nicht verfügbar, wird die Messung im RAM gepuffert (Standard 48 Messungen = 2 h, `MQTT_BACKLOG_SIZE`) und nach dem Reconnect auf `.../backlog` nachgesendet — mit `ts` und `alter_s` (Alter in Sekunden). Bewusst ein eigenes Topic, damit FHEM die aktuellen Readings von `radiation` nicht mit alten Werten überschreibt. Nach einem Neustart ist der Puffer leer.

### Diagnose-Topic `.../diag`

| Feld | Bedeutung |
|------|-----------|
| `seq` | Nummer der letzten Messung |
| `uptime_s`, `boot_count` | Laufzeit; Neustart-Zähler (im NVS, überlebt Stromausfall) — unterscheidet echte Neustarts von Reconnects |
| `ntp_ok` | Uhrzeit per NTP synchronisiert |
| `reset_reason` | `PowerOn`, `Software`, `Panic`, `TaskWatchdog`, `InterruptWatchdog`, `Brownout`, … (siehe Hinweis unten) |
| `restart_reason` | vom Gerät selbst gespeicherter Grund vor einem Neustart (z. B. `Firmware-Update`, `WLAN-SSID geaendert`), sonst leer |
| `rssi`, `channel`, `bssid` | Funkqualität und verbundener Access Point |
| `wifi_reconnects`, `mqtt_reconnects` | erfolgreiche Neuverbindungen seit Boot |
| `wifi_disconnects`, `wifi_disc_reason`, `wifi_disc_reason_name` | WLAN-Abbrüche/Fehlversuche seit Boot und Grundcode des letzten (z. B. 200 `BEACON_TIMEOUT` = Funk/Strom, 2/3/8 = vom AP abgemeldet, 202 `AUTH_FAIL`) |
| `mqtt_state` | `PubSubClient::state()` (0 = verbunden, −2 = Verbindung fehlgeschlagen, −3 = Verbindung verloren, −4 = Timeout, 5 = nicht autorisiert) |
| `heap_free`, `heap_min`, `heap_maxblock` | freier Heap, Minimum seit Boot, größter zusammenhängender Block |
| `loop_max_ms` | längster Durchlauf von `loop()` seit der letzten Diag-Meldung |
| `http_err`, `http_last_ms` | Fehler seit Boot und Dauer des letzten Uploads zu sensor.community |
| `backlog`, `backlog_dropped` | gepufferte Messungen; wegen vollem Puffer verworfene |

Der Hostname im Topic entspricht dem in der Webkonfiguration eingestellten Namen (Standard: `duogeiger-<ChipID>`).

**Hinweis `reset_reason`:** Nach dem Einstecken meldet das Heltec-Board oft `OtherWatchdog` statt `PowerOn`: Der allererste
Bootversuch scheitert (`flash read err`, langsam ansteigende Spannung), der RTC-Watchdog startet neu, danach läuft alles normal.
Ein Absturz im Betrieb zeigt sich als `Panic`/`TaskWatchdog` oder `OtherWatchdog` bei vorher hohem `uptime_s`.

---

## WLAN

- Hostname im Router = in der Webkonfiguration eingestellter Name (setzt IotWebConf vor `WiFi.begin()`).
- WLAN-Stromsparmodus ist aus (`WIFI_NO_SLEEP`), solange BLE nicht aktiv ist — BLE und WLAN brauchen gemeinsam den Modem-Sleep.
- Sendeleistung 19,5 dBm (Maximum).
- Kein Access-Point-Modus mehr beim Start: das Gerät verbindet sich sofort mit dem Heim-WLAN (`skipApStartup()`).
  Den eigenen Access Point `duogeiger` öffnet es nur noch, wenn keine WLAN-Daten eingerichtet sind oder die Verbindung scheitert.
- Verbindungs-Timeout 120 s (`WIFI_CONNECTION_TIMEOUT_MS`): erst danach fällt IotWebConf kurz in den eigenen Access-Point-Modus
  zurück. Hintergrund: Mit dem früheren AP-Start dauerte es nach dem Wechsel AP → Heim-WLAN 24–60 s bis zur IP-Adresse
  (DHCP-Server antwortet in Millisekunden, die Verzögerung lag im ESP32). Ohne AP-Start: **ca. 7–8 s vom Einschalten bis zur IP**
  (Befund: `docs/WLAN-START-BEFUND-2026-11-23.md`). Ein Rückfall in den AP-Modus führt wieder über diesen langsamen Weg,
  daher das lange Zeitlimit.
- Beim ersten Verbindungsversuch nach einem Neustart meldet der ESP32 meist `AUTH_FAIL` (202): Die Fritzbox hält noch die alte
  Anmeldung. Der Core versucht es sofort erneut (0,1 s). Scheitert es danach endgültig (`WL_CONNECT_FAILED`), startet die
  Firmware nach 5 s selbst einen neuen Versuch.
- Beim Start listet das Log alle sichtbaren Access Points mit Signalstärke (`WiFi scan: …`) – zum Vergleich mit anderen Geräten
  am selben Ort (Antennen-/Standortprüfung).
- Bei RSSI unter ca. −75 dBm hilft Software kaum noch: Standort, Repeater oder Board mit Antennenbuchse.
- Ein Firmware-Update über WLAN braucht eine stabile Verbindung; reißt sie ab, meldet der Browser eine leere Antwort
  (`NS_ERROR_NET_EMPTY_RESPONSE`), die alte Firmware läuft unverändert weiter.

## Feste IP-Adresse

Unter `/config` → „Netzwerk (feste IP-Adresse)“: IP, Gateway, Netzmaske (leer = 255.255.255.0) und DNS (leer = Gateway).
Bleibt das IP-Feld leer, holt sich das Gerät die Adresse per DHCP (Standard). Nach dem Speichern geänderter Werte
startet das Gerät neu (`restart_reason`: „IP-Einstellungen geaendert“). Ungültige Eingaben → DHCP mit Fehlermeldung im Log.
Die Statusseite zeigt hinter der IP „(fest)“ oder „(DHCP)“.

Die feste Adresse sollte außerhalb des DHCP-Bereichs liegen oder dort für dieses Gerät reserviert sein. Den Start
beschleunigt sie kaum: Der DHCP-Server antwortet in Millisekunden, die Wartezeit entsteht davor (siehe Abschnitt WLAN).

## Uhrzeit (NTP)

Die Uhrzeit wird per NTP geholt (UTC). Optional zuerst ein eigener Server, z. B. der Router:
`/config` → „Generic Device Settings“ → **NTP-Server** (z. B. `fritz.box`), wird nach dem Speichern sofort übernommen.
Voreinstellung für neue Geräte in `userdefines.h`: `#define NTP_SERVER "fritz.box"`.

Antwortet er nicht, wechselt der SNTP-Client des ESP32 selbständig zu `pool.ntp.org` bzw. `ptbtime1.ptb.de` (reihum). Status: `ntp_ok` im `diag`-Topic, „Uhrzeit (NTP)“ auf `/status`; ohne Sync alle 5 min eine Warnung im Log. Zeitstempel `1970-01-01…` im Log bedeuten: noch keine Zeit.

## Remote-Syslog

Alle Logmeldungen können zusätzlich per UDP an einen Syslog-Server gehen (z. B. FHEM, rsyslog, Synology). In `userdefines.h`:

```c
#define SYSLOG_SERVER "192.168.178.10"
#define SYSLOG_PORT 514
```

---

## Telegram

Bot-Token und Chat-ID in der Webkonfiguration eintragen.
Einstieg: https://core.telegram.org/bots

---

## Compilieren

### Voraussetzungen

- [PlatformIO](https://platformio.org/) (VS Code Extension oder CLI)
- Python 3.x

### Build

```bash
cd duogeiger
pio run -e geiger
# oder:
python -m platformio run -e geiger
```

### userdefines.h anlegen

Vor dem ersten Build die Beispieldatei kopieren:

```bash
cp duogeiger/userdefines-example.h duogeiger/userdefines.h
```

Die Datei enthält Tube-Typ und weitere Compile-Zeit-Einstellungen (nicht im Git).

---

### Tests auf dem PC

Die hardwareunabhängige Logik (Strahlenstatus, Offline-Puffer, JSON-Format, MQTT-Topic) liegt in
`duogeiger/mqtt_logic.h` und wird ohne ESP32 mit `g++` getestet:

```bash
make -C test/host
```

---

## Firmware flashen

### Voraussetzungen

- Python mit esptool: `pip install esptool`
- Gerät über USB verbunden (COM6 unter Windows, ggf. anpassen)

### Bootloader-Modus aktivieren

Das Gerät ist **empfindlich gegenüber USB-Verbindungsproblemen**. Bei Verbindungsfehlern:
1. USB-Kabel abziehen
2. An einem anderen USB-Port / USB-Hub neu einstecken
3. Bootloader-Modus manuell aktivieren:
   - **PRG**-Taste gedrückt halten
   - **RST**-Taste kurz drücken und loslassen
   - **PRG**-Taste loslassen
   - Gerät sollte jetzt still sein (kein Piepton = Download-Modus aktiv)

### Flash-Befehl

```powershell
python -m esptool --chip esp32 --port COM6 --baud 115200 `
  --before no-reset --connect-attempts 3 write-flash `
  0x1000  ".pio\build\geiger\bootloader.bin" `
  0x8000  ".pio\build\geiger\partitions.bin" `
  0x10000 ".pio\build\geiger\firmware.bin"
```

**Wichtige Hinweise:**
- Nur **115200 Baud** ist stabil — höhere Baudraten brechen nach `bootloader.bin` ab
- `--before no-reset` ist nötig, da der Auto-Reset nicht zuverlässig funktioniert
- esptool **v5.x** schlägt mit `Invalid head of packet` fehl → stattdessen esptool aus PlatformIO nutzen (v4.x):
  ```powershell
  python -m platformio run -e geiger --target upload
  ```

### OTA-Update über WLAN

Ist das Gerät im WLAN, kann die Firmware ohne USB aktualisiert werden:

- **Browser:** `http://<IP-Adresse>/firmware` (Login `admin` + Admin-Passwort), `firmware.bin` hochladen.
- **PlatformIO (espota):** Abschnitt `[env:geiger_ota]` aus `platformio-example.ini` in `platformio.ini` übernehmen, `upload_port` und `--auth` (Admin-Passwort) anpassen, dann:
  ```bash
  pio run -e geiger_ota -t upload
  ```
  Port 3232/UDP+TCP muss zwischen PC und Gerät erreichbar sein. Abschaltbar mit `ENABLE_ARDUINO_OTA false`.

### Nach dem Flash

Das Gerät startet automatisch, spielt eine kurze Melodie und beginnt zu messen. Die erste Messung erscheint nach ca. 2,5 Minuten auf der Statusseite.

---

## Dokumentation (Upstream)

https://multigeiger.readthedocs.org/ (Englisch + Deutsch)

---

## Changelog

| Commit | Änderung |
|--------|----------|
| 23.11.2026 | WLAN-Start ohne AP-Modus (`skipApStartup`, ~7–8 s bis IP); feste IP-Adresse und NTP-Server in der Webkonfiguration; WLAN-Abbruch-Grundcodes und Start-Scan; Neuversuch nach `AUTH_FAIL`; Hilfe-Link; `pinMode(-1)`-Meldung beseitigt; deutsche Code-Kommentare |
| 23.11.2026 | Diagnose (`…/diag`, `/diag`, Statusseite), Last Will, Offline-Puffer `…/backlog`, `seq`/`ts`, NTP-Fallback, Remote-Syslog, ArduinoOTA, WLAN-Tuning; Logik in `mqtt_logic.h` mit PC-Tests; Bugfix Luftdruck Pa→hPa |
| 23.11.2026 | Bugfixes: MQTT-Felder aus *einem* Intervall, `info` gleich beim Boot korrekt, Schwellwerte nur an einer Stelle, Division durch 0 im Ton-Interrupt; LF-Zeilenenden |
| `78dbe27` | Statusseite: einheitliche Tabellenbreite und Ausrichtung |
| `6c1e8b6` | Bugfix: Stack-Overflow beim Aufruf von `/status` nach längerem Betrieb |
| `8d37770` | Statusseite: Strahlenstatus-Badge, Systeminfos, Ø-Dosis; MQTT: geteilte Topics, deutsche Feldnamen, Hostname als Device-ID |
| `8d02aa2` | Bugfix: ISR-Crash durch MCPWM-API-Aufruf während NVS-Schreibzugriff |
| `fc5d491` | THp-Sensor: BSEC/BME680 entfernt, nur noch BME280 |

---

## Links

- Dieses Projekt: https://github.com/jodler80/duogeiger
- Upstream-Projekt: https://github.com/ecocurious2/MultiGeiger
- Multigeiger-Karte: https://multigeiger.ecocurious.de/
- Ecocurious-Projektseite: https://ecocurious.de/projekte/multigeiger-2/
