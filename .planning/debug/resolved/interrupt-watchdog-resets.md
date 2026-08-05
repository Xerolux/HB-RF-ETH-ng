---
status: in_progress
trigger: "die eths stürzen reihenweise ab, leds sind aus, man muss den strom ziehen; vermutung supporter key / tls abruf der keys"
created: 2026-08-05T12:00:00+02:00
updated: 2026-08-05T12:00:00+02:00
---

## Current Focus

hypothesis: "Der Interrupt-Watchdog bei gesundem Heap (99–104 KB frei, low_streak=0 in jeder Blackbox) ist KEINE Heap-Exhaustion und KEIN Supporter-Key/CRL/TLS-Problem (Nutzer ohne Key crashen identisch; der CRL-Task läuft nur mit konfiguriertem Key). Das Muster (IWDT 300 ms, beide CPUs, zufällige Uptimes 2 min–32 h) ist ein Interrupt-/Cache-Kontrakt-Problem im Always-on-Pfad. Der .planning-Fix (EMAC WORK_WITH_CACHE_DISABLE entfernt, ETH_IRAM_OPTIMIZATION aus) hat die Feld-Crashes NICHT beseitigt — Beta.4/5 melden sogar Totalausfälle."
test: "Drei Rest-Kandidaten im Always-on-Pfad beheben: (A) IRAM_ATTR-Wrapper, der Nicht-IRAM-Code aufruft (dieselbe Bug-Klasse wie der EMAC-Fix), (B) DHCP-Restart im Ethernet-Event-Handler (blockierende lwIP-Aufrufe im Event-Loop bei jedem Link-Up), (C) ETH-DMA-Puffer-Reduktion 20/20 → 12/8 in Beta.5 rückgängig (RX-Descriptor-Exhaustion unter CCU-Traffic → EMAC-ISR-Wait)."
expecting: "Firmware baut, läuft über Tage stabil; keine IWDT-Resets mehr."
next_action: "Feldtest der Beta mit den drei Fixes; wenn Resets bleiben: Seriell-Backtrace des IWDT-Panics (PC-Adresse) ist der einzige definitive Weg — WebUI-Log stirbt mit dem Gerät."

## Symptoms

expected: "HB-RF-ETH-ng läuft jahrelang stabil unter CCU-Raw-UART-Traffic."
actual: "Geräte stürzen reihenweise ab (2 min bis 32 h Uptime), LEDs aus, nur Power-Cycle hilft; Boot zeigt 'Watchdog Reset (Interrupt Watchdog)'."
errors: "free=103064 larg=94208 min=92716 int=153952 up=126s; free=91028 larg=63488 min=37280 int=141916 up=115026s; immer low_streak=0."
reproduction: "Kein deterministischer Trigger. Betrifft Geräte MIT und OHNE MQTT/Syslog/Events; auch mit allen Features deaktiviert (zoephelweb). 2.1.10 stabil."
started: "Seit der 2.2.x-Linie; durch die Beta-Serie nicht behoben."

## Evidence

- timestamp: 2026-08-05T12:00:00+02:00
  checked: "Issue #362 (29 Kommentare), alle Crash-Blackbox-Werte, heruntergeladene Syslog-/WebUI-Logs."
  found: "Heap ist bei JEDEM Crash gesund (99–104 KB frei, 94–98 KB größter Block, min_ever 90–97 KB, internal 150 KB). Der Heap über 32 h stabil — kein Leak, kein Fragmentierungs-Wachstum. Uptimes zufällig, kein 6-h-CRL-Muster."
  implication: "Heap-Exhaustion UND Supporter-Key/CRL/TLS sind ausgeschlossen. Zoephelweb crashte mit ALLEN Features deaktiviert (kein Key, kein CRL-Task, kein TLS)."

- timestamp: 2026-08-05T12:00:00+02:00
  checked: "Git-Tags v2.2.6-Beta.4/Beta.5 vs. .planning-Fix."
  found: "v2.2.6-Beta.4 enthält 7763e8d (EMAC-Fix). Feld: Beta.4/5 = Totalausfälle (Walki2000). Der EMAC-Fix hat das Problem NICHT gelöst — die Suche muss weitergehen."
  implication: "Die .planning-Root-Cause war unvollständig; es gibt eine zweite Ursache im Always-on-Pfad."

- timestamp: 2026-08-05T12:00:00+02:00
  checked: "rawuartudplistener.cpp, ethernet.cpp, sdkconfig.hb-rf-eth-ng."
  found: "(A) _raw_uart_udpReceivePaket ist IRAM_ATTR, ruft aber _udpReceivePacket (Nicht-IRAM, inkl. ESP_LOGW) — Cache-off-Zugriff möglich. (B) ETHERNET_EVENT_CONNECTED macht esp_netif_dhcpc_stop/start (blockierende lwIP-Calls im Event-Loop, Prio-20-Task) bei jedem Link-Up. (C) Beta.5 reduzierte ETH-DMA-Puffer auf 12/8 RX, 8 TX."
  implication: "Alle drei sind IWDT-relevante Rest-Kandidaten; A und C sind Beta.5-Änderungen, B kam in fae05f0."

- timestamp: 2026-08-05T12:00:00+02:00
  checked: "ESP-IDF v6.1-beta1 Release-Notes."
  found: "Keine EMAC/lwIP-Fixes für dieses Problem; Breaking Change: esp-mqtt wandert in den component manager (bricht mqtt_handler.cpp)."
  implication: "IDF-Wechsel auf 6.1-beta1 ist KEIN Lösungsweg."

## Eliminated

- hypothesis: "Heap-Exhaustion / Fragmentierung."
  evidence: "free=99–104 KB, largest=94–98 KB, low_streak=0 bei jedem dokumentierten Crash."
  timestamp: 2026-08-05T12:00:00+02:00

- hypothesis: "Supporter-Key / CRL-TLS-Abruf."
  evidence: "CRL-Task startet nur mit Key (main.cpp:152-158); Nutzer ohne Key crashen identisch; Crash-Uptimes nicht am 6-h-Zyklus."
  timestamp: 2026-08-05T12:00:00+02:00

- hypothesis: "ESP-IDF v6.1-beta1 löst das Problem."
  evidence: "Keine relevanten Fixes in den Release-Notes; Breaking Change esp-mqtt."
  timestamp: 2026-08-05T12:00:00+02:00

## Resolution

root_cause: "Offen — Felddaten schließen Heap und TLS aus; das IWDT-Muster weist auf Interrupt-/Cache-Kontrakt im Always-on-Pfad. Drei konkrete Rest-Kandidaten wurden behoben (IRAM_ATTR-Wrapper, DHCP-Churn im Event-Handler, DMA-Puffer 12/8 → 20/20)."
fix: "main/rawuartudplistener.cpp: IRAM_ATTR vom UDP-Recv-Wrapper entfernt. main/ethernet.cpp: DHCP-Restart aus ETHERNET_EVENT_CONNECTED entfernt (esp_netif glue erneuert DHCP selbst). sdkconfig.hb-rf-eth-ng: CONFIG_ETH_DMA_RX/TX_BUFFER_NUM 20/20 wiederhergestellt."
verification: "idf.py build (ESP-IDF v6.0.2, esp32) erfolgreich, 30% Partition frei. Feldtest ausstehend."
files_changed: "main/rawuartudplistener.cpp, main/ethernet.cpp, sdkconfig.hb-rf-eth-ng"
