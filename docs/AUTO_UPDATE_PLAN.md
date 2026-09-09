# Plan: Wiedereinführung von Update-Suche und Auto-Update (Firmware + WebUI)

Status: **Entwurf / Planung** — keine Implementierung. Zielversion: ab `2.3.0`.
Bezug: `docs/OTA_TEST_PROTOCOL.md`, `docs/WEBUI_UPDATES.md`, `docs/RELEASE_VERSIONING.md`.

---

## 0. Ausgangslage

Die frühere Online-Update-Funktion wurde vollständig entfernt (`/api/check_update`,
`/api/ota_url`, `/api/changelog`, `/api/firmware_archive`, eingebettetes
`archive.json.gz`, MQTT/Home-Assistant-Update-Trigger). Übrig ist genau ein Pfad:
manueller Raw-Upload nach `POST /ota_update` bzw. `POST /api/webui/update`.

Gleichzeitig ist die *schwierige* Hälfte bereits gebaut und stabil:

| Vorhanden | Wo |
|---|---|
| Streaming-Flash der Firmware mit 4-KiB-Puffer, Exklusiv-Gate, Fail-Safe-Abbruch | `main/webui_ota.cpp` |
| Heap-Freigabe vor dem Flash (`prepare_ota_heap()` → `monitoring_pause_for_ota()`) | `main/webui_ota.cpp` |
| Streaming-Flash des WebUI-Images inkl. SHA-256, NVS-Transaktionsmarker, Manifest- und API-Kompatibilitätsprüfung, Embedded-Fallback | `main/webui_storage.cpp` |
| Serialisierung aller ausgehenden TLS-Verbindungen | `g_net_fetch_mutex` (`main/monitoring.cpp`) |
| Bewährtes HTTPS-Client-Muster mit absoluter Deadline, `is_async`, Cert-Bundle | `main/events.cpp` |
| GitHub-Redirect-Fix für Release-Assets (Bug #235) | `main/ota_config.cpp` |
| Bootloader-Rollback inkl. 15-s-Selbsttest-Fenster | `CONFIG_APP_ROLLBACK_ENABLE=y`, `main/main.cpp:83` |
| Fertige Release-Manifeste mit Version, SHA-256, Größe, API-Version, Min-Firmware | `latest.json`, `beta.json`, `archive.json`, `webui-archive.json` |

**Es fehlt nur das Vorderteil:** ein winziger Manifest-Abruf und ein Downloader,
der Bytes in die bereits existierenden Senken schiebt. Genau dort ist es früher
jedes Mal am Speicher gescheitert — deshalb ist dieser Plan primär ein
Speicher-Plan, nicht ein Feature-Plan.

### Harte Randbedingungen der Plattform

- ESP32-WROOM-32, **kein PSRAM** (`# CONFIG_SPIRAM is not set`), ~145 KB
  minimaler freier Heap im Feld (`status/min_free_heap`).
- TLS: `MBEDTLS_DYNAMIC_BUFFER=y`, `DYNAMIC_FREE_CA_CERT=y`,
  `ASYMMETRIC_CONTENT_LEN` (In 16 KiB / Out 4 KiB), volles Cert-Bundle (200 Certs).
  Eine Session kostet im Peak grob 35–45 KB — **eine zur Zeit, nie zwei.**
- Task-Watchdog: `ESP_TASK_WDT_TIMEOUT_S=5`, `PANIC=y`, Idle-Task-Check auf
  **beiden** Cores. Ein blockierender Flash-Erase/Write-Block ohne Yield ist ein
  Reset.
- Das Gerät ist ein Echtzeit-Relay (UDP ↔ UART zur CCU). Ein Update darf den
  Funk-Relay-Pfad nicht spürbar verzögern.

---

## 1. Lehren aus den alten Fehlern (verbindliche Regeln)

Jede Regel unten ist eine Reaktion auf einen real aufgetretenen Fehler. Sie sind
Akzeptanzkriterien, keine Empfehlungen.

| # | Alter Fehler | Neue Regel |
|---|---|---|
| 1 | GitHub-API-JSON (Release-Liste, hunderte KB) im RAM geparst → OOM | Gerät spricht **nie** mit `api.github.com`. Es lädt ein schlankes, festes Geräte-Manifest **≤ 1024 Byte**; alles darüber wird beim Lesen abgebrochen und verworfen. |
| 2 | `archive.json.gz` in die Firmware eingebettet → Flash-Verschwendung + sofort veraltet | Nichts wird eingebettet. Versionshistorie ist ein Link, den der **Browser** öffnet. |
| 3 | Update-Fetch parallel zu MQTT-/Syslog-TLS → zwei Handshakes, Heap weg | Jeder ausgehende Fetch nimmt `g_net_fetch_mutex`. Vor dem Download zusätzlich `net_fetch_set_ota_active(true)`. |
| 4 | Download bei laufendem MQTT/CheckMK/Prometheus/Syslog → Peak zu hoch | `prepare_ota_heap()` läuft **vor** dem TLS-Verbindungsaufbau, nicht danach. Die freigegebenen Worker-Stacks finanzieren die TLS-Session. |
| 5 | `vTaskDelay(pdMS_TO_TICKS(86400000))` → 32-Bit-Überlauf, Timer feuerte viel zu früh | Kein langer Einzel-Delay. Fester **1-Stunden-`esp_timer`** plus Zähler; Intervall wird in Stunden gerechnet. |
| 6 | TLS-Fetch im `esp_timer`-Callback → Stack des Timer-Tasks zu klein, WDT-Reset | Der Timer setzt **nur ein Flag**. Die Arbeit macht ein **transienter Task**, der sich nach getaner Arbeit selbst löscht (`xTaskCreate` → `vTaskDelete(NULL)`, exakt das Muster aus `main.cpp:107`). Im Leerlauf kostet das 0 Byte Heap. |
| 7 | `/api/ota_url` akzeptierte beliebige URLs → SSRF, TLS zu fremden Hosts | Es gibt **keinen** URL-Parameter mehr. Installiert wird ausschließlich die URL aus dem signierten Release-Manifest, zusätzlich Host-Allowlist (`*.github.com`, `*.githubusercontent.com`), nur `https://`, Re-Prüfung **nach** jedem Redirect. |
| 8 | MQTT-/Home-Assistant-Trigger konnten unbeaufsichtigt flashen | v1 hat **keine** Remote-Install-Kommandos. MQTT/HA bekommen nur lesende Sensoren. |
| 9 | Keep-Alive + Redirects brachen GitHub-Asset-Downloads (#235) | `configure_ota_http_client()` unverändert weiterverwenden (Keep-Alive aus, TX-Puffer 2048, ≤ 5 Redirects). |
| 10 | Defektes/abgeschnittenes Image geflasht | Größe gegen Manifest, ESP32-Magic `0xE9`, **SHA-256 vollständig verifiziert bevor** `esp_ota_set_boot_partition()` aufgerufen wird, danach `esp_ota_end()`-Validierung, darüber Bootloader-Rollback. |
| 11 | Neues WebUI auf zu alter Firmware → unbenutzbare Oberfläche | `uiApi` und `uiMinFw` werden **vor dem Download** gegen `webui_api_contract.json` und `version.txt` geprüft. Bei Firmware+WebUI zusammen gilt: Firmware zuerst, Reboot, dann WebUI. |
| 12 | Hängender Fetch → Gerät blockiert, nur Reboot half | Absolute Deadline (10 min Download / 20 s Check), Per-Read-Timeout, Stall-Detektor (30 s ohne Fortschritt → Abbruch). Nach jedem Abbruch ist das Gerät **ohne Reboot** wieder voll funktionsfähig; Worker werden neu gestartet. |
| 13 | Update störte den HomeMatic-Relay-Pfad | Auto-Installation nur im Wartungsfenster und nur bei Idle; im Schreib-Loop expliziter Yield + WDT-Reset. |
| 14 | Jeder Seitenaufruf/Boot löste einen Check aus → Server-Hammering, Flash-Wear | Erster Check frühestens 10 min nach Boot, MAC-abgeleiteter Jitter 0–59 min, Mindestintervall 6 h, exponentielles Backoff bei Fehlern. |

---

## 2. Serverseite (Phase 0) — Risiko null für das Gerät

### 2.1 Gefundenes Problem

`pages.yml` veröffentlicht ausschließlich `./docs`. `latest.json`, `beta.json`,
`archive.json` liegen aber im **Repo-Root** und werden damit *nicht* über GitHub
Pages ausgeliefert. Ein Gerät müsste heute `raw.githubusercontent.com` abrufen
(Redirects, kein CDN-Caching, anderer Trust-Pfad). Das wird zuerst repariert.

### 2.2 Schlankes Geräte-Manifest

Die Release-Workflows erzeugen zusätzlich zu den bestehenden Manifesten je eine
Datei unter einem **versionierten, für immer stabilen Pfad**:

```
https://xerolux.github.io/HB-RF-ETH-ng/updates/v1/stable.json
https://xerolux.github.io/HB-RF-ETH-ng/updates/v1/beta.json
```

Der Pfadbestandteil `v1` ist die Schema-Version. Ein späteres inkompatibles
Schema wird als `v2` daneben veröffentlicht; `v1` wird weiter mitgeschrieben,
damit Altgeräte niemals stehenbleiben.

Format — flach, keine Arrays, keine Freitexte, Zielgröße ≤ 512 Byte,
Hard-Limit 1024 Byte:

```json
{
  "s": 1,
  "fw": "2.3.0",
  "fwUrl": "https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v2.3.0/firmware_2.3.0.bin",
  "fwSha": "<64 hex>",
  "fwSize": 1320848,
  "fwMinFrom": "2.2.0",
  "ui": "1.1.0",
  "uiUrl": "https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v2.3.0/webui_1.1.0.bin",
  "uiSha": "<64 hex>",
  "uiSize": 327680,
  "uiApi": 1,
  "uiMinFw": "2.2.5-Beta.1",
  "notes": "https://github.com/Xerolux/HB-RF-ETH-ng/releases/tag/v2.3.0"
}
```

`fwMinFrom` ist neu: die niedrigste Firmware, von der aus ein **direkter** Sprung
auf dieses Release erlaubt ist (Migrationsschranke, z. B. bei Partitions- oder
NVS-Layout-Änderungen). Ältere Geräte melden „Update nur manuell" statt sich
selbst zu zerlegen.

### 2.3 Betroffene Workflow-Änderungen

- `.github/workflows/release.yml`: erzeugt zusätzlich `docs/updates/v1/{stable,beta}.json`
  und committet sie mit den bestehenden Manifesten nach `main`.
- `.github/workflows/release-webui.yml`: aktualisiert bei einem reinen
  WebUI-Release nur die `ui*`-Felder, lässt `fw*` unangetastet.
- `pages.yml` bleibt unverändert (liefert `./docs` bereits aus).
- Neuer CI-Check in `docs.yml` oder `pr-check.yml`: Manifest ist gültiges JSON,
  ≤ 1024 Byte, alle Pflichtfelder gesetzt, SHA-256 = 64 Hex, URLs `https://` mit
  erlaubtem Host. **Ein zu großes Manifest bricht den Release-Build ab** — so
  kann die Serverseite das Gerät nicht nachträglich in den OOM schieben.

---

## 3. Firmware — Modul 1: Update-Suche (`main/update_check.cpp`)

Reiner Lesepfad. Schreibt nichts in den Flash außer einem 16-Byte-NVS-Wert.

### 3.1 Scheduler

```
esp_timer (periodisch, 1 h)  →  setzt nur ein Flag / gibt eine Semaphore
                              →  Worker-Task wird bei Bedarf erzeugt (4 KiB Stack)
                              →  Fetch, Auswertung, Ergebnis in RAM
                              →  vTaskDelete(NULL)      ← Heap-Kosten im Leerlauf: 0
```

- Intervall in **Stunden** (`upd_interval`, Default 24, Bereich 6…168). Der Zähler
  vergleicht Stunden gegen Stunden — nie wird eine Millisekunden-Dauer > 2^32/100
  an `pdMS_TO_TICKS` übergeben (Regel 5).
- Erster Check: `10 min + (MAC[5] % 60) min` nach Boot. Der Jitter ist
  deterministisch pro Gerät, damit Pages nicht minütlich Lastspitzen sieht.
- Backoff bei Fehler: 1 h → 2 h → 4 h → 8 h, gedeckelt auf das konfigurierte
  Intervall, maximal 3 Versuche pro Fenster.

### 3.2 Vorbedingungen — alle müssen erfüllt sein, sonst wird der Zyklus still übersprungen

1. Link up, IP vorhanden, DNS auflösbar.
2. `esp_get_free_heap_size() > 90 KB` **und**
   `heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) > 48 KB`.
   Fragmentierung ist der eigentliche Killer, nicht die Summe — deshalb werden
   beide Werte geprüft.
3. Keine laufende OTA-/WebUI-/Backup-/Restore-/Factory-Reset-Operation
   (`ota_operation_try_begin()`, `net_fetch_ota_active()`).
4. `g_net_fetch_mutex` innerhalb von 5 s erhältlich, sonst überspringen (nicht warten).
5. `upd_mode != off`.

### 3.3 Fetch

- `configure_ota_http_client()` + `cfg.is_async = true` + absolute Deadline 20 s
  (Muster aus `events.cpp:383`).
- `crash_blackbox_net_op_begin("update_check")` / `..._end()` um den Abschnitt.
- Lesen in **einen** 1024-Byte-Stack-Puffer. Sobald 1024 Byte überschritten
  werden: abbrechen, Fehler „manifest_too_large", nichts parsen.
- `cJSON_Parse` auf dem nullterminierten Puffer (~2–3 KB transient, identisch zum
  bereits erprobten Vorgehen in `webui_storage.cpp:233`).

### 3.4 Ergebnis

- Im RAM: eine ~200-Byte-Struktur (Versionen, Flags, `lastCheck`, `lastError`,
  Notes-URL, Verfügbarkeits-Bits). Keine URLs länger als nötig — die
  Download-URLs werden mitgehalten (2 × 128 Byte), damit `install` keine zweite
  Manifest-Runde braucht.
- In NVS: **eine** Schlüssel-Wert-Zeile `upd_seen` (letzte gesehene Version,
  ≤ 16 Byte), damit die Oberfläche direkt nach dem Boot etwas anzeigen kann,
  ohne einen Fetch auszulösen.
- **Kein Changelog auf dem Gerät.** Die Oberfläche verlinkt `notes` und der
  Browser lädt ihn. Das ersetzt die früheren `/api/changelog` + `marked`-Pfade
  ersatzlos und kostet 0 Byte RAM und 0 Byte Flash.

### 3.5 Versionsvergleich

Ein sauberer SemVer-Vergleich inklusive Prerelease-Ordnung
(`2.2.7-Beta.8 < 2.2.7`) gehört in `main/validation.cpp` und bekommt einen
Unit-Test in `test/` — hier hat sich in der Vergangenheit ein reiner
String-Vergleich als Fehlerquelle erwiesen. Downgrade wird nie automatisch
vorgeschlagen.

---

## 4. Firmware — Modul 2: Download & Installation (`main/update_install.cpp`)

Ein Downloader, zwei Senken. Der Code, der Bytes in den Flash schreibt,
existiert bereits und wird **nicht** dupliziert.

```
                       ┌─ Senke A: esp_ota_write()               (Firmware)
HTTPS-Stream ──4 KiB──►┤
      + SHA-256        └─ Senke B: webui_storage_update_write()  (WebUI/SPIFFS)
```

### 4.1 Ablauf (Firmware)

1. Exklusiv-Reservierung (`ota_operation_try_begin()`), `net_fetch_set_ota_active(true)`,
   `g_net_fetch_mutex` nehmen.
2. Vorprüfung **ohne** Netzwerk: Zielversion > laufende Version,
   laufende Version ≥ `fwMinFrom`, URL `https://` mit erlaubtem Host,
   `fwSize` plausibel (0x10000 … Partitionsgröße).
3. **`prepare_ota_heap()`** — Monitoring-, MQTT-, CheckMK-, Prometheus-, Syslog-
   und Notify-Worker stoppen. Das gibt mehrere 6–8-KB-Stacks und TLS-Zustand frei
   **bevor** die eigene TLS-Session aufgebaut wird. Das ist der zentrale
   Unterschied zur alten Implementierung.
4. `esp_http_client_open()` → Content-Length gegen `fwSize` prüfen → erstes Byte
   auf `0xE9` prüfen → `esp_ota_begin()`.
5. Schleife: `esp_http_client_read()` in den 4-KiB-Puffer →
   `mbedtls_sha256_update()` → `esp_ota_write()` → Fortschritt in `_ota_progress`
   → alle N Blöcke `esp_task_wdt_reset()` und `vTaskDelay(pdMS_TO_TICKS(2))`
   (Regel 13: der UDP-/UART-Relay muss weiterlaufen).
6. Abschluss: SHA-256 gegen `fwSha` — **erst bei Gleichheit** `esp_ota_end()` und
   `esp_ota_set_boot_partition()`. Danach Neustart über die vorhandene
   `full_system_restart_with_reserved_operation()`-Maschinerie.
7. Nach dem Boot greift das bestehende 15-s-Selbsttest-Fenster
   (`main.cpp:83`); scheitert der Start, rollt der Bootloader automatisch auf
   die alte Partition zurück.

### 4.2 Ablauf (WebUI)

Identisch, aber:
- Vorprüfung zusätzlich: `uiApi == supportedApiVersion` und
  laufende Firmware ≥ `uiMinFw` — **vor** dem Download, damit kein Flash gelöscht
  wird, der danach ohnehin verworfen würde.
- Senke ist `webui_storage_update_begin(size, sha)` / `_write()` / `_finish()`.
  SHA-256, NVS-Transaktionsmarker, Manifest- und Kompatibilitätsprüfung sowie der
  Embedded-Fallback laufen dort bereits.
- **Kein Reboot nötig** — nur Remount. Deshalb ist der WebUI-Auto-Update-Pfad
  deutlich risikoärmer als der Firmware-Pfad und wird zuerst freigegeben (§7).

### 4.3 Fehlerbehandlung (nicht verhandelbar)

Bei **jedem** Abbruch — DNS, TLS, HTTP ≠ 200, Größenabweichung, SHA-Fehler,
Stall, Deadline, Stromausfall des Gegenübers:

- Boot-Partition wird **nicht** umgestellt bzw. `webui_storage_update_abort()`
  invalidiert das Teil-Image;
- Puffer freigeben, `crash_blackbox_net_op_end()`, Mutex freigeben,
  `net_fetch_set_ota_active(false)`, Reservierung freigeben;
- gestoppte Monitoring-Worker werden **wieder gestartet**;
- Fehlertext in `/api/ota_status`, Log-Eintrag, Event
  (`events_emit`) — kein stiller Fehlschlag;
- das Gerät ist danach **ohne Reboot** vollständig arbeitsfähig.

### 4.4 Zweistufiges Update (Firmware + WebUI gleichzeitig neu)

Reihenfolge ist zwingend **Firmware → Reboot → WebUI**, weil das neue WebUI die
neue Firmware-API voraussetzen darf. Zustand in NVS: ein Schlüssel `upd_stage`
mit `none | webui_after_fw` plus Zielversion. Nach dem Reboot prüft der
Update-Task diesen Marker, verifiziert erneut gegen das Manifest und installiert
das WebUI. Der Marker hat ein Verfallsdatum (2 Boots), damit ein hängengebliebener
Zustand sich selbst aufräumt.

---

## 5. Firmware — Modul 3: REST-API (`main/update_api.cpp`)

Alle Antworten sind feste, kleine Puffer (≤ 512 Byte), kein dynamisches JSON.
Registrierung wie im Projekt üblich: `extern httpd_uri_t` in
`main/webui_internal.h`, Registrierung in `WebUI::start()` (`main/webui.cpp`).

| Endpunkt | Zweck |
|---|---|
| `GET /api/update/status` | Laufende Versionen, letzte bekannte Versionen, `updateAvailable`-Flags, `lastCheck`, `lastError`, `mode`, `channel`, `notesUrl`, `stage` |
| `POST /api/update/check` | Sofort-Check anstoßen; per `rate_limiter` auf 1×/60 s begrenzt; kehrt sofort zurück, Ergebnis wird über `/api/update/status` abgeholt |
| `POST /api/update/install` | Body ausschließlich `{"target":"firmware"\|"webui"}` — **niemals** eine URL |
| `GET`/`PUT /api/update/config` | `mode`, `channel`, `interval`, `window`, `waitIdle` |

Fortschritt wird **nicht** neu erfunden: Firmware über `/api/ota_status`,
WebUI über `/api/webui/status`. Beide existieren und die Oberfläche kennt sie.

Die manuellen Upload-Endpunkte bleiben unverändert der Wiederherstellungsweg und
werden von diesem Plan nicht angefasst.

---

## 6. Einstellungen, WebUI, MQTT

### 6.1 Neue NVS-Schlüssel (≤ 15 Zeichen, `include/settings.h` + `main/settings.cpp`)

| Schlüssel | Typ | Default | Bedeutung |
|---|---|---|---|
| `upd_mode` | u8 | `off` bei Bestandsgeräten, `notify` bei Werkszustand | `off` / `notify` / `auto_webui` / `auto_all` |
| `upd_channel` | u8 | `stable` | `stable` / `beta` |
| `upd_interval` | u8 | 24 | Stunden, 6…168 |
| `upd_win_start` | u8 | 3 | Beginn Wartungsfenster (Ortszeit) |
| `upd_win_len` | u8 | 2 | Länge in Stunden |
| `upd_idle_only` | u8 | 1 | Nur installieren, wenn ≥ 5 min kein Raw-UART-Client verbunden war |
| `upd_seen` | str | – | Zuletzt gesehene Version (Anzeige ohne Fetch) |
| `upd_stage` | str | – | Zweistufen-Marker (§4.4) |

**Bestandsgeräte starten bewusst auf `off`.** Ein Firmware-Update darf keine
neuen ausgehenden Netzwerkverbindungen einschalten, die der Betreiber nicht
angefordert hat. Der Release-Hinweis muss das ausdrücklich erwähnen.

### 6.2 WebUI (`webui/src/firmwareupdate.vue`)

- Neue Karte „Online-Update" oberhalb der bestehenden Upload-Karte; die
  Upload-Karte bleibt vollständig erhalten.
- Anzeige: laufende Version, verfügbare Version, Kanal, Zeitpunkt des letzten
  Checks, Buttons „Jetzt suchen" / „Installieren".
- Changelog ausschließlich als externer Link (`target="_blank" rel="noopener"`).
- Alle Texte über `vue-i18n`; Styling nur über Tokens, beide Themes gemäß
  `docs/WEBUI_DESIGN_SYSTEM.md`.
- Neuer Zustand im WebUI erhöht **nicht** die API-Version, solange nur Felder
  hinzukommen — es steigt lediglich `minFirmwareVersion` in
  `webui/compatibility.json` (siehe `docs/RELEASE_VERSIONING.md`).

### 6.3 MQTT / Home Assistant — nur lesend

`status/update_available` (bool), `status/latest_version`,
`status/webui_latest_version`. Discovery als `binary_sensor`, **nicht** als
`update`-Entity mit Install-Button. Ferngesteuertes Flashen war einer der Gründe
für den damaligen Rückbau und bleibt in v1 draußen. Eine spätere
`update`-Entity wäre v2 und müsste hinter einem separaten Opt-In-Schalter liegen.

---

## 7. Stufenweiser Rollout

| Stufe | Inhalt | Freigabe erst wenn |
|---|---|---|
| A | Serverseite: schlankes Manifest auf Pages + CI-Größenprüfung | Manifest über HTTPS abrufbar, ≤ 1024 Byte |
| B | Modul 1 (Check) + `/api/update/status` + `/api/update/check` + UI-Anzeige, Modus `notify` | Heap-Messung §8 bestanden, 7 Tage Feldbetrieb ohne Auffälligkeit |
| C | Modul 2 nur für **WebUI** + Modus `auto_webui` | Testprotokoll §8 vollständig grün |
| D | Modul 2 für **Firmware** + Modus `auto_all` mit Wartungsfenster | Stufe C zwei Releases lang stabil |

Jede Stufe ist eigenständig nützlich und einzeln zurücknehmbar. Wenn Stufe D
Probleme macht, bleiben B und C im Feld.

---

## 8. Abnahme: Messen, nicht schätzen

Ergänzt `docs/OTA_TEST_PROTOCOL.md` um einen Abschnitt „Online-Update".

**Harte Speicher-Schranke — Ausschlusskriterium:**
`esp_get_minimum_free_heap_size()` wird vor und nach jedem Szenario abgelesen.

- Update-Check: minimaler freier Heap **> 80 KB**.
- Online-Firmware-Download (mit vorherigem `prepare_ota_heap()`):
  minimaler freier Heap **> 60 KB**.
- Nach Abschluss oder Abbruch muss der freie Heap auf ≤ 4 KB an den Wert vor der
  Operation zurückkehren (Leck-Nachweis) — geprüft über 20 aufeinanderfolgende
  Zyklen.

Wird eine Schranke gerissen, wird die Stufe nicht freigegeben.

**Funktions- und Fehlerszenarien:**

- [ ] Check mit erreichbarem Manifest → korrekte Versionsanzeige
- [ ] Check ohne Internet / ohne DNS → sauberer Fehler, kein Hänger, nächster Zyklus normal
- [ ] Manifest > 1024 Byte → abgewiesen, nichts geparst
- [ ] Manifest mit fremdem Host in `fwUrl` → Installation verweigert
- [ ] Manifest mit falschem SHA-256 → Download läuft, Boot-Partition bleibt unverändert
- [ ] Abgeschnittener Download (Server bricht ab) → Abbruch, Gerät ohne Reboot voll nutzbar
- [ ] Stromausfall mitten im Firmware-Download → alte Firmware bootet
- [ ] Stromausfall mitten im WebUI-Download → NVS-Marker greift, Embedded-Fallback aktiv
- [ ] WebUI mit falscher `uiApi` → gar kein Download, kein Flash-Erase
- [ ] Firmware unter `fwMinFrom` → Hinweis „nur manuell", kein Download
- [ ] Check parallel zu aktivem MQTT-TLS → serialisiert, kein OOM
- [ ] Download parallel zu laufendem CCU-Datenverkehr → Relay-Aussetzer messen
- [ ] Zwei Install-Anfragen gleichzeitig → zweite abgewiesen
- [ ] `off`-Modus → über 48 h **keine** ausgehende Verbindung zu GitHub/Pages
      (per `tcpdump` am Uplink nachgewiesen)
- [ ] Zweistufiges Update (Firmware + WebUI) → korrekte Reihenfolge, Marker sauber aufgeräumt
- [ ] 30 Tage Dauerlauf im `notify`-Modus → kein Heap-Trend nach unten

---

## 9. Betroffene Dateien

**Neu**

| Datei | Inhalt |
|---|---|
| `include/update_check.h` | Öffentliche Schnittstelle: Status-Struktur, Start/Stop, Trigger |
| `main/update_check.cpp` | Scheduler, Vorbedingungen, Manifest-Fetch, Versionsvergleich |
| `main/update_install.cpp` | Streaming-Downloader für beide Senken |
| `main/update_api.cpp` | REST-Handler |

Die Aufteilung folgt der bestehenden Trennung
`webui.cpp` / `webui_backup.cpp` / `webui_ota.cpp` und verhindert, dass
`webui_ota.cpp` (bereits 691 Zeilen) weiter wächst.

**Geändert**

`main/CMakeLists.txt` · `main/webui_internal.h` · `main/webui.cpp` (Routen) ·
`include/settings.h` + `main/settings.cpp` · `main/validation.cpp` (SemVer) ·
`main/monitoring.cpp` / `main/mqtt_handler.cpp` (lesende Sensoren) ·
`webui/src/firmwareupdate.vue` + `webui/src/locales/*` ·
`docs/API.md` · `docs/openapi.yaml` · `docs/OTA_TEST_PROTOCOL.md` ·
`.github/workflows/release.yml` · `.github/workflows/release-webui.yml` ·
`.github/workflows/pr-check.yml` (Manifest-Validierung) ·
`test/` (SemVer- und Manifest-Parser-Tests)

**Bewusst unverändert:** `partitions.csv`, `main/webui_ota.cpp` (bis auf die
Wiederverwendung von `prepare_ota_heap()`), `main/webui_storage.cpp`,
`sdkconfig.hb-rf-eth-ng` (Rollback ist bereits aktiv).

---

## 10. Erwarteter Ressourcenbedarf

| Größe | Schätzung |
|---|---|
| Flash (Firmware) | +12…15 KB |
| RAM dauerhaft (Leerlauf) | ~300 Byte Statusstruktur + ein `esp_timer` (~100 Byte) — **kein** dauerhafter Task |
| RAM Peak Check | 4 KiB Task-Stack + 1 KiB Puffer + ~3 KB cJSON + TLS-Session |
| RAM Peak Download | 4 KiB Puffer + ~120 Byte SHA-Kontext + TLS-Session, **nach** Freigabe der Monitoring-Worker (≈ 20 KB) |
| Zusätzlicher Netzwerkverkehr | ~1 KB pro Check, bei 24-h-Intervall ≈ 30 KB/Monat |

Die TLS-Session ist derselbe Kostenpunkt, den Syslog-, Webhook- und
MQTT-TLS-Pfade heute bereits bezahlen — durch `g_net_fetch_mutex` kommt sie
nie zusätzlich obendrauf, sondern immer nur abwechselnd.

---

## 11. Offene Punkte für die Freigabe

1. Soll `notify` bei Werkszustand wirklich der Default sein, oder soll auch dort
   `off` gelten und der Betreiber schaltet bewusst ein?
2. Beta-Kanal im Gerät anbieten oder nur `stable` (Beta-Tester laden ohnehin manuell)?
3. Wartungsfenster in Ortszeit setzt eine gültige Zeitzone voraus — Verhalten
   definieren, wenn NTP/DCF77/GPS keine Zeit liefern (Vorschlag: Installation
   aufschieben, Check läuft weiter).
4. Soll `fwMinFrom` rückwirkend in die bereits veröffentlichten Manifeste
   eingetragen werden?
