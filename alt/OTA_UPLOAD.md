# OTA Upload via PlatformIO

Diese Anleitung erklärt, wie du Firmware-Updates über das Netzwerk (OTA - Over The Air) mit PlatformIO durchführst.

## Voraussetzungen

### 1. Python Requests-Modul installieren
```bash
pip install requests
```

### 2. Umgebungsvariable für Passwort setzen (optional)

**Windows (PowerShell):**
```powershell
$env:HB_RF_ETH_PASSWORD="dein_passwort"
```

**Windows (CMD):**
```cmd
set HB_RF_ETH_PASSWORD=dein_passwort
```

**Linux/Mac:**
```bash
export HB_RF_ETH_PASSWORD="dein_passwort"
```

**Alternativ:** Wenn keine Umgebungsvariable gesetzt ist, wirst du beim Upload nach dem Passwort gefragt.

## Verwendung

### Methode 1: OTA-Upload mit vorkonfigurierter IP

Die IP-Adresse `192.168.178.56` ist bereits in `platformio.ini` vorkonfiguriert.

**In Visual Studio Code:**
1. Öffne die PlatformIO-Leiste
2. Wähle ein OTA-Environment aus:
   - `hb-rf-eth-ng-standard-ota`
   - `hb-rf-eth-ng-hmlgw-ota`
   - `hb-rf-eth-ng-analyzer-ota`
   - `hb-rf-eth-ng-full-ota`
3. Klicke auf "Upload" (oder drücke `Ctrl+Alt+U`)

**In der Kommandozeile:**
```bash
pio run -e hb-rf-eth-ng-standard-ota -t upload
```

### Methode 2: OTA-Upload mit individueller IP

Überschreibe die IP-Adresse via Umgebungsvariable:

```bash
# Windows PowerShell
$env:UPLOAD_PORT="192.168.1.100"
pio run -e hb-rf-eth-ng-standard-ota -t upload

# Linux/Mac
UPLOAD_PORT=192.168.1.100 pio run -e hb-rf-eth-ng-standard-ota -t upload
```

### Methode 3: Permanente IP-Adresse konfigurieren

**Empfohlen:** Nutze `platformio_local.ini` für deine lokale IP (wird nicht in Git committed):

```bash
# Kopiere das Template
cp platformio_local.ini.example platformio_local.ini

# Bearbeite platformio_local.ini und trage deine IP ein
```

**Alternativ:** Bearbeite direkt `platformio.ini` und ändere `upload_port`:

```ini
[env:hb-rf-eth-ng-standard-ota]
upload_port = 192.168.178.56  ; <-- Deine IP hier eintragen
```

⚠️ **Hinweis:** Wenn du die IP direkt in `platformio.ini` änderst, wird sie bei `git commit` mit committed. Nutze besser `platformio_local.ini` oder die Umgebungsvariable `UPLOAD_PORT`.

## Ablauf

Der OTA-Upload führt folgende Schritte automatisch aus:

1. **Login**: Authentifizierung mit dem Gerätepasswort
2. **Token-Abruf**: Erhält Authentifizierungs-Token vom Gerät
3. **Firmware-Upload**: Lädt die kompilierte `.bin` Datei hoch
4. **Neustart**: Gerät startet automatisch mit neuer Firmware

## Beispiel-Ausgabe

```
============================================================
HB-RF-ETH-ng OTA Upload
============================================================
Target device: 192.168.178.56
Firmware: .pio/build/hb-rf-eth-ng-standard/firmware.bin
------------------------------------------------------------
Logging in to 192.168.178.56...
✓ Login successful, token received
Uploading firmware: firmware.bin (918528 bytes)
Uploading... (this may take 30-60 seconds)
✓ Firmware uploaded successfully!
Device is restarting with new firmware...

OTA Update completed!
```

## Fehlerbehebung

### "No password provided"
- Setze die Umgebungsvariable `HB_RF_ETH_PASSWORD`
- Oder führe den Upload in einer interaktiven Shell aus (Passwort-Prompt)

### "Authentication failed"
- Überprüfe das Passwort
- Standard-Passwort: `admin` (sollte beim ersten Login geändert werden)

### "Connection timeout"
- Überprüfe die IP-Adresse mit `ping 192.168.178.56`
- Stelle sicher, dass Gerät und PC im gleichen Netzwerk sind
- Firewall-Regeln prüfen (Port 80 muss erreichbar sein)

### "Upload failed"
- Ausreichend Speicherplatz auf dem Gerät prüfen
- Firmware-Datei korrumpiert → Rebuild mit `pio run -e <environment> -t clean`
- Gerät neu starten und erneut versuchen

## Normaler Upload via USB

Für den ersten Upload oder bei Problemen nutze die Standard-Environments (ohne `-ota`):

```bash
# Via USB
pio run -e hb-rf-eth-ng-standard -t upload
```

## Sicherheit

- **Token-Sicherheit**: Das Token wird bei jedem Boot neu generiert
- **Passwort**: Ändere das Standard-Passwort beim ersten Login!
- **Netzwerk**: OTA-Updates funktionieren nur im lokalen Netzwerk
- **Verschlüsselung**: HTTP wird verwendet (keine TLS/HTTPS auf ESP32)
