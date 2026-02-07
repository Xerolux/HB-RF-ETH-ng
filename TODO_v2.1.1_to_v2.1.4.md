# TODO: Features von v2.1.1 → v2.1.4 schrittweise einbauen

## Status: Zurück auf v2.1.1 (funktionierender Stand)

**Datum:** 2026-02-07
**Grund:** RaspberryMatic-Verbindung funktioniert in v2.1.4 nicht mehr
**Plan:** Schrittweise Features einbauen und nach jedem Schritt testen

---

## 🎯 Strategie

1. ✅ **Basis:** v2.1.1 (funktioniert nachweislich)
2. ⏳ **Schrittweise hinzufügen:** Jedes Feature einzeln einbauen
3. ✅ **Testen:** Nach jedem Feature mit RaspberryMatic testen
4. ❌ **Bei Fehler:** Feature zurücknehmen oder fixen

---

## 📋 Features die nach v2.1.1 hinzugefügt wurden

### 🔴 KRITISCH - Nicht einbauen (haben Probleme verursacht):

- [ ] ❌ **Queue-Optimierungen in rawuartudplistener.cpp**
  - Änderung: `xQueueSend(..., portMAX_DELAY)` → `xQueueSend(..., 0)`
  - Problem: Droppt Connection-Pakete
  - **NICHT EINBAUEN!**

- [ ] ❌ **RadioModuleConnector Mutex-Timeout**
  - Änderung: `portMAX_DELAY` → `pdMS_TO_TICKS(5)`
  - Problem: Frames werden gedroppt
  - **NICHT EINBAUEN!**

- [ ] ❌ **MSG_DONTWAIT in HMLGW**
  - Änderung: Blocking send → Non-blocking send
  - Problem: Vorzeitige Disconnects
  - **NICHT EINBAUEN!**

---

### 🟡 OPTIONAL - Mit Vorsicht einbauen:

#### 1. HMLGW (HomeMatic LAN Gateway Emulation)
**Dateien:**
- `include/hmlgw.h`
- `src/hmlgw.cpp`

**Was es macht:**
- Emuliert ein HM-LGW (LAN Gateway)
- Alternative Verbindungsmethode zu RaspberryMatic
- Für Leute die das LAN Gateway Protokoll nutzen wollen

**Priorität:** NIEDRIG (optional Feature)

**Schritte:**
- [ ] HMLGW Header und Source aus neuerer Version holen
- [ ] Build-Flag `ENABLE_HMLGW` in platformio.ini aktivieren
- [ ] Kompilieren und testen
- [ ] Mit RaspberryMatic testen (Raw UART UDP muss weiter funktionieren!)

---

#### 2. Analyzer (Paket-Analyzer für Debugging)
**Dateien:**
- `include/analyzer.h`
- `src/analyzer.cpp`

**Was es macht:**
- WebSocket-basierter Paket-Analyzer
- Zeigt HomeMatic-Frames in Echtzeit
- Debugging-Tool

**Priorität:** NIEDRIG (Debug-Feature)

**Schritte:**
- [ ] Analyzer Header und Source aus neuerer Version holen
- [ ] Build-Flag `ENABLE_ANALYZER` in platformio.ini aktivieren
- [ ] Kompilieren und testen
- [ ] WebUI-Integration testen

---

#### 3. DTLS Encryption (verschlüsselte Verbindung)
**Dateien:**
- `include/dtls_encryption.h`
- `include/dtls_api.h`
- `src/dtls_encryption.cpp`
- `src/dtls_api.cpp`

**Was es macht:**
- DTLS-Verschlüsselung für Raw UART UDP
- Sichere Verbindung zwischen HB-RF-ETH und RaspberryMatic
- Optional einschaltbar

**Priorität:** MITTEL (Security-Feature)

**Schritte:**
- [ ] DTLS Dateien aus neuerer Version holen
- [ ] Dependencies prüfen (mbedtls?)
- [ ] Kompilieren und testen
- [ ] Mit RaspberryMatic testen (unverschlüsselt muss weiter funktionieren!)

---

#### 4. Nextcloud Backup Integration
**Dateien:**
- `include/nextcloud_api.h`
- `include/nextcloud_client.h`
- `src/nextcloud_api.cpp`
- `src/nextcloud_client.cpp`

**Was es macht:**
- Automatische Backups zu Nextcloud
- WebDAV-Client
- Backup-Verwaltung

**Priorität:** NIEDRIG (Nice-to-have)

**Schritte:**
- [ ] Nextcloud Dateien aus neuerer Version holen
- [ ] WebDAV-Dependencies prüfen
- [ ] Monitoring-Integration prüfen
- [ ] Kompilieren und testen

---

#### 5. Log Manager
**Dateien:**
- `include/log_manager.h`
- `src/log_manager.cpp`

**Was es macht:**
- Zentrales Log-Management
- Log-Rotation
- Log-Export

**Priorität:** NIEDRIG

**Schritte:**
- [ ] Log Manager Dateien holen
- [ ] Integration in bestehendes Logging
- [ ] Kompilieren und testen

---

#### 6. Security Headers & Utilities
**Dateien:**
- `include/security_headers.h`
- `include/secure_utils.h`
- `include/semver.h`

**Was es macht:**
- HTTP Security Headers
- Sichere String-Operationen
- Semantic Versioning

**Priorität:** MITTEL (Security)

**Schritte:**
- [ ] Security-Dateien holen
- [ ] In WebUI integrieren
- [ ] Kompilieren und testen

---

### 🟢 KLEINERE VERBESSERUNGEN:

#### 7. Settings-Erweiterungen
**Datei:** `src/settings.cpp`, `include/settings.h`

**Änderungen:**
- DTLS-Einstellungen
- Nextcloud-Einstellungen
- Erweiterte Monitoring-Optionen

**Schritte:**
- [ ] Diff zwischen v2.1.1 und aktuell anschauen
- [ ] Nur nötige Erweiterungen übernehmen
- [ ] Testen

---

#### 8. WebUI-Verbesserungen
**Dateien:** `webui/src/*.vue`, `webui/src/locales/*.js`

**Änderungen:**
- Neue Einstellungs-Seiten
- DTLS-Konfiguration
- Analyzer-Integration
- Nextcloud-Backup-UI

**Schritte:**
- [ ] WebUI Schritt für Schritt aktualisieren
- [ ] Nach jeder Änderung Build testen
- [ ] Browser-Tests

---

#### 9. Monitoring-Erweiterungen
**Dateien:** `src/monitoring.cpp`, `src/monitoring_api.cpp`

**Änderungen:**
- Erweiterte Metriken
- Nextcloud-Integration
- Verbesserte APIs

**Schritte:**
- [ ] Monitoring-Änderungen reviewen
- [ ] Schrittweise übernehmen
- [ ] API-Tests

---

## 🔧 Build-Konfiguration (platformio.ini)

**Wichtige Änderungen:**
- Build-Varianten eingeführt: standard, hmlgw, analyzer, full
- Feature-Flags: `ENABLE_HMLGW`, `ENABLE_ANALYZER`
- Optimierung-Flags

**Schritte:**
- [ ] platformio.ini Änderungen reviewen
- [ ] Build-Varianten testen
- [ ] Standard-Variante MUSS funktionieren!

---

## 📝 Vorgehen morgen:

### Phase 1: Basis stabilisieren (Tag 1)
1. ✅ v2.1.1 als Basis bestätigen
2. ✅ Build testen
3. ✅ Mit RaspberryMatic testen → **MUSS funktionieren!**
4. Baseline dokumentieren

### Phase 2: Build-System (Tag 1-2)
1. platformio.ini aktualisieren (Build-Varianten)
2. Standard-Variante bauen und testen
3. Feature-Flags implementieren

### Phase 3: Core Features (Tag 2-3)
1. Security Headers (einfach, wenig Risiko)
2. Log Manager (isoliert, kein Risiko)
3. Settings-Erweiterungen (vorsichtig!)

### Phase 4: Optional Features (Tag 3-4)
1. DTLS (mit Tests!)
2. HMLGW (separate Variante)
3. Analyzer (separate Variante)

### Phase 5: Nice-to-have (Tag 4-5)
1. Nextcloud
2. WebUI-Verbesserungen
3. Monitoring-Erweiterungen

---

## ⚠️ WICHTIGE REGELN:

1. **NIEMALS gleichzeitig mehrere Features einbauen!**
2. **Nach JEDEM Feature mit RaspberryMatic testen!**
3. **Bei Problemen: Feature sofort zurücknehmen!**
4. **Git-Commit nach jedem funktionierenden Feature!**
5. **Dokumentieren was funktioniert und was nicht!**

---

## 📊 Fortschritt verfolgen:

- [ ] = Noch nicht begonnen
- [~] = In Arbeit
- [✓] = Erfolgreich eingebaut und getestet
- [✗] = Verursacht Probleme, nicht einbauen

---

## 🎯 Erfolgskriterium:

**RaspberryMatic-Verbindung muss nach JEDEM Schritt funktionieren!**

Wenn ein Feature die Verbindung kaputt macht:
1. Feature zurücknehmen
2. In TODO-Liste als [✗] markieren
3. Grund dokumentieren
4. Zum nächsten Feature übergehen

---

Session: https://claude.ai/code/session_019cLVXQJQZYVqs4hvMLXKUv
Erstellt: 2026-02-07
