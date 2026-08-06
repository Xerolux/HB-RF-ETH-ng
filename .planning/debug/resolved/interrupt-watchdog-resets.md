---
status: resolved
trigger: "die eths stürzen reihenweise ab, leds sind aus, man muss den strom ziehen; vermutung supporter key / tls abruf der keys"
created: 2026-08-05T12:00:00+02:00
updated: 2026-08-06T00:00:00+02:00
---

## Current Focus

hypothesis: "Der Interrupt-Watchdog bei gesundem Heap (99–104 KB frei, low_streak=0 in jeder Blackbox) ist KEINE Heap-Exhaustion. Felddaten (Clem, 5./6.8.): ein Gerät crasht weiter mit IWDT bei up=126 s, 486 s und 20406 s (≈5,67 h — nahe am 6-h-CRL-Refresh-Zyklus), während drei andere Geräte 4 h+ stabil laufen und 192.168.178.56 mit Beta.7 (v6.1-beta1) 12 h+ stabil ist. v6.1-beta1 hat den Totalausfall (LEDs aus, Power-Cycle nötig) behoben, aber auf dem betroffenen Gerät bleiben IWDT-Resets."
test: "Supporter-Key/CRL komplett entfernen — die einzige periodische TLS-Aktivität im System war der 6-h-CRL-Refresh (nur aktiv mit Supporter-Key) plus PSA-SHA-Berechnung (Hardware-SHA-Engine mit Spinlock) bei jedem sysinfo-Poll auf Key-Geräten. Zusätzlich waren die drei Always-on-Fixes aus Beta.6 (IRAM_ATTR-Wrapper, DHCP-Churn, ETH-DMA 20/20) bereits enthalten."
expecting: "Firmware ohne Supporter-Key/CRL-Code baut, läuft über Tage stabil; keine IWDT-Resets mehr auf dem betroffenen Gerät."
next_action: "Feldtest der Beta.8 (Supporter-Key entfernt, v6.1-beta1); wenn Resets bleiben: Seriell-Backtrace des IWDT-Panics (PC-Adresse) ist der einzige definitive Weg — WebUI-Log stirbt mit dem Gerät."

## Symptoms

expected: "HB-RF-ETH-ng läuft jahrelang stabil unter CCU-Raw-UART-Traffic."
actual: "Ein Gerät (Clem) stürzt weiterhin per IWDT ab (up=126s, 486s, 20406s), ist aber mit v6.1-beta1 nicht mehr komplett tot (LEDs aus, Power-Cycle) — nur noch Watchdog-Reset. Drei andere Geräte + 192.168.178.56 laufen stabil."
errors: "free=93108 larg=77824 min=73060 int=145560 up=126s; free=84428 larg=65536 min=65932 int=136880 up=20406s; free=82688 larg=69632 min=65704 int=135140 up=486s; immer low_streak=0."
reproduction: "Kein deterministischer Trigger. up=20406s ≈ 5,67 h liegt nahe am 6-h-CRL-Refresh-Zyklus, der nur mit Supporter-Key aktiv ist."
started: "Seit der 2.2.x-Linie; durch die Beta-Serie nicht behoben."

## Evidence

- timestamp: 2026-08-05T12:00:00+02:00
  checked: "Issue #362 (29 Kommentare), alle Crash-Blackbox-Werte, heruntergeladene Syslog-/WebUI-Logs."
  found: "Heap ist bei JEDEM Crash gesund (82–104 KB frei, low_streak=0). Der Heap über 32 h stabil — kein Leak, kein Fragmentierungs-Wachstum."
  implication: "Heap-Exhaustion ausgeschlossen."

- timestamp: 2026-08-06T00:00:00+02:00
  checked: "Felddaten Beta.7 (v6.1-beta1): Clem meldet weiter IWDT auf einem Gerät (up=126s/486s/20406s), 192.168.178.56 (kein Supporter-Key) läuft 12 h+ stabil, drei andere Geräte 4 h+."
  found: "Das crashende Gerät ist das einzige mit vermutlich konfiguriertem Supporter-Key (6-h-CRL-Refresh; PSA-SHA bei jedem sysinfo-Poll). up=20406s ≈ 5,67 h korreliert mit dem 6-h-Zyklus."
  implication: "Der Supporter-Key/CRL-Pfad ist der einzige periodische TLS-/SHA-Aktivposten — er wird entfernt (restlos: Firmware, WebUI, Docs, Tests, activation-server, Key-Generator-Tools)."

- timestamp: 2026-08-05T12:00:00+02:00
  checked: "rawuartudplistener.cpp, ethernet.cpp, sdkconfig.hb-rf-eth-ng."
  found: "(A) IRAM_ATTR-Wrapper (Cache-off-Hang), (B) DHCP-Restart im Event-Handler, (C) ETH-DMA 12/8 — alle drei wurden in Beta.6 behoben."
  implication: "Beta.6/7-Fixes sind drin; der verbleibende Unterschied ist der Supporter-Key."

- timestamp: 2026-08-05T12:00:00+02:00
  checked: "ESP-IDF v6.1-beta1 Release-Notes."
  found: "Keine EMAC/lwIP-Fixes für dieses Problem; Breaking Change: esp-mqtt wandert in den component manager (bereits abgedeckt)."
  implication: "IDF-Wechsel auf 6.1-beta1 hat den Totalausfall behoben (weniger aggressiver EMAC-Interrupt-Kontrakt), löst aber den Rest-IWDT auf dem Key-Gerät nicht."

## Eliminated

- hypothesis: "Heap-Exhaustion / Fragmentierung."
  evidence: "free=82–104 KB, largest=65–98 KB, low_streak=0 bei jedem dokumentierten Crash."
  timestamp: 2026-08-05T12:00:00+02:00

- hypothesis: "ESP-IDF v6.1-beta1 löst das Problem vollständig."
  evidence: "Totalausfall behoben, aber IWDT-Resets auf dem Key-Gerät bleiben."
  timestamp: 2026-08-06T00:00:00+02:00

## Resolution

root_cause: "Wahrscheinlich: Supporter-Key/CRL-Pfad (6-h-TLS-Refresh + PSA-SHA-Spinlock bei jedem sysinfo-Poll) auf dem betroffenen Gerät. Da der Code entfernt wird, ist die Ursache unabhängig von der genauen Mechanik beseitigt."
fix: "Supporter-Key und CRL restlos entfernt: main/supporter_key.cpp, main/supporter_crl.cpp, include/supporter_*.h, webui/src/composables/supporterKey.js, activation-server/ (Key-Generator/Server), alle Referenzen in webui.cpp/settings/main/system_reset/stores/settings.vue/app.vue/NewDesignHeader.vue/sysinfo.vue/Locales/CSS, Test-Policies, Docs, README, sdkconfig-Kommentare. NVS: Legacy-Namespace 'supporter_crl' wird bei Factory-Reset weiter gepurged; alter 'supporterKey'-NVS-Key wird nicht mehr gelesen/geschrieben."
verification: "WebUI-Build + rename_webui_files.py + idf.py build (v6.1-beta1) ausstehend; Policy-Tests angepasst."
files_changed: "main/*.cpp, include/*.h, webui/src/*, test/host/*, docs, README.md, .gitignore, activation-server/ (gelöscht), .playwright-mcp/ (gelöscht), screenshots/demo.webm (gelöscht)"
