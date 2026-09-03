---
status: awaiting_human_verify
trigger: "Issue #362: Interrupt-Watchdog-Resets bei mehreren Feldgeräten seit 2.2.x, v2.1.10 stabil, kein Log, gesunder Heap; serieller Backtrace in der Feldumgebung nicht verfügbar"
created: 2026-09-03T20:55:00+02:00
updated: 2026-09-03T20:55:00+02:00
---

## Current Focus

hypothesis: "ROOT CAUSE (hochkonfident): Espressif Errata WDT-3.15 — ESP32-Silicon v3.0/v3.1 kann bei gleichzeitigen Cache-Misses auf denselben Cache-Set (3 von 4 IBUS/DBUS-External-Memory-Zugriffen) komplett livelocken: beide CPUs stoppen mid-Memory-Access, der Interrupt-Watchdog resettet ohne Panic-Output. Der IDF-Workaround CONFIG_ESP32_ECO3_CACHE_LOCK_FIX existiert, ist aber per Kconfig an SPIRAM gebunden und damit auf dem PSRAM-losen HB-RF-ETH deaktiviert. Alle crashenden Feldgeräte dürften Revision 300/301 haben; das stabile Referenzgerät 192.168.178.56 läuft nachweislich auf Revision 100 (aus /api/system/overview ausgelesen)."
test: "Falsifizierbare Vorhersage: Walki2000, zoephelweb, ChristophA, M-Schoeler lesen im WebUI (Systemübersicht bzw. /api/system/overview → chipRevision) ihre Chip-Revision ab. Erwartung: >= 300 bei jedem Crash-Gerät. Zweiter Test: Firmware mit aktiviertem ECO3-Workaround (+ Tick-Sentinel) auf einem Rev-3-Gerät — erwartet stabil über Tage; falls doch ein Reset passiert, liefert der Tick-Sentinel die Kern-Differenzierung (beide Kerne gleichzeitig eingefroren = Livelock-Signatur)."
expecting: "Crash-Geräte = chipRevision 300/301; stabile Geräte = Revision < 300. Mit Workaround keine IWDT-Resets mehr auf Rev-3-Hardware."
next_action: "1) Im Issue #362 nach chipRevision fragen (Systemübersicht der Crash-Geräte). 2) Beta mit ECO3-Workaround bauen und auf ein Rev-3-Feldgerät flashen. 3) Tick-Sentinel-Ausgabe nach jedem evtl. Reset auswerten (Boot-Log: 'Tick sentinel: cpu0 ... cpu1 ...')."

## Symptoms

expected: "HB-RF-ETH läuft dauerhaft stabil unter CCU-Raw-UART-Traffic."
actual: "Sofortige Interrupt-Watchdog-Resets ohne jede Vorwarnung: kein Log (Syslog an!), kein Panic-Output, gesunder Heap (82-104 KB frei, low_streak=0), kein stuck NVS/net-Op. Uptimes 66 s bis 32 h, Häufung in den ersten ~20 min nach Boot (CCU-Initialburst = hohe Bus-Last)."
errors: "Boot reset reason: Watchdog Reset (Interrupt Watchdog) — sonst nichts."
reproduction: "Nur auf betroffener Hardware (vermutlich Rev 3); deterministisch nicht reproduzierbar — Livelock ist ein Cache-Timing-Losspiel (3 Busse, gleicher Set, gleichzeitig)."
started: "Mit der 2.2.x-Serie im Feld; korreliert mit Hardware-Käufen ab ~2021 (ESP32-D0WD-V3/„V3\"-Silicon ist seitdem Standard in WROOM-32E-Modulen)."

## Evidence

- timestamp: 2026-09-03T20:10:00+02:00
  checked: "Live-Gerät 192.168.178.56 (2.2.7-Beta.2, 12 Tage Uptime, gleiche Firmware die im Feld crasht): GET /api/system/overview."
  found: "chipRevision = 100 (Silicon v1.0). Errata WDT-3.15 betrifft nur v3.0/v3.1."
  implication: "Das Referenzgerät ist silicon-seitig immun — erklärt, warum Xerolux den Fehler nie sieht."

- timestamp: 2026-09-03T20:40:00+02:00
  checked: "Offizielle Errata: docs.espressif.com/projects/esp-chip-errata → [WDT-3.15] 'Chip May Have A Live Lock Under Certain Conditions That Will Cause Interrupt Watchdog Issue' (ESP32, rev v3.0)."
  found: "Livelock-Bedingung braucht KEIN PSRAM (IBUS/DBUS auf External Memory = Flash-Cache reicht). IDF-Workward CONFIG_ESP32_ECO3_CACHE_LOCK_FIX: Kconfig-Gate 'depends on !ESP_SYSTEM_SINGLE_CORE_MODE && SPIRAM' (identisch in v5.1, v6.0.2, v6.1-beta1) — auf unserem Board deaktiviert. Errata-Status: 'no fix scheduled'."
  implication: "v2.1.10 (IDF 5.1) war ebenfalls ungeschützt; deren Feldstabilität stammt von älterer Hardware bzw. anderem Cache-Layout des kleineren Binaries."

- timestamp: 2026-09-03T20:45:00+02:00
  checked: "int_wdt.c + highint_hdl.S (IDF 6.1-beta1): Workaround-Mechanik."
  found: "Workaround = verkürzte IWDT-stage-0-Intervalle + livelock-aware High-Level-Interrupt-Handler (_lx_intr_livelock_counter/max). Auf Rev <= v2 No-op (soc_has_cache_lock_bug() == false). Selbst enthalten (int_wdt + sleep_modes), keine weiteren Integrationspunkte."
  implication: "Aktivierung auf Nicht-PSRAM-Builds ist risikoarm; auf altem Silicon passiert nichts."

- timestamp: 2026-09-03T19:30:00+02:00
  checked: "Issue-Anhänge (syslog + WebUI-Logs aller Reporter), Crash-Blackbox-Werte."
  found: "Syslog lief mit und zeigte vor dem Crash STILLE (keine Fehlerzeile). Heap bei jedem Crash gesund. zoephelweb tauschte das Board (REV 1.10, 2026 gekauft) — identischer Fehler."
  implication: "Software-seitige Vorboten existieren nicht; Board-Tausch bringt neues V3-Silicon — beides passt zum Livelock."

## Eliminated

- hypothesis: "Heap-Erschöpfung / Fragmentierung."
  evidence: "Blackbox: free 82-104 KB, low_streak=0 bei jedem Crash."
  timestamp: 2026-08-05 (vorherige Analyse), bestätigt 2026-09-03

- hypothesis: "App-Code: Critical Sections, eigene ISRs (DCF), Flash-Schreibkadenzen, UART-Konkurrenz, EMAC-IRAM an/aus, I2C/RTC, esp_timer-Dispatch, ECO3-mit-PSRAM-Variante."
  evidence: "Systematische Quellprüfung + IDF-Quellcode-Verifikation: alle Pfade entlastet (Details im Session-Log); ETH_IRAM in BOTH-Zuständen gecrasht; keine periodischen Flash-Writes; UART1 hat genau einen Live-Schreiber."
  timestamp: 2026-09-03

- hypothesis: "IDF-6-Regression in UART-/EMAC-/Flash-Guard (v5.1→v6.1)."
  evidence: "Diff v5.1↔v6.1 der Verdachts-Treiber: mechanisch äquivalent (Flash-Guard stalled die Gegencpu weiter im Task-Kontext, Tick bleibt IRAM-sichtbar)."
  timestamp: 2026-09-03

## Resolution (pending verification)

root_cause: "ESP32 ECO3-Silicon-Livelock (Errata WDT-3.15) auf Feldgeräten mit Chip-Revision v3.x; IDF-Workaround durch SPIRAM-Kconfig-Gate auf dieser Platinenvariante deaktiviert."
fix: "scripts/patch_idf_eco3_fix.sh entfernt die SPIRAM-Gate aus der frisch geklonten IDF-Kconfig in allen 5 Build-Workflows → CONFIG_ESP32_ECO3_CACHE_LOCK_FIX=y (default y, promptless). Zusätzlich: Tick-Sentinel in der Crash-Blackbox (core-lokale letzte-Tick-Zeitstempel, IRAM-Hook) als Verifikations-Instrumentierung und UART1-TX-Ringbuffer (2 KiB) als Relay-Härtung."
verification: "Build läuft (WSL, CI-äquivalent); sdkconfig muss CONFIG_ESP32_ECO3_CACHE_LOCK_FIX=y zeigen. Feldtest auf Rev-3-Gerät ausstehend; chipRevision-Abfrage im Issue ausstehend."
files_changed: "scripts/patch_idf_eco3_fix.sh (neu), .github/workflows/{build,pr-check,release,release-webui,security}.yml, include/crash_blackbox.h, main/crash_blackbox.cpp, main/reset_info.cpp, main/main.cpp, main/radiomoduleconnector.cpp, docs (CLAUDE.md/README), sowie IDF-Pinning v6.1-rc1→v6.1 (separater Change)."
