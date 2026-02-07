# HB-RF-ETH-ng Firmware v2.1.2

Dieses Release enthält die fertige Firmware für die HB-RF-ETH Platine.

## Was ist neu in v2.1.2?

### Erhöhte Sicherheit
- Stärkere Passwort-Anforderungen (8 Zeichen, Groß-/Kleinschreibung, Zahlen)
- OTA-Passwort wird nicht mehr im Browser gespeichert (Security Best Practice)

### Verbesserte Firmware-Update-UX
- Passwort-Stärke-Anzeige beim Setzen
- Warnhinweis wenn OTA-Passwort nicht gesetzt
- Automatischer Neustart nach erfolgreichem Update

### Detaillierte Neustart-Gründe
- Zeigt Ursache des letzten Neustarts (Update, Werksreset, Fehler, etc.)

### Neue Features
- **OTA Update per URL** - Firmware direkt aus dem Netzwerk herunterladen
- **Werksreset** auch über WebUI möglich
- **Moderne, responsive WebUI** mit Mobile-Support

## Installationsanleitung

1. Herunterladen der `firmware_2.1.2.bin` Datei
2. In der WebUI zur Seite "Firmware Update" navigieren
3. Die .bin Datei hochladen
4. OTA-Passwort eingeben (falls konfiguriert)
5. Update wird automatisch eingespielt

**Alternativ via URL-Download:**
1. In der WebUI zur Seite "Firmware Update" navigieren
2. Direkte URL zur .bin Datei eingeben
3. Quick-Button für die neueste GitHub-Version nutzen
4. Firmware wird heruntergeladen und installiert

## Prüfsümme

Die SHA256-Prüfsummen befinden sich in `SHA256SUMS.txt`.

## Unterstützung gefällig?

Dir gefällt dieses Projekt und du möchtest mir einen Kaffee ausgeben?

[![Buy Me A Coffee][buymeacoffee-badge]][buymeacoffee]
[![Tesla][tesla-badge]][tesla]

<!-- Links -->
[buymeacoffee]: https://www.buymeacoffee.com/xerolux
[buymeacoffee-badge]: https://img.shields.io/badge/buy%20me%20a%20coffee-donate-yellow.svg?style=for-the-badge
[tesla]: https://ts.la/sebastian564489
[tesla-badge]: https://img.shields.io/badge/Tesla-Referral-red?style=for-the-badge&logo=tesla
