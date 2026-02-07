# Änderungen und Fixes - Session 2026-02-07

## Zusammenfassung
Versuch, RaspberryMatic-Verbindungsprobleme zu beheben, die nach "Stability"-Optimierungen auftraten.

## Problem
RaspberryMatic erkannte kein RF-Modul (zeigte "n/a" für alle Werte), obwohl:
- HB-RF-ETH das Funkmodul korrekt erkannte
- Raw UART UDP Modus gestartet wurde
- Netzwerkverbindung funktionierte

## Identifizierte Bugs in neueren Versionen

### 1. Queue Packet Dropping (KRITISCH)
**Datei:** `src/rawuartudplistener.cpp`
**Problem:**
- Alte Version: `xQueueSend(..., portMAX_DELAY)` - blockiert bis Platz frei
- Kaputte Version: `xQueueSend(..., 0)` - droppt Pakete sofort
- **Ergebnis:** Connection-Pakete wurden gedroppt, Verbindung konnte nicht aufgebaut werden

**Fix:** Zurück zu `portMAX_DELAY`

### 2. MSG_DONTWAIT in HMLGW (nur HMLGW-Variante betroffen)
**Datei:** `src/hmlgw.cpp`
**Problem:**
- `send(..., MSG_DONTWAIT)` verursachte vorzeitige Disconnects bei vollem TCP-Buffer
- EAGAIN/EWOULDBLOCK ist NORMAL auf ESP32, sollte nicht zum Disconnect führen

**Fix:** Zurück zu blocking send `send(..., 0)`

### 3. RadioModuleConnector Mutex Timeout
**Datei:** `src/radiomoduleconnector.cpp`
**Problem:**
- Timeout von `portMAX_DELAY` auf `pdMS_TO_TICKS(5)` reduziert
- Frames wurden gedroppt wenn Mutex nicht in 5ms verfügbar
- HomeMatic-Geräte erschienen nicht in RaspberryMatic

**Fix:** Zurück zu `portMAX_DELAY`

### 4. Raw UART UDP Queue Timeout
**Datei:** `src/rawuartudplistener.cpp`
**Problem:**
- Timeout von 10ms auf 100ms erhöht + extra `vTaskDelay(1)`
- Unnötige Latenz

**Fix:** Zurück zu 10ms ohne extra delay

### 5. Endpoint ID Handling
**Problem:**
- RaspberryMatic speichert endpoint ID persistent
- Nach HB-RF-ETH Reboot: Mismatch zwischen RaspberryMatic (ID=3) und HB-RF-ETH (ID=1)
- HB-RF-ETH sendete Response mit ID=1, aber RaspberryMatic ignorierte sie

**Versuchte Fixes:**
- Client-ID akzeptieren und synchronisieren
- `_connectionStarted` auf true setzen bei Reconnect
- **Alle schlugen fehl**

## Versuchte Lösungsansätze (fehlgeschlagen)

1. **Inkrementelle Fixes:** Einzelne Bugs fixen (funktionierte nicht)
2. **Client ID Sync:** Endpoint ID vom Client akzeptieren (funktionierte nicht)
3. **Connection Started Flag:** Bei Reconnect auf true setzen (funktionierte nicht)

## CCU3-Log Analyse
**Wichtige Erkenntnis:**
```
"no GPIO/USB connected RF-hardware found"
HmRF/HmIP not available
```

ABER: HB-RF-ETH Log zeigte, dass Pakete empfangen wurden!

**Vermutung:** Frames wurden nicht korrekt weitergeleitet oder Session nicht vollständig etabliert.

## Endgültige Lösung
**Komplette Wiederherstellung des funktionierenden Codes aus v2.1.2 (commit f50fb52)**

Grund: Inkrementelle Fixes funktionierten nicht, da das Problem möglicherweise tiefer liegt oder an mehreren Stellen gleichzeitig auftritt.

## Lessons Learned

1. **"Optimierungen" können Funktionalität zerstören:**
   - MSG_DONTWAIT ist NICHT besser für ESP32
   - Aggressive Timeouts droppen kritische Pakete
   - Blocking ist manchmal die richtige Lösung

2. **Bei komplexen Protokollen:**
   - Lieber funktionierenden Code verwenden als optimieren
   - Edge Cases (wie persistent sessions) sind schwer zu debuggen

3. **Debugging-Ansatz:**
   - Logs von BEIDEN Seiten (HB-RF-ETH + RaspberryMatic) nötig
   - CCU3-Log war entscheidend um zu sehen, dass RF-Hardware nicht erkannt wurde

## Dateien mit Änderungen (vor Restore)

- `src/rawuartudplistener.cpp` - Hauptdatei mit Verbindungslogik
- `src/hmlgw.cpp` - LAN Gateway Emulation (nur in HMLGW-Variante)
- `src/radiomoduleconnector.cpp` - Funkmodul-Kommunikation

## Empfehlung

**Verwende v2.1.1 oder v2.1.2 Code als Basis!**

Diese Versionen sind getestet und funktionieren nachweislich mit RaspberryMatic.

---

Session: https://claude.ai/code/session_019cLVXQJQZYVqs4hvMLXKUv
Datum: 2026-02-07
