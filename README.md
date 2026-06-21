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
| Statusseite `/status` | einfach | erweitert (Strahlenstatus, System, Ø-Dosis) |
| Default-Hostname | `ESP32-<ID>` | `duogeiger-<ID>` |

---

## Hardware

**Heltec WiFi LoRa 32 V2**
- Chip: ESP32-D0WDQ6, 240 MHz, 320 KB RAM, 8 MB Flash
- Kristall: 26 MHz
- Onboard-LED: GPIO 25
- Boot-Modus: GPIO 0 (PRG-Taste)
- Reset: EN (RST-Taste)

---

## Erstkonfiguration

### 1. Gerät starten

Nach dem ersten Flash erscheint ein WLAN-Accesspoint:

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
http://<IP-Adresse>/status  → Messwerte & Systemstatus
http://<IP-Adresse>/config  → Konfiguration
```

Die aktuelle IP steht auf der Statusseite und wird per MQTT im `info`-Topic übertragen.

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

**System:**
- Version, Röhrentyp, Hostname, IP, Laufzeit, WLAN-Qualität (dBm), MQTT-Status

---

## MQTT

Benötigt WiFi-Verbindung. Server und Zugangsdaten in der Webkonfiguration eintragen.

### Topics

Basis-Topic: `<Prefix>/<Hostname>/`

| Topic | Inhalt | Intervall |
|-------|--------|-----------|
| `.../radiation` | CPM, Dosisleistung_nSvh, Schnitt_nSvh, Zaehlungen, Messzeit_s, HV_Pulse, Strahlenstatus | konfigurierbar (Standard: 250 s) |
| `.../environmental` | Temperatur_C, Luftfeuchte_pct, Luftdruck_hPa | wie radiation (nur wenn Sensor vorhanden) |
| `.../info` | Hostname, IP, Version, TubeType | beim Boot, alle 24 h, bei IP-Änderung (retained) |

Das `info`-Topic wird mit `retain=true` gesendet — ein neu verbundener Client bekommt sofort den letzten Stand.

Der Hostname im Topic entspricht dem in der Webkonfiguration eingestellten Namen (Standard: `duogeiger-<ChipID>`).

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

### Nach dem Flash

Das Gerät startet automatisch, spielt eine kurze Melodie und beginnt zu messen. Die erste Messung erscheint nach ca. 2,5 Minuten auf der Statusseite.

---

## Dokumentation (Upstream)

https://multigeiger.readthedocs.org/ (Englisch + Deutsch)

---

## Changelog

| Commit | Änderung |
|--------|----------|
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
