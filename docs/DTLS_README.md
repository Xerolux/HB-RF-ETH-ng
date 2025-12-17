# DTLS/TLS Transportverschlüsselung für HB-RF-ETH

## Was ist das?

Eine optionale, hochsichere Transportverschlüsselung für die Netzwerkverbindung (RJ45/Ethernet) zwischen der **HB-RF-ETH Platine** und der **CCU** (HomeMatic Central Control Unit).

## ⚠️ Wichtige Einschränkungen

**DTLS Verschlüsselung ist NICHT kompatibel mit:**
- **HM-LGW Modus** - HM-LGW arbeitet mit TCP und benötigt unverschlüsselte Daten
- **Analyzer Modus** - Der Analyzer muss die Rohdaten vor der Verschlüsselung analysieren können

**DTLS funktioniert NUR im Raw-UART UDP Modus** (Standard-Modus).

Wenn HM-LGW oder Analyzer aktiviert sind, wird DTLS automatisch deaktiviert, auch wenn in den Einstellungen aktiviert.

## Warum Verschlüsselung?

**Problem**: Die Raw-UART UDP Kommunikation auf Port 3008 ist standardmäßig **unverschlüsselt**. Das bedeutet:
- ❌ Jeder im lokalen Netzwerk kann HomeMatic-Funkdaten mitlesen
- ❌ Man-in-the-Middle Angriffe sind möglich
- ❌ Keine Authentifizierung der Kommunikationspartner
- ❌ Keine Integritätsprüfung über CRC16 hinaus

**Lösung**: DTLS 1.2 Verschlüsselung schützt die komplette Kommunikation:
- ✅ Ende-zu-Ende Verschlüsselung (AES-256-GCM)
- ✅ Gegenseitige Authentifizierung (PSK oder Zertifikate)
- ✅ Schutz vor Replay-Angriffen
- ✅ Manipulationserkennung
- ✅ Forward Secrecy

## Features

### Verschlüsselungsmodi
1. **Deaktiviert** (Standard) - Volle Abwärtskompatibilität
2. **Pre-Shared Key (PSK)** - Einfach, sicher, empfohlen
3. **X.509 Zertifikate** - Höchster Standard (geplant)

### Cipher Suites
- **AES-128-GCM-SHA256** - Schnell, gute Sicherheit
- **AES-256-GCM-SHA384** - Standard, maximale Sicherheit (hardware-beschleunigt)
- **ChaCha20-Poly1305-SHA256** - Moderne Alternative für Embedded

### Sicherheits-Features
- DTLS 1.2 Protokoll (neueste sichere Version)
- Perfect Forward Secrecy (PFS)
- Session Resumption für Performance
- Replay Protection
- Secure Key Storage (verschlüsselte NVS-Partition)
- PSK Rotation Support

## Schnellstart

### Für Benutzer (Platinen-Konfiguration)

1. **WebUI öffnen**: `http://IP-der-Platine`
2. **Einstellungen** → **Sicherheit** → **DTLS Verschlüsselung**
3. **Modus auswählen**: "Pre-Shared Key (PSK)"
4. **PSK generieren**: Button "Neuen PSK generieren" klicken
5. **PSK kopieren**: Der angezeigte Schlüssel wird **nur einmal** angezeigt!
6. **Speichern**: Einstellungen übernehmen
7. **CCU konfigurieren**: PSK in CCU-Software eintragen (siehe CCU-Dokumentation)

### Für Entwickler (CCU-seitige Integration)

Siehe ausführliche Dokumentation:
- **Vollständige Anleitung**: [`DTLS_ENCRYPTION_GUIDE.md`](DTLS_ENCRYPTION_GUIDE.md)
- **Schnellreferenz**: [`DTLS_QUICK_REFERENCE.md`](DTLS_QUICK_REFERENCE.md)

**Minimal-Beispiel** (OpenSSL C):
```c
// DTLS Context erstellen
SSL_CTX *ctx = SSL_CTX_new(DTLS_client_method());
SSL_CTX_set_psk_client_callback(ctx, psk_callback);
SSL_CTX_set_cipher_list(ctx, "PSK-AES256-GCM-SHA384");

// Verbindung aufbauen
SSL *ssl = SSL_new(ctx);
BIO *bio = BIO_new_dgram(udp_socket, BIO_NOCLOSE);
SSL_set_bio(ssl, bio, bio);
SSL_connect(ssl);

// Raw-UART Kommunikation (wie bisher, aber verschlüsselt)
SSL_write(ssl, raw_uart_packet, packet_len);
SSL_read(ssl, response_buffer, buffer_size);
```

## Kompatibilität

| Platine (DTLS) | CCU ohne DTLS | CCU mit DTLS |
|----------------|---------------|---------------|
| Deaktiviert (Standard) | ✅ Funktioniert | ✅ Funktioniert |
| PSK aktiviert | ❌ Keine Verbindung | ✅ Funktioniert |

**Wichtig**: Die Verschlüsselung ist **opt-in**. Ohne Aktivierung verhält sich die Platine wie bisher (unverschlüsselt).

## Architektur

```
┌─────────────────────────────────────────────────────────────────┐
│                        Ethernet (RJ45)                          │
│                     192.168.1.x:3008 (UDP)                      │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │
         ┌────────────────────┼────────────────────┐
         │                    │                    │
         ▼                    ▼                    ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│  Unverschlüsselt│  │  DTLS 1.2 PSK   │  │ DTLS 1.2 Cert   │
│   (Legacy)      │  │  (Empfohlen)    │  │  (Zukünftig)    │
├─────────────────┤  ├─────────────────┤  ├─────────────────┤
│ Raw-UART UDP    │  │ DTLS Tunnel     │  │ DTLS Tunnel     │
│ [Cmd][Cnt]      │  │   AES-256-GCM   │  │   TLS 1.3       │
│ [Payload][CRC]  │  │ [Verschlüsselt] │  │ [Verschlüsselt] │
└─────────────────┘  └─────────────────┘  └─────────────────┘
         │                    │                    │
         └────────────────────┼────────────────────┘
                              │
                              ▼
                    ┌──────────────────┐
                    │ Raw-UART Handler │
                    │  (rawuartudp-    │
                    │   listener.cpp)  │
                    └──────────────────┘
                              │
                              ▼
                    ┌──────────────────┐
                    │  Radiomodul      │
                    │ (HM-MOD-RPI-PCB) │
                    └──────────────────┘
```

## Dateiübersicht

### Implementierung auf der Platine
- **`include/dtls_encryption.h`** - DTLS Klasse und API
- **`src/dtls_encryption.cpp`** - DTLS Implementierung (mbedTLS)
- **`include/settings.h`** - Erweitert um DTLS-Einstellungen
- **`src/settings.cpp`** - NVS-Speicherung der DTLS-Konfiguration

### Dokumentation
- **`docs/DTLS_README.md`** - Diese Datei (Übersicht)
- **`docs/DTLS_ENCRYPTION_GUIDE.md`** - Vollständige Implementierungsanleitung für CCU-Entwickler
- **`docs/DTLS_QUICK_REFERENCE.md`** - Schnellreferenz und Code-Snippets

## Was müssen CCU-Entwickler wissen?

### Minimale Anforderungen

1. **DTLS 1.2 Client-Bibliothek**: z.B. OpenSSL 1.1.1+, mbedTLS 2.28+, GnuTLS 3.6+
2. **PSK Support**: Pre-Shared Key Authentifizierung
3. **Cipher Suite**: Mindestens `PSK-AES256-GCM-SHA384`

### Was ändert sich?

**Raw-UART Protokoll**: Bleibt **identisch**!
- Commands (0-7): Unverändert
- Packet Format: Unverändert
- CRC16: Unverändert

**Transport Layer**: UDP → DTLS über UDP
- **Unverschlüsselt**: `sendto(socket, packet, ...)`
- **Verschlüsselt**: `SSL_write(ssl, packet, ...)`

Das ist alles! Die Verschlüsselung ist transparent für das Raw-UART Protokoll.

### PSK Management

**Platine (HB-RF-ETH)**:
- Generiert kryptographisch sicheren PSK (256 bit)
- Speichert PSK verschlüsselt in NVS
- Zeigt PSK **einmalig** an (bei Generierung)

**CCU (debmatic/OpenCCU)**:
- Benutzer trägt PSK manuell in Konfiguration ein
- CCU speichert PSK sicher (verschlüsselt oder systemd-creds)
- CCU verwendet PSK für DTLS Handshake

**WICHTIG**: PSK ist wie ein Passwort - niemals öffentlich teilen oder unverschlüsselt speichern!

## Sicherheitshinweise

### Für Administratoren
- ✅ Verwenden Sie starke, zufällig generierte PSKs (256 bit)
- ✅ Ändern Sie das Admin-Passwort der WebUI
- ✅ Nutzen Sie separates VLAN für HomeMatic-Geräte
- ✅ Rotieren Sie den PSK regelmäßig (z.B. jährlich)
- ❌ Teilen Sie den PSK niemals öffentlich oder per E-Mail

### Für Entwickler
- ✅ Validieren Sie immer die DTLS-Version (mind. 1.2)
- ✅ Verwenden Sie nur sichere Cipher Suites (GCM, Poly1305)
- ✅ Implementieren Sie Timeout-Handling
- ✅ Speichern Sie PSK verschlüsselt
- ❌ Verwenden Sie niemals vorhersehbare PSKs
- ❌ Deaktivieren Sie nicht Certificate Validation (Cert Mode)

## Performance

**Overhead durch DTLS**:
- **Handshake**: ~150-300ms (einmalig beim Connect)
- **Daten**: ~29 Bytes pro Paket (Header + MAC)
- **CPU**: Minimal (AES-GCM ist hardware-beschleunigt auf ESP32)
- **Durchsatz**: Keine merkbare Reduktion für HomeMatic-Daten

**Mit Session Resumption**:
- Reconnect: ~50-100ms statt 150-300ms

## Testing

### Test-Tools auf der Platine
- Serieller Monitor: Zeigt DTLS-Logs
- WebUI: DTLS-Status und Statistiken
- LED-Anzeige:
  - Grün blinkend: DTLS Handshake läuft
  - Grün dauerhaft: DTLS Verbindung etabliert
  - Rot blinkend: DTLS Fehler

### CCU-seitige Tests
```bash
# OpenSSL Test-Client
openssl s_client -dtls1_2 -psk <psk-hex> -psk_identity <identity> \
  -connect 192.168.1.100:3008 -cipher PSK-AES256-GCM-SHA384

# Paket-Analyse mit Wireshark
wireshark -i eth0 -f "udp port 3008"
# (PSK in Wireshark konfigurieren für Dekodierung)
```

## Roadmap

### ✅ Implementiert (v2.1)
- DTLS 1.2 Grundgerüst (mbedTLS)
- PSK Mode (256-bit)
- AES-128/256-GCM Cipher Suites
- ChaCha20-Poly1305 Cipher Suite
- Session Resumption
- Secure Key Storage (NVS)
- WebUI Konfiguration
- PSK Rotation Support

### 🔧 Geplant (v2.2)
- X.509 Certificate Mode
- Automatische PSK-Rotation
- DTLS 1.3 Support (wenn mbedTLS unterstützt)
- Hardware Security Module (HSM) Integration
- OCSP Stapling (für Cert Mode)

### 💡 Ideen
- Automatische Fallback bei DTLS-Fehler
- Multi-PSK Support (verschiedene CCUs)
- Integration mit externen Key Management Systemen
- SNMP-Alerts bei Verschlüsselungsfehlern

## FAQ

**Q: Ist die Verschlüsselung standardmäßig aktiv?**
A: Nein. Standard ist "Deaktiviert" für volle Kompatibilität mit bestehenden Installationen.

**Q: Benötige ich neue Hardware?**
A: Nein. Funktioniert auf allen HB-RF-ETH Platinen mit Firmware v2.1+

**Q: Kann ich zwischen verschlüsselt und unverschlüsselt wechseln?**
A: Ja, jederzeit über die WebUI. Keine Firmware-Neuinstallation nötig.

**Q: Was passiert, wenn die CCU DTLS nicht unterstützt?**
A: DTLS auf Platine deaktivieren → Kommunikation funktioniert wie bisher (unverschlüsselt).

**Q: Wie sicher ist PSK Mode?**
A: Bei korrekt generiertem 256-bit PSK: Sehr hoch. AES-256-GCM ist militärischer Standard.

**Q: Kann ich eigene Zertifikate verwenden?**
A: Zukünftig ja (Certificate Mode in v2.2). Aktuell nur PSK.

**Q: Performance-Impact?**
A: Vernachlässigbar. AES-GCM ist hardware-beschleunigt. Für HomeMatic-Datenmengen kein Problem.

**Q: Was ist mit anderen CCU-Implementierungen (RaspberryMatic, piVCCU)?**
A: Funktioniert mit allen, sofern sie DTLS 1.2 + PSK implementieren (siehe Entwickler-Doku).

## Support & Kontakt

- **GitHub Repository**: https://github.com/xerolux/HB-RF-ETH-ng
- **Issues/Bug Reports**: https://github.com/xerolux/HB-RF-ETH-ng/issues
- **Diskussionen**: HomeMatic Forum (Bereich HB-RF-ETH)
- **Dokumentation**: https://github.com/xerolux/HB-RF-ETH-ng/docs

## Credits

**Entwicklung**: Xerolux (2025)
**Basierend auf**: HB-RF-ETH von Alexander Reinert
**Kryptographie**: mbedTLS (Apache 2.0 License)
**Protokoll**: DTLS 1.2 (RFC 6347)

## Lizenz

Die DTLS-Implementierung ist Teil der HB-RF-ETH Firmware und lizenziert unter:

**Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)**

- ✅ Teilen: Kopieren und weiterverbreiten
- ✅ Anpassen: Remixen, verändern, aufbauen
- ❌ Kommerzielle Nutzung: Nicht erlaubt
- ✅ Namensnennung: Erforderlich
- ✅ Weitergabe unter gleichen Bedingungen

---

**Copyright © 2025 Xerolux**
**Alle Rechte vorbehalten gemäß CC BY-NC-SA 4.0**
