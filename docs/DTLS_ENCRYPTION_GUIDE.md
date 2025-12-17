# DTLS Verschlüsselung für HB-RF-ETH - Implementierungsanleitung

## Überblick

Dieses Dokument beschreibt die DTLS/TLS-Verschlüsselung für die Raw-UART UDP Kommunikation zwischen der HB-RF-ETH Platine und der CCU (Homematic Central Unit). Die Verschlüsselung ist **optional** und kann über die WebUI aktiviert werden.

## Sicherheitsziele

- **Vertraulichkeit**: Niemand kann die Funkdaten zwischen Platine und CCU mitlesen
- **Integrität**: Manipulation der Daten wird erkannt und verhindert
- **Authentifizierung**: Nur autorisierte CCU kann sich mit der Platine verbinden
- **Replay-Schutz**: Alte Pakete können nicht erneut eingespielt werden
- **Forward Secrecy**: Kompromittierung eines Sitzungsschlüssels gefährdet keine alten Verbindungen

## Verschlüsselungsmodi

### 1. Modus: Deaktiviert (Standard)
- Keine Verschlüsselung
- Kompatibel mit allen bestehenden Installationen
- **Achtung**: Daten werden unverschlüsselt über das Netzwerk übertragen!

### 2. Modus: Pre-Shared Key (PSK) - **Empfohlen**
- Symmetrische Verschlüsselung mit gemeinsam genutztem Schlüssel
- Einfache Einrichtung ohne Zertifikate
- Geringer Overhead, hohe Performance
- **Sicherheitslevel**: Sehr hoch (bei sicherem Schlüssel)

#### Verfügbare Cipher Suites:
1. **AES-128-GCM-SHA256** (schnell, gute Sicherheit)
2. **AES-256-GCM-SHA384** (Standard, maximale Sicherheit)
3. **ChaCha20-Poly1305-SHA256** (moderne Alternative, gut für Embedded)

### 3. Modus: X.509 Zertifikate (Zukünftig)
- Zertifikat-basierte gegenseitige Authentifizierung
- Höchster Sicherheitsstandard
- **Status**: Noch nicht vollständig implementiert

---

## Protokoll-Spezifikation

### Transport Layer: DTLS 1.2 über UDP

Die HB-RF-ETH Platine lauscht auf **UDP Port 3008** für Raw-UART Pakete. Mit aktivierter Verschlüsselung werden alle Pakete über DTLS 1.2 gesichert.

### DTLS Handshake

```
CCU                                     HB-RF-ETH Board
 |                                              |
 |  ClientHello (PSK_WITH_AES_256_GCM_SHA384)  |
 |--------------------------------------------->|
 |                                              |
 |  ServerHello, PSK Extension                 |
 |<---------------------------------------------|
 |                                              |
 |  ChangeCipherSpec, Finished                 |
 |--------------------------------------------->|
 |                                              |
 |  ChangeCipherSpec, Finished                 |
 |<---------------------------------------------|
 |                                              |
 |  === DTLS Tunnel etabliert ===              |
 |                                              |
 |  Raw-UART Connect (verschlüsselt)           |
 |--------------------------------------------->|
 |                                              |
 |  Raw-UART Connect Response (verschlüsselt)  |
 |<---------------------------------------------|
 |                                              |
 |  Raw-UART Frames (verschlüsselt)            |
 |<-------------------------------------------->|
```

### Paketformat

**Ohne Verschlüsselung** (Legacy):
```
[Command][Counter][Payload][CRC16]
```

**Mit DTLS-Verschlüsselung**:
```
[DTLS Header (13 Bytes)]
[Encrypted: [Command][Counter][Payload][CRC16]]
[DTLS MAC (16 Bytes für GCM)]
```

Die Payload wird **vollständig** durch DTLS geschützt. CRC16 bleibt innerhalb der verschlüsselten Payload zur Kompatibilität.

---

## CCU-seitige Implementierung

### Voraussetzungen

Die CCU-seitige Software muss DTLS 1.2 mit PSK-Unterstützung implementieren. Dies kann mit folgenden Bibliotheken erreicht werden:

#### Linux/debmatic/OpenCCU (empfohlen):

1. **OpenSSL 1.1.1+** (empfohlen)
   ```bash
   apt-get install libssl-dev
   ```

2. **mbedTLS 2.28+** (Alternative, kleiner Footprint)
   ```bash
   apt-get install libmbedtls-dev
   ```

3. **GnuTLS 3.6+** (Alternative)
   ```bash
   apt-get install libgnutls28-dev
   ```

### Implementierung mit OpenSSL (C/C++)

#### 1. DTLS Client Setup

```c
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Globale Variablen für PSK
static unsigned char psk_key[32];  // 256-bit PSK
static size_t psk_key_len = 32;
static char psk_identity[64] = "HB-RF-ETH-AABBCCDDEEFF";

// PSK Callback Funktion
static unsigned int psk_client_callback(SSL *ssl, const char *hint,
                                        char *identity, unsigned int max_identity_len,
                                        unsigned char *psk, unsigned int max_psk_len)
{
    // Identity setzen
    snprintf(identity, max_identity_len, "%s", psk_identity);

    // PSK kopieren
    if (psk_key_len > max_psk_len) {
        fprintf(stderr, "PSK zu lang\n");
        return 0;
    }

    memcpy(psk, psk_key, psk_key_len);
    return psk_key_len;
}

// DTLS Client initialisieren
SSL_CTX* init_dtls_client()
{
    SSL_CTX *ctx;

    // OpenSSL initialisieren
    SSL_library_init();
    SSL_load_error_strings();

    // DTLS 1.2 Client Context erstellen
    ctx = SSL_CTX_new(DTLS_client_method());
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    // Minimum DTLS Version auf 1.2 setzen (Sicherheit!)
    SSL_CTX_set_min_proto_version(ctx, DTLS1_2_VERSION);

    // PSK Callback registrieren
    SSL_CTX_set_psk_client_callback(ctx, psk_client_callback);

    // Cipher Suite setzen (AES-256-GCM wie auf Board konfiguriert)
    if (!SSL_CTX_set_cipher_list(ctx, "PSK-AES256-GCM-SHA384")) {
        fprintf(stderr, "Cipher Suite nicht verfügbar\n");
        SSL_CTX_free(ctx);
        return NULL;
    }

    // Optional: Session Resumption aktivieren für schnellere Reconnects
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_CLIENT);

    return ctx;
}

// UDP Socket erstellen und DTLS Verbindung aufbauen
SSL* connect_to_board(SSL_CTX *ctx, const char *board_ip, uint16_t port)
{
    int sock;
    struct sockaddr_in server_addr;
    SSL *ssl;
    BIO *bio;

    // UDP Socket erstellen
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return NULL;
    }

    // Server Adresse konfigurieren
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, board_ip, &server_addr.sin_addr);

    // Mit Server verbinden (UDP connect für Filterung)
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return NULL;
    }

    // SSL Objekt erstellen
    ssl = SSL_new(ctx);
    if (!ssl) {
        ERR_print_errors_fp(stderr);
        close(sock);
        return NULL;
    }

    // BIO für DTLS über UDP erstellen
    bio = BIO_new_dgram(sock, BIO_NOCLOSE);
    if (!bio) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(sock);
        return NULL;
    }

    // Timeout für DTLS Handshake setzen (10 Sekunden)
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

    SSL_set_bio(ssl, bio, bio);

    // DTLS Handshake durchführen
    printf("Starte DTLS Handshake...\n");
    if (SSL_connect(ssl) <= 0) {
        fprintf(stderr, "DTLS Handshake fehlgeschlagen:\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(sock);
        return NULL;
    }

    printf("DTLS Verbindung erfolgreich!\n");
    printf("Cipher: %s\n", SSL_get_cipher(ssl));

    return ssl;
}

// Raw-UART Paket senden (verschlüsselt)
int send_rawuart_packet(SSL *ssl, uint8_t command, uint8_t *payload, size_t payload_len)
{
    uint8_t buffer[2048];
    static uint8_t counter = 0;
    size_t total_len = 2 + payload_len + 2;  // Command + Counter + Payload + CRC16

    // Paket zusammenbauen
    buffer[0] = command;
    buffer[1] = counter++;

    if (payload && payload_len > 0) {
        memcpy(buffer + 2, payload, payload_len);
    }

    // CRC16 berechnen (HomeMatic CRC16)
    uint16_t crc = calculate_hmframe_crc16(buffer, total_len - 2);
    buffer[total_len - 2] = (crc >> 8) & 0xFF;
    buffer[total_len - 1] = crc & 0xFF;

    // Über DTLS verschlüsselt senden
    int ret = SSL_write(ssl, buffer, total_len);
    if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);
        fprintf(stderr, "SSL_write Fehler: %d\n", err);
        return -1;
    }

    return ret;
}

// Raw-UART Paket empfangen (entschlüsselt)
int recv_rawuart_packet(SSL *ssl, uint8_t *buffer, size_t buffer_size)
{
    int ret = SSL_read(ssl, buffer, buffer_size);

    if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
            return 0;  // Keine Daten verfügbar
        }
        fprintf(stderr, "SSL_read Fehler: %d\n", err);
        return -1;
    }

    // CRC16 validieren
    if (ret >= 4) {
        uint16_t received_crc = (buffer[ret - 2] << 8) | buffer[ret - 1];
        uint16_t calculated_crc = calculate_hmframe_crc16(buffer, ret - 2);

        if (received_crc != calculated_crc) {
            fprintf(stderr, "CRC16 Fehler!\n");
            return -1;
        }
    }

    return ret;
}

// Beispiel: Verbindung zur Platine aufbauen
int main()
{
    SSL_CTX *ctx;
    SSL *ssl;

    // PSK aus sicherem Speicher laden (siehe unten)
    if (!load_psk_from_secure_storage(psk_key, &psk_key_len, psk_identity)) {
        fprintf(stderr, "PSK konnte nicht geladen werden\n");
        return 1;
    }

    // DTLS Client initialisieren
    ctx = init_dtls_client();
    if (!ctx) {
        return 1;
    }

    // Verbindung zur Platine aufbauen
    ssl = connect_to_board(ctx, "192.168.1.100", 3008);
    if (!ssl) {
        SSL_CTX_free(ctx);
        return 1;
    }

    // Raw-UART Connect senden (Command 0, Protocol Version 2)
    uint8_t connect_payload[] = {2, 0};  // Protocol v2, new connection
    send_rawuart_packet(ssl, 0, connect_payload, 2);

    // Response empfangen
    uint8_t response[256];
    int len = recv_rawuart_packet(ssl, response, sizeof(response));
    if (len > 0) {
        printf("Connect Response empfangen, Länge: %d\n", len);
    }

    // ... weitere Kommunikation ...

    // Keep-Alive Thread starten (alle 1 Sekunde Command 2 senden)
    // ... siehe Implementierung unten ...

    // Cleanup
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    return 0;
}
```

#### 2. CRC16 Implementierung (HomeMatic kompatibel)

```c
// HomeMatic CRC16 Berechnung (Polynomial 0x1021)
uint16_t calculate_hmframe_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;

        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}
```

### Implementierung mit Python (für Tests/Prototyping)

```python
#!/usr/bin/env python3
import socket
import ssl
import struct
import time

class DTLSRawUartClient:
    def __init__(self, board_ip, port=3008):
        self.board_ip = board_ip
        self.port = port
        self.psk_key = None
        self.psk_identity = None
        self.sock = None
        self.ssl_sock = None
        self.counter = 0

    def set_psk(self, key_hex, identity):
        """PSK setzen (key_hex als Hex-String, z.B. 'AABBCCDD...')"""
        self.psk_key = bytes.fromhex(key_hex)
        self.psk_identity = identity.encode('utf-8')

    def connect(self):
        """DTLS Verbindung aufbauen"""
        # UDP Socket erstellen
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.connect((self.board_ip, self.port))

        # DTLS Context erstellen
        # HINWEIS: Python's ssl Modul unterstützt DTLS nicht nativ!
        # Verwenden Sie python-dtls Bibliothek:
        # pip install Dtls

        from dtls import do_patch
        do_patch()

        context = ssl.SSLContext(ssl.PROTOCOL_DTLSv1_2)
        context.set_ciphers('PSK-AES256-GCM-SHA384')

        # PSK konfigurieren (erfordert python-dtls mit PSK Support)
        # ... siehe python-dtls Dokumentation ...

        self.ssl_sock = context.wrap_socket(self.sock, server_hostname=None)
        print("DTLS Verbindung hergestellt")

    def send_packet(self, command, payload=b''):
        """Raw-UART Paket senden"""
        packet = bytearray()
        packet.append(command)
        packet.append(self.counter)
        self.counter = (self.counter + 1) & 0xFF
        packet.extend(payload)

        # CRC16 berechnen
        crc = self.crc16(packet)
        packet.extend(struct.pack('>H', crc))

        self.ssl_sock.send(bytes(packet))

    def recv_packet(self, timeout=1.0):
        """Raw-UART Paket empfangen"""
        self.ssl_sock.settimeout(timeout)
        try:
            data = self.ssl_sock.recv(2048)
            if len(data) < 4:
                return None

            # CRC validieren
            received_crc = struct.unpack('>H', data[-2:])[0]
            calculated_crc = self.crc16(data[:-2])

            if received_crc != calculated_crc:
                print("CRC Fehler!")
                return None

            return data
        except socket.timeout:
            return None

    @staticmethod
    def crc16(data):
        """HomeMatic CRC16"""
        crc = 0xFFFF
        for byte in data:
            crc ^= byte << 8
            for _ in range(8):
                if crc & 0x8000:
                    crc = (crc << 1) ^ 0x1021
                else:
                    crc <<= 1
                crc &= 0xFFFF
        return crc

# Beispiel Verwendung
if __name__ == '__main__':
    client = DTLSRawUartClient('192.168.1.100')

    # PSK konfigurieren (aus sicherem Speicher laden!)
    psk_hex = 'AABBCCDDEEFF...'  # 256-bit PSK in Hex
    client.set_psk(psk_hex, 'HB-RF-ETH-AABBCCDDEEFF')

    client.connect()

    # Connect senden
    client.send_packet(0, b'\x02\x00')  # Protocol v2

    # Response empfangen
    response = client.recv_packet()
    if response:
        print(f"Response: {response.hex()}")
```

---

## PSK Management

### PSK Generierung auf der Platine

Die Platine kann automatisch einen kryptographisch sicheren PSK generieren:

1. **Über WebUI**:
   - Navigation zu "Einstellungen" → "Sicherheit" → "DTLS Verschlüsselung"
   - "PSK generieren" Button klicken
   - PSK wird angezeigt (einmalig!) zum Kopieren

2. **Manuelle Generierung** (für maximale Kontrolle):
   ```bash
   # 256-bit (32 Byte) zufälliger Schlüssel
   openssl rand -hex 32
   ```
   Diesen Schlüssel dann über WebUI eintragen.

### PSK Speicherung auf der CCU

**WICHTIG**: Der PSK ist das Geheimnis, das beide Seiten kennen müssen. Er sollte **NIEMALS** im Klartext in Konfigurationsdateien gespeichert werden!

#### Empfohlene Speichermethoden:

1. **Verschlüsselte Konfigurationsdatei**:
   ```bash
   # PSK verschlüsselt speichern
   echo "PSK_KEY=abc123..." | openssl enc -aes-256-cbc -salt -pbkdf2 \
     -out /etc/hb-rf-eth/psk.enc -pass pass:$(hostname)

   # PSK entschlüsseln beim Programmstart
   PSK=$(openssl enc -aes-256-cbc -d -pbkdf2 \
     -in /etc/hb-rf-eth/psk.enc -pass pass:$(hostname))
   ```

2. **Systemd Credentials** (moderne Linux Systeme):
   ```bash
   # PSK in systemd credential speichern
   systemd-creds encrypt --name=hb-rf-eth-psk - psk.cred

   # Im Service laden
   LoadCredentialEncrypted=hb-rf-eth-psk:/etc/hb-rf-eth/psk.cred
   ```

3. **Hardware Security Module** (für höchste Sicherheit):
   - TPM 2.0
   - Secure Enclave

### PSK Rotation

Für maximale Sicherheit sollte der PSK regelmäßig gewechselt werden:

1. **Neuen PSK auf Platine generieren**
2. **Alten PSK als "Previous PSK" markieren** (Platine akzeptiert kurzzeitig beide)
3. **CCU-Software aktualisieren** mit neuem PSK
4. **Nach Bestätigung**: Alten PSK löschen

Die Platine unterstützt eine Übergangsphase, in der sowohl alter als auch neuer PSK akzeptiert werden.

---

## Sicherheitshinweise

### Für Entwickler

1. **PSK Länge**: Minimum 128 Bit (16 Bytes), empfohlen 256 Bit (32 Bytes)

2. **PSK Qualität**: Niemals vorhersehbare Werte verwenden:
   - ❌ FALSCH: `"password123"`, MAC-Adresse, Seriennummer
   - ✅ RICHTIG: Kryptographisch sicherer Zufallsgenerator (`/dev/urandom`, `openssl rand`)

3. **DTLS Version**: Immer mindestens DTLS 1.2 verwenden, niemals DTLS 1.0

4. **Cipher Suites**: Nur moderne Cipher Suites verwenden:
   - ✅ `PSK-AES256-GCM-SHA384` (empfohlen)
   - ✅ `PSK-AES128-GCM-SHA256`
   - ✅ `PSK-CHACHA20-POLY1305-SHA256`
   - ❌ Keine CBC Mode Ciphers (anfällig für Padding Oracle Attacks)

5. **Certificate Validation** (bei Cert Mode):
   - Immer Zertifikate validieren
   - Certificate Pinning verwenden
   - Auf Certificate Expiry prüfen

6. **Timeout Handling**:
   - DTLS Handshake Timeout: 10 Sekunden
   - Keep-Alive Intervall: 1 Sekunde
   - Connection Timeout: 5 Sekunden (wie unverschlüsselt)

7. **Error Handling**:
   - Bei DTLS Fehler: Verbindung trennen und neu aufbauen
   - Keine Details über Fehler nach außen geben (Information Leakage)

8. **Replay Protection**:
   - DTLS bietet eingebauten Replay-Schutz
   - Counter im Raw-UART Protokoll zusätzlich verwenden

### Für Benutzer

1. **Netzwerksicherheit**:
   - DTLS schützt nur die Übertragung
   - Physischer Zugriff auf die Platine ermöglicht PSK-Extraktion
   - Separates VLAN für HomeMatic-Geräte empfohlen

2. **Passwortschutz**:
   - Admin-Passwort der WebUI ändern!
   - Zugriff auf WebUI nur aus vertrautem Netz

3. **Updates**:
   - Firmware regelmäßig aktualisieren
   - PSK regelmäßig rotieren (z.B. jährlich)

---

## Debugging und Troubleshooting

### DTLS Verbindung schlägt fehl

1. **Cipher Suite Mismatch**:
   ```bash
   # OpenSSL: Verfügbare PSK Cipher Suites anzeigen
   openssl ciphers -v | grep PSK
   ```
   Prüfen, ob gleiche Cipher Suite auf Board und CCU konfiguriert ist.

2. **PSK falsch**:
   - Auf Platine: Serieller Monitor zeigt "PSK auth failed"
   - Auf CCU: OpenSSL gibt "wrong psk identity or key" zurück

   Lösung: PSK auf beiden Seiten neu synchronisieren

3. **DTLS Version Mismatch**:
   - Minimum DTLS 1.2 auf beiden Seiten erforderlich
   - DTLS 1.0/1.1 wird aus Sicherheitsgründen nicht unterstützt

4. **Firewall blockiert**:
   - Port 3008 UDP muss offen sein
   - Prüfen mit: `nc -u 192.168.1.100 3008`

### Paket-Analyse

Mit Wireshark DTLS Traffic analysieren (wenn PSK bekannt):

1. **Wireshark starten** und Interface wählen
2. **Filter**: `udp.port == 3008`
3. **Edit → Preferences → Protocols → TLS**
4. **Pre-Shared Keys hinzufügen**:
   ```
   Identity: HB-RF-ETH-AABBCCDDEEFF
   PSK: <hex-encoded-key>
   ```
5. **Dekodierter DTLS Traffic** wird angezeigt

### Logging aktivieren

**Auf der Platine** (über serielle Konsole):
```
LOG_LOCAL_LEVEL=ESP_LOG_DEBUG
```

**Auf der CCU** (OpenSSL):
```c
SSL_CTX_set_info_callback(ctx, ssl_info_callback);
```

---

## Performance-Überlegungen

### Overhead durch DTLS

- **Handshake**: Ca. 150-300ms (einmalig beim Verbindungsaufbau)
- **Daten-Overhead**: ~29 Bytes pro Paket (DTLS Header + MAC)
- **CPU-Last**: AES-256-GCM ist hardwarebeschleunigt auf ESP32 → minimaler Overhead
- **Durchsatz**: Keine merkbare Reduktion für HomeMatic-Daten

### Session Resumption

Session Resumption reduziert Handshake-Zeit bei Reconnects:
- **Ohne Session Resumption**: 150-300ms
- **Mit Session Resumption**: 50-100ms

Empfehlung: Immer aktiviert lassen!

### Empfohlene Konfiguration

Für beste Performance bei hoher Sicherheit:
- **Modus**: PSK
- **Cipher Suite**: AES-256-GCM (hardware-accelerated)
- **Session Resumption**: Aktiviert
- **Keep-Alive**: 1 Sekunde (wie unverschlüsselt)

---

## Testplan für Entwickler

### Manuelle Tests

1. **Grundfunktion**:
   - [ ] DTLS Handshake erfolgreich
   - [ ] Raw-UART Connect funktioniert
   - [ ] Frames werden korrekt übertragen
   - [ ] Keep-Alive funktioniert

2. **Sicherheit**:
   - [ ] Falscher PSK wird abgelehnt
   - [ ] Replay-Angriff wird erkannt
   - [ ] Man-in-the-Middle wird verhindert

3. **Robustheit**:
   - [ ] Verbindungsabbruch wird erkannt
   - [ ] Automatischer Reconnect funktioniert
   - [ ] Session Resumption funktioniert

### Automatisierte Tests

```bash
# Test 1: Verbindung mit korrektem PSK
./test_dtls_client --psk CORRECT_PSK --expect success

# Test 2: Verbindung mit falschem PSK
./test_dtls_client --psk WRONG_PSK --expect failure

# Test 3: Performance Vergleich
./benchmark_rawuart --encryption off --duration 60
./benchmark_rawuart --encryption dtls-psk --duration 60

# Test 4: Stress Test
./stress_test --clients 10 --duration 300
```

---

## FAQ für CCU-Entwickler

**Q: Muss ich DTLS implementieren oder ist es optional?**
A: Optional. Wenn DTLS auf der Platine deaktiviert ist (Standard), funktioniert die unverschlüsselte Raw-UART Kommunikation wie bisher.

**Q: Welche Bibliothek soll ich verwenden?**
A: Empfehlung: OpenSSL 1.1.1+ für Linux-basierte CCUs (debmatic/OpenCCU). Gut dokumentiert, weit verbreitet, regelmäßige Security Updates.

**Q: Wo bekomme ich den PSK her?**
A: Der Benutzer generiert ihn auf der Platine via WebUI und trägt ihn manuell in die CCU-Konfiguration ein. Ähnlich wie WLAN-Passwort.

**Q: Was ist mit bestehenden Installationen?**
A: Voll kompatibel. Solange DTLS deaktiviert ist, verhält sich die Platine wie früher (unverschlüsselt).

**Q: Kann ich den PSK aus der Platine auslesen?**
A: Nein. Der PSK ist in verschlüsselter NVS-Partition gespeichert. Nur über WebUI (einmalig bei Generierung) oder seriellen Zugriff (Debugging).

**Q: Was passiert bei PSK-Rotation?**
A: Die Platine unterstützt eine Übergangsphase mit zwei PSKs (alt + neu). CCU hat Zeit für Update. Details siehe "PSK Rotation" oben.

**Q: Performance-Impact?**
A: Vernachlässigbar. AES-GCM ist hardware-beschleunigt auf ESP32. DTLS Handshake nur einmal beim Connect.

**Q: Kann ich eigene Zertifikate verwenden?**
A: Zukünftig ja (Certificate Mode). Aktuell nur PSK Mode vollständig implementiert.

**Q: Wie teste ich die Implementierung?**
A: Schrittweise:
1. Ohne Verschlüsselung testen (DTLS Mode = Disabled auf Platine)
2. PSK Mode aktivieren und manuell PSK synchronisieren
3. DTLS Handshake debuggen (OpenSSL Logging)
4. Raw-UART Kommandos über DTLS testen

---

## Kontakt und Support

Bei Fragen zur Implementierung:
- **GitHub Issues**: https://github.com/xerolux/HB-RF-ETH-ng/issues
- **Forum**: HomeMatic Forum (Bereich HB-RF-ETH)
- **Dokumentation**: https://github.com/xerolux/HB-RF-ETH-ng/docs

---

## Änderungshistorie

- **2025-01-XX**: Initiale Version
  - PSK Mode implementiert
  - AES-128/256-GCM und ChaCha20-Poly1305 Cipher Suites
  - Session Resumption Support
  - Secure Key Storage in NVS

---

## Lizenz

Dieses Dokument und die DTLS-Implementierung sind Teil der HB-RF-ETH Firmware und lizenziert unter:

**Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License**

Copyright 2025 Xerolux
