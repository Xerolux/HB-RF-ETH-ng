# DTLS Verschlüsselung - Schnellreferenz für CCU-Entwickler

## Übersicht in 60 Sekunden

Die HB-RF-ETH Platine kann optional DTLS 1.2 Verschlüsselung für die Raw-UART UDP Kommunikation aktivieren.

- **Port**: UDP 3008 (wie bisher)
- **Protokoll**: DTLS 1.2 über UDP
- **Modus**: Pre-Shared Key (PSK)
- **Cipher**: `PSK-AES256-GCM-SHA384` (Standard)
- **Abwärtskompatibel**: Ja (wenn DTLS deaktiviert)
- **⚠️ NUR für Raw-UART UDP Modus** - NICHT kompatibel mit HM-LGW oder Analyzer!

## Minimal-Implementierung (OpenSSL C)

```c
#include <openssl/ssl.h>

// 1. PSK Callback
unsigned char g_psk[32];
size_t g_psk_len = 32;
char g_psk_id[64] = "HB-RF-ETH-AABBCCDDEEFF";

unsigned int psk_cb(SSL *ssl, const char *hint, char *id,
                    unsigned int max_id_len, unsigned char *psk,
                    unsigned int max_psk_len) {
    snprintf(id, max_id_len, "%s", g_psk_id);
    memcpy(psk, g_psk, g_psk_len);
    return g_psk_len;
}

// 2. DTLS Context erstellen
SSL_CTX *ctx = SSL_CTX_new(DTLS_client_method());
SSL_CTX_set_min_proto_version(ctx, DTLS1_2_VERSION);
SSL_CTX_set_psk_client_callback(ctx, psk_cb);
SSL_CTX_set_cipher_list(ctx, "PSK-AES256-GCM-SHA384");

// 3. UDP Socket + DTLS
int sock = socket(AF_INET, SOCK_DGRAM, 0);
// ... connect to board:3008 ...

SSL *ssl = SSL_new(ctx);
BIO *bio = BIO_new_dgram(sock, BIO_NOCLOSE);
SSL_set_bio(ssl, bio, bio);

// 4. Handshake
if (SSL_connect(ssl) <= 0) {
    // Fehlerbehandlung
}

// 5. Kommunikation (wie unverschlüsselt, aber über SSL_read/SSL_write)
uint8_t packet[256] = {0, 0, 2, 0, 0, 0};  // Connect v2
calculate_crc16(packet, 4);  // CRC an Ende

SSL_write(ssl, packet, 6);  // Verschlüsselt senden
SSL_read(ssl, response, sizeof(response));  // Verschlüsselt empfangen
```

## Was ändert sich für bestehende Raw-UART Implementierungen?

### Ohne DTLS (bisher):
```c
sendto(sock, packet, len, 0, &addr, sizeof(addr));
recvfrom(sock, buffer, sizeof(buffer), 0, NULL, NULL);
```

### Mit DTLS:
```c
SSL_write(ssl, packet, len);
SSL_read(ssl, buffer, sizeof(buffer));
```

Das Raw-UART Protokoll (Commands, Counter, CRC16) bleibt **identisch**!

## PSK Verwaltung

### PSK vom Benutzer erhalten
1. Benutzer generiert PSK auf Platine via WebUI
2. Platine zeigt PSK als Hex-String: `A1B2C3D4E5F6...` (64 Zeichen = 256 bit)
3. Benutzer kopiert PSK in CCU-Konfiguration

### PSK in CCU laden
```c
// Aus verschlüsselter Config-Datei laden
FILE *f = fopen("/etc/hb-rf-eth/psk.conf", "r");
char psk_hex[65];
fscanf(f, "%64s", psk_hex);
fclose(f);

// Hex → Binary
for (int i = 0; i < 32; i++) {
    sscanf(psk_hex + 2*i, "%2hhx", &g_psk[i]);
}
g_psk_len = 32;
```

## Fehlerbehandlung

### DTLS Handshake schlägt fehl
```c
if (SSL_connect(ssl) <= 0) {
    unsigned long err = ERR_get_error();
    char errbuf[256];
    ERR_error_string_n(err, errbuf, sizeof(errbuf));

    // Typische Fehler:
    // - PSK nicht korrekt → "wrong psk"
    // - Timeout → "handshake timeout"
    // - Cipher Suite nicht unterstützt → "no ciphers available"

    fprintf(stderr, "DTLS Fehler: %s\n", errbuf);

    // Fallback: DTLS deaktiviert?
    // → Versuche unverschlüsselte Verbindung
}
```

### Keep-Alive
```c
// Alle 1 Sekunde Keep-Alive senden (Command 2)
while (connected) {
    uint8_t keepalive[4] = {2, counter++, 0, 0};
    calculate_crc16(keepalive, 2);
    SSL_write(ssl, keepalive, 4);
    sleep(1);
}
```

## Detektion: DTLS aktiv oder nicht?

Die CCU kann testen, ob DTLS auf der Platine aktiviert ist:

```c
// Versuch 1: DTLS Handshake
if (SSL_connect(ssl) > 0) {
    // DTLS ist aktiv
    use_encrypted = true;
} else {
    // Versuch 2: Unverschlüsselt senden
    uint8_t test[6] = {0, 0, 2, 0, 0x12, 0x34};  // Test-Connect
    sendto(sock, test, 6, 0, &addr, sizeof(addr));

    uint8_t response[256];
    int len = recvfrom(sock, response, sizeof(response),
                       MSG_DONTWAIT, NULL, NULL);
    if (len > 0) {
        // Unverschlüsselte Antwort → DTLS deaktiviert
        use_encrypted = false;
    }
}
```

## Kompatibilität

| DTLS Mode auf Platine | CCU ohne DTLS | CCU mit DTLS |
|-----------------------|---------------|--------------|
| Deaktiviert (0)       | ✅ Funktioniert | ✅ Funktioniert (Fallback) |
| PSK (1)               | ❌ Keine Verbindung | ✅ Funktioniert |

**Empfehlung**: CCU sollte beide Modi unterstützen (mit Auto-Detektion).

## Benötigte Libraries

### debmatic / OpenCCU (Debian-basiert)
```bash
apt-get install libssl-dev
```

### Python (Prototyping/Testing)
```bash
pip3 install Dtls
```

## Cipher Suites (Platine → CCU)

| Platine Setting | OpenSSL Cipher String |
|-----------------|----------------------|
| 0: AES-128-GCM | `PSK-AES128-GCM-SHA256` |
| 1: AES-256-GCM (Standard) | `PSK-AES256-GCM-SHA384` |
| 2: ChaCha20-Poly1305 | `PSK-CHACHA20-POLY1305-SHA256` |

**WICHTIG**: Cipher Suite auf CCU muss mit Platinen-Konfiguration übereinstimmen!

## Beispiel-Konfiguration (CCU)

```ini
# /etc/hb-rf-eth/config.ini
[encryption]
enabled = true
mode = psk
cipher = PSK-AES256-GCM-SHA384
psk_file = /etc/hb-rf-eth/psk.key
psk_identity_file = /etc/hb-rf-eth/psk.id
session_resumption = true

[connection]
board_ip = 192.168.1.100
port = 3008
timeout = 10
keepalive_interval = 1
```

## Sicherheits-Checkliste

- [ ] PSK hat mindestens 256 Bit (32 Bytes)
- [ ] PSK ist kryptographisch zufällig generiert
- [ ] PSK wird verschlüsselt auf CCU gespeichert
- [ ] Minimum DTLS 1.2 (kein 1.0/1.1)
- [ ] Nur GCM oder Poly1305 Cipher Suites
- [ ] Certificate Validation bei Cert Mode
- [ ] Timeouts konfiguriert (Handshake, Keep-Alive)
- [ ] Error Handling implementiert

## Performance-Tipps

1. **Session Resumption aktivieren**: Spart 200ms bei Reconnects
2. **AES-256-GCM bevorzugen**: Hardware-beschleunigt auf ESP32
3. **Keep-Alive optimieren**: 1 Sekunde ist optimal
4. **Connection Pooling**: Verbindung halten statt ständig neu aufbauen

## Debugging

### OpenSSL Debug-Modus
```c
SSL_CTX_set_info_callback(ctx, ssl_info_callback);

void ssl_info_callback(const SSL *ssl, int where, int ret) {
    if (where & SSL_CB_HANDSHAKE_START) {
        printf("DTLS Handshake gestartet\n");
    }
    if (where & SSL_CB_HANDSHAKE_DONE) {
        printf("DTLS Handshake abgeschlossen\n");
        printf("Cipher: %s\n", SSL_get_cipher(ssl));
    }
}
```

### Wireshark DTLS Dekodierung
```
Edit → Preferences → Protocols → TLS
→ (Pre)-Master-Secret log filename: /tmp/sslkeys.log
```

In CCU-Programm:
```c
// Vor SSL_connect():
setenv("SSLKEYLOGFILE", "/tmp/sslkeys.log", 1);
```

## API-Referenz: Raw-UART Commands über DTLS

Alle Commands bleiben identisch, nur Transport ändert sich:

| Command | Name | Payload | Verschlüsselt? |
|---------|------|---------|----------------|
| 0 | Connect | Protocol Version, Endpoint ID | ✅ |
| 1 | Disconnect | - | ✅ |
| 2 | Keep-Alive | - | ✅ |
| 3 | LED | RGB Bits | ✅ |
| 4 | Reset | - | ✅ |
| 5 | Start Connection | - | ✅ |
| 6 | End Connection | - | ✅ |
| 7 | Frame | HomeMatic Frame Data | ✅ |

**Alle Payloads werden durch DTLS verschlüsselt übertragen!**

## Support

- **Vollständige Dokumentation**: `docs/DTLS_ENCRYPTION_GUIDE.md`
- **GitHub Issues**: https://github.com/xerolux/HB-RF-ETH-ng/issues
- **HomeMatic Forum**: HB-RF-ETH Bereich

---

**Lizenz**: Creative Commons BY-NC-SA 4.0
**Copyright**: 2025 Xerolux
