# Befund 23.11.2026: Langsame IP-Vergabe nach dem Start liegt nicht am DHCP-Server

**Anlass:** DuoGeiger (Name `duogeiger`, im Heim-WLAN) braucht nach einem Neustart
24–60 s bis zur IP-Adresse. Bisherige Annahme im README: „der DHCP-Server brauchte zeitweise bis 60 s“. Die Messungen
vom 23.11.2026 widerlegen das.

## Wer vergibt die Adressen

Seit 17.09.2026 vergibt im WLAN-Netz <WLAN-Netz> der **Router (MikroTik)** die Adressen (DHCP-Server,
Lease 1 h). Die FritzBox und die Fritz-Repeater sind nur Mesh/IP-Client und reichen
DHCP durch — deshalb zeigt die FritzBox beim DuoGeiger „IP ---“.

## Messungen

1. **Mitschnitt am DHCP-Router** (alle DHCP-Pakete, 17:13:47–17:16:47, Neustart des DuoGeigers durch den Nutzer in diesem Zeitraum):

   | Zeit | Paket |
   |---|---|
   | 17:15:12,929 | DuoGeiger → DHCP-DISCOVER (Hostname `duogeiger`) |
   | 17:15:12,932 | DHCP-Router → OFFER <IP des DuoGeigers> (**3 ms**) |
   | 17:15:13,034 | DuoGeiger → REQUEST |
   | 17:15:13,035 | DHCP-Router → ACK (**1 ms**) |

   Gesamte DHCP-Abwicklung ≈ 0,1 s. Auch alle anderen Geräte im Mitschnitt (PC, Raspberry Pi, weitere Geräte …) bekamen ihre
   Antwort in 2–5 ms. Vor dem DISCOVER um 17:15:12 kam **kein** Paket des DuoGeigers an — keine verlorenen, wiederholten Anfragen.
2. **Gegenprobe mit Vergleichs-PC** im selben WLAN (AP FritzBox): WLAN-Anmeldung fertig → IP-Adresse in **≈ 0,2 s**.
3. **DHCP-Router-Log** der letzten Neuverbindungen: „deassigned“/„assigned“ jeweils in derselben Sekunde.
4. Nach einem Neustart war in der WLAN-Liste des Vergleichs-PCs ein eigenes WLAN **„duogeiger“** (BSSID des DuoGeigers) sichtbar —
   der DuoGeiger stand also zunächst im Access-Point-Modus.

## Ursache (sehr wahrscheinlich)

**IotWebConf startet nach jedem Boot zuerst im eigenen Access-Point-Modus** und verbindet sich erst danach mit dem
konfigurierten WLAN. Aus `IotWebConf.h`:

> „By default IotWebConf starts up in AP mode. Calling this method before the init will force IotWebConf to connect
> immediately to the configured WiFi network.“ — `void skipApStartup()`

Die Dauer ist der Parameter „Startup delay (seconds)“ (`iwcApTimeout`), Voreinstellung
`IOTWEBCONF_DEFAULT_AP_MODE_TIMEOUT_SECS "30"`, in der Webkonfiguration änderbar. Die Firmware ruft `skipApStartup()` **nicht**
auf (`duogeiger/webconf.cpp`, `iotWebConf.init()` Zeile ~633). Damit erklären sich ≈ 30 s Startverzögerung plus
WLAN-Anmeldung und DHCP; mit einem höher eingestellten „Startup delay“ entsprechend länger.

## Mögliche Änderungen an der Firmware (Stand vor dem Nachtrag – Punkt 1 und 4 inzwischen umgesetzt)

1. `iotWebConf.skipApStartup();` vor `iotWebConf.init()` aufrufen → Gerät verbindet sich sofort mit dem Heim-WLAN.
   Der AP-Modus bleibt als Rückfall erhalten, wenn die Verbindung innerhalb von `WIFI_CONNECTION_TIMEOUT_MS` scheitert,
   und über den Config-Pin erzwingbar (Einrichtung eines neuen Geräts prüfen: ohne gültige WLAN-Daten startet IotWebConf ohnehin im AP).
2. Alternativ nur den „Startup delay“ in der Webkonfiguration auf wenige Sekunden senken (kein Neubau nötig, gilt je Gerät).
3. `WIFI_CONNECTION_TIMEOUT_MS` (120 s) wurde wegen des vermeintlich langsamen DHCP-Servers erhöht — nach Änderung 1 prüfen,
   ob ein kürzerer Wert sinnvoller ist.
4. README-Abschnitte „WLAN“ und „Feste IP-Adresse“ korrigieren: Die Verzögerung kommt aus dem AP-Start, nicht vom DHCP-Server;
   eine feste IP-Adresse beschleunigt den Start nicht.

**Nachweis nach einer Änderung:** Neustart mit Zeitnotiz, gleichzeitig Mitschnitt am DHCP-Router (`/tool sniffer`, Port 67/68) —
erstes DISCOVER sollte wenige Sekunden nach dem Neustart kommen, kein WLAN „duogeiger“ mehr sichtbar.

## Nachtrag 23.11.2026, 17:30 – Messung auf DuoGeiger-Seite, Änderung 1 umgesetzt

**Vorher** (serielles Log, Zeit ab „Connecting to [<Heim-WLAN>]“, also *nach* der 30-s-AP-Phase):

| Start | „Connecting“ → IP |
|---|---|
| 16:00 | Abbruch nach 60 s (Fritzbox: angemeldet 16:00:59, „IP ---“) |
| 16:04 | 59 s |
| 16:08 | 24 s |
| 16:40 | 2 s |

Zusammen mit dem DHCP-Router-Mitschnitt (erstes DISCOVER ~1 min nach Ende der AP-Phase, DHCP selbst 0,1 s): Die Verzögerung
entstand **auf dem ESP32 nach dem Wechsel AP-Modus → Heim-WLAN**, vor dem ersten DHCP-Paket – zusätzlich zu den 30 s AP-Phase.

**Umgesetzt:** `iotWebConf.skipApStartup()` vor `iotWebConf.init()` (`duogeiger/webconf.cpp`).

**Nachher** (Testbuild mit `CORE_DEBUG_LEVEL=4`, drei Neustarts per USB-Reset, 17:20–17:24):

| Start | Boot → „Connecting“ | `AUTH_FAIL` → `STA_CONNECTED` → `STA_GOT_IP` | Boot → IP |
|---|---|---|---|
| 1 | 7,4 s | 0,1 s → 0,2 s | 7,7 s |
| 2 | 7,2 s | 0,1 s → 0,2 s | 7,5 s |
| 3 | 7,3 s | 0,1 s → 0,2 s | 7,6 s |

Die 7 s bis „Connecting“ sind WLAN-Scan (Diagnose) + Initialisierung. Das `AUTH_FAIL` (202) beim ersten Versuch bleibt
(Fritzbox hält die alte Anmeldung nach dem Reset), der Core wiederholt sofort.

**Offen:** Punkt 3 (`WIFI_CONNECTION_TIMEOUT_MS` 120 s) bleibt vorerst – ein Rückfall in den AP-Modus führt wieder über
den langsamen Wechsel AP → WLAN, daher lieber länger im WLAN-Versuch bleiben. Punkt 4 (README) korrigiert.
Danach Normal-Firmware ohne Debug-Ausgaben geflasht (17:29).
