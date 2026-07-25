---
status: resolved
trigger: "Mit der neuen Firmware ist MQTT kaputt: Einstellungen vergessen, Konfiguration scheitert mit \"Invalid check mkport\", Rollback auf Beta 13 hilft nicht, und MQTT-Daten fehlen im Backup."
created: 2026-07-25T11:10:00+02:00
updated: 2026-07-25T12:35:00+02:00
---

## Current Focus

hypothesis: "Bestätigt: Settings::clear() leert den Monitoring-Namespace über ein READWRITE-Handle, wobei der Namespace bestehen bleibt. load_config_from_nvs() setzte Defaults nur, wenn nvs_open() scheiterte, und lud aus einem vorhandenen leeren Namespace deshalb eine nullinitialisierte CheckMK-Konfiguration mit Port 0."
test: "Native Default-/Normalisierungs-Tests, vollständiger ESP32-Build und WebUI-Regression mit emuliertem CheckMK-Port 0."
expecting: "Ein leerer Namespace ergibt CheckMK-Port 6556 und MQTT-Port 1883; vorhandene gültige MQTT-Schlüssel bleiben unverändert; die WebUI sendet nie Port 0 zurück."
next_action: "Beta 15 veröffentlichen und Clem die Neuinstallation sowie die Grenzen der Datenwiederherstellung mitteilen."

## Symptoms

expected: "Ein Firmware-Update erhält die MQTT-Konfiguration. MQTT lässt sich anschließend konfigurieren. Ein Backup enthält alle MQTT-Einstellungen."
actual: "Nach Installation der neuen Firmware sind die MQTT-Einstellungen vergessen und MQTT lässt sich nicht mehr konfigurieren."
errors: "\"Invalid check mkport\" beziehungsweise vermutlich \"Invalid CheckMK port\"."
reproduction: "Neue Firmware installieren, Monitoring/MQTT öffnen und speichern; anschließend Backup und Rollback auf Beta.13 prüfen."
started: "Mit der neuen Beta.14; laut Bericht bleibt der Fehler nach Rollback auf Beta.13 bestehen."

## Eliminated

## Evidence

- timestamp: 2026-07-25T11:34:00+02:00
  checked: "Diff v2.2.5-Beta.13..v2.2.5-Beta.14 für monitoring.cpp, monitoring_api.cpp und monitoring.vue."
  found: "Monitoring-API und -WebUI sind unverändert. Beta.14 ergänzt nur Backup/Restore und eine strengere NVS-Fehlerrückgabe."
  implication: "Der Fehler entsteht nicht durch eine neue MQTT- oder CheckMK-Formularlogik."

- timestamp: 2026-07-25T11:38:00+02:00
  checked: "Settings::clear(), erase_nvs_namespace() und load_config_from_nvs()."
  found: "Der Werksreset öffnet 'monitoring' mit NVS_READWRITE und ruft nvs_erase_all auf. Der Loader setzt Defaults ausschließlich dann, wenn das anschließende NVS_READONLY-Öffnen fehlschlägt. Bei einem vorhandenen leeren Namespace bleibt current_config.checkmk.port aus der statischen Nullinitialisierung auf 0; MQTT-Port besitzt zufällig einen eigenen Missing-Key-Fallback auf 1883."
  implication: "GET /api/monitoring liefert CheckMK-Port 0. Die WebUI sendet beim Speichern aller Monitoring-Bereiche diesen Wert zurück, und POST /api/monitoring bricht vor MQTT mit 'Invalid CheckMK port' ab."

- timestamp: 2026-07-25T11:40:00+02:00
  checked: "Firmware-Downgrade-Verhalten und Legacy-Backup-Kompatibilität."
  found: "OTA und Firmware-Downgrade verändern die NVS-Partition nicht. Beta.13 enthält denselben Default-on-open-failure-Loader. Legacy-Backups ohne 'monitoring' lassen den Namespace absichtlich unverändert; Beta.14-Backups serialisieren MQTT einschließlich Passwort und TLS-Material."
  implication: "Der defekte leere Namespace überlebt den Downgrade. Verlorene Werte nach einem absichtlichen Werksreset sind erwartungsgemäß gelöscht; die Unmöglichkeit, MQTT neu zu konfigurieren, ist der eigentliche Defekt."

## Resolution

root_cause: >-
  nvs_erase_all() ließ einen vorhandenen, aber leeren Namespace zurück. Der Monitoring-Loader
  initialisierte Defaults nur bei fehlgeschlagenem nvs_open(). Dadurch blieb checkmk.port auf 0.
  Da die WebUI alle Monitoring-Bereiche gemeinsam speichert, wurde jede MQTT-Änderung vor ihrer
  Verarbeitung mit "Invalid CheckMK port" abgewiesen. Ein Downgrade änderte den NVS-Zustand nicht.
fix: >-
  Der Firmware-Loader setzt nun immer eine vollständige Default-Konfiguration und legt anschließend
  vorhandene NVS-Werte darüber. Eine Normalisierung repariert ungültige Ports und Enumerationen, ohne
  MQTT-Zugangsdaten oder andere Secrets anzutasten. Die WebUI führt dieselbe Port-Reparatur als
  Kompatibilitätsschutz für ältere Firmware aus. Backup-Regressionen prüfen den vollständigen
  MQTT-Block einschließlich Passwort, TLS-Zertifikaten, Private Key und Command-Token.
verification:
  "- Nativer Host-Test für leere, partielle und ungültige Monitoring-Konfiguration: bestanden.
  - WebUI-Build: bestanden.
  - Playwright: 35/35 Regressionstests bestanden.
  - Vollständiger ESP-IDF-6.1-Build: bestanden; Firmware-Image 0x144210 Bytes, 30 % frei.
  - git diff --check: bestanden."
files_changed:
  "- include/monitoring.h
  - main/monitoring.cpp
  - main/monitoring_config.cpp
  - test/host/test_monitoring_defaults.cpp und Stubs
  - webui/src/stores.js
  - webui/tests/regressions.spec.js
  - webui/package.json und package-lock.json
  - webui/spiffs_image/*
  - .github/workflows/build.yml
  - .github/workflows/release.yml"
