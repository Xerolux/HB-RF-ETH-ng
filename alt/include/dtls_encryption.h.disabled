/*
 *  dtls_encryption.h is part of the HB-RF-ETH firmware v2.1
 *
 *  Copyright 2025 Xerolux
 *  DTLS/TLS Encryption Layer for Raw-UART UDP Protocol
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#pragma once

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "lwip/opt.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include <atomic>
#include <cstring>

/**
 * DTLS Encryption Modes
 *
 * DISABLED: No encryption (legacy mode)
 * PSK: Pre-Shared Key mode - symmetric encryption with shared secret
 * CERTIFICATE: X.509 certificate-based authentication (mutual TLS)
 */
typedef enum
{
    DTLS_MODE_DISABLED = 0,
    DTLS_MODE_PSK = 1,
    DTLS_MODE_CERTIFICATE = 2
} dtls_mode_t;

/**
 * DTLS Cipher Suites
 *
 * Defines supported cipher suites for different security levels
 */
typedef enum
{
    DTLS_CIPHER_AES_128_GCM = 0,      // TLS_PSK_WITH_AES_128_GCM_SHA256
    DTLS_CIPHER_AES_256_GCM = 1,      // TLS_PSK_WITH_AES_256_GCM_SHA384
    DTLS_CIPHER_CHACHA20_POLY1305 = 2 // TLS_PSK_WITH_CHACHA20_POLY1305_SHA256
} dtls_cipher_suite_t;

/**
 * DTLS Connection State
 */
typedef enum
{
    DTLS_STATE_IDLE = 0,
    DTLS_STATE_HANDSHAKING = 1,
    DTLS_STATE_CONNECTED = 2,
    DTLS_STATE_ERROR = 3
} dtls_state_t;

/**
 * DTLS Statistics
 */
struct dtls_stats_t
{
    uint32_t handshakes_completed;
    uint32_t handshakes_failed;
    uint32_t packets_encrypted;
    uint32_t packets_decrypted;
    uint32_t encryption_errors;
    uint32_t decryption_errors;
    uint64_t bytes_encrypted;
    uint64_t bytes_decrypted;
    uint32_t psk_auth_failures;
    uint32_t cert_auth_failures;
};

/**
 * DTLSEncryption Class
 *
 * Provides DTLS 1.2/1.3 encryption for Raw-UART UDP protocol
 *
 * Features:
 * - Pre-Shared Key (PSK) authentication
 * - X.509 certificate-based mutual authentication
 * - Multiple cipher suites (AES-128/256-GCM, ChaCha20-Poly1305)
 * - Perfect Forward Secrecy (PFS)
 * - Session resumption for reduced handshake overhead
 * - Replay protection
 * - Secure key storage in NVS (encrypted partition)
 *
 * Architecture:
 * - Transparent wrapper around Raw-UART UDP protocol
 * - Zero-copy where possible for performance
 * - Thread-safe operations
 * - Automatic key rotation support
 */
class DTLSEncryption
{
private:
    // mbedTLS contexts
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    // PSK contexts
    unsigned char psk_key[64];      // Pre-Shared Key (max 512 bits)
    size_t psk_key_len;
    char psk_identity[64];          // PSK Identity string

    // Certificate contexts (for certificate mode)
    mbedtls_x509_crt server_cert;
    mbedtls_pk_context server_key;
    mbedtls_x509_crt ca_cert;

    // Configuration
    std::atomic<dtls_mode_t> mode;
    std::atomic<dtls_cipher_suite_t> cipher_suite;
    std::atomic<dtls_state_t> state;
    std::atomic<bool> session_resumption_enabled;
    std::atomic<bool> require_client_cert;

    // Statistics
    dtls_stats_t stats;

    // Internal buffers
    unsigned char recv_buffer[2048];
    unsigned char send_buffer[2048];

    // Timer for DTLS handshake timeout
    int64_t handshake_timeout_us;

    // Session cache for resumption
    mbedtls_ssl_session saved_session;
    bool session_valid;

    // Private methods
    int setupPSK();
    int setupCertificate();
    int performHandshake();
    void resetConnection();

    // mbedTLS callbacks
    static int bio_send(void *ctx, const unsigned char *buf, size_t len);
    static int bio_recv(void *ctx, unsigned char *buf, size_t len);
    static int bio_recv_timeout(void *ctx, unsigned char *buf, size_t len, uint32_t timeout);
    static void debug_callback(void *ctx, int level, const char *file, int line, const char *str);
    static int psk_callback(void *parameter, mbedtls_ssl_context *ssl,
                           const unsigned char *psk_identity, size_t identity_len);

public:
    DTLSEncryption();
    ~DTLSEncryption();

    // Initialization and configuration
    bool init(dtls_mode_t encryption_mode, dtls_cipher_suite_t cipher = DTLS_CIPHER_AES_256_GCM);
    void deinit();

    // PSK configuration
    bool setPSK(const unsigned char *key, size_t key_len, const char *identity);
    bool generatePSK(size_t key_bits = 256); // Generate random PSK
    bool getPSK(unsigned char *key_out, size_t *key_len_out, char *identity_out);

    // Certificate configuration
    bool loadServerCertificate(const unsigned char *cert_pem, size_t cert_len);
    bool loadServerKey(const unsigned char *key_pem, size_t key_len);
    bool loadCACertificate(const unsigned char *ca_cert_pem, size_t ca_len);
    bool generateSelfSignedCertificate(); // Generate self-signed cert for testing

    // Encryption/Decryption operations
    int encrypt(const unsigned char *plaintext, size_t plaintext_len,
                unsigned char *ciphertext, size_t *ciphertext_len);
    int decrypt(const unsigned char *ciphertext, size_t ciphertext_len,
                unsigned char *plaintext, size_t *plaintext_len);

    // Connection management
    bool startHandshake(const ip_addr_t *remote_addr, uint16_t remote_port);
    bool isHandshakeComplete();
    void closeConnection();

    // Session resumption
    bool saveSession();
    bool resumeSession();

    // Configuration
    void setSessionResumption(bool enable) { session_resumption_enabled = enable; }
    void setRequireClientCert(bool require) { require_client_cert = require; }
    void setCipherSuite(dtls_cipher_suite_t cipher) { cipher_suite = cipher; }

    // Status and diagnostics
    dtls_state_t getState() const { return state; }
    dtls_mode_t getMode() const { return mode; }
    const dtls_stats_t* getStats() const { return &stats; }
    void resetStats();

    // Key rotation
    bool rotatePSK(); // Generate and set new PSK

    // Validation
    bool isConfigured() const;
    const char* getLastError() const;

    // Testing/Debugging
    void enableDebug(bool enable);
    void printCipherInfo();
};

/**
 * Helper class for secure key storage in NVS
 */
class DTLSKeyStorage
{
public:
    // Store PSK securely in encrypted NVS partition
    static bool storePSK(const unsigned char *key, size_t key_len, const char *identity);
    static bool loadPSK(unsigned char *key, size_t *key_len, char *identity);
    static bool deletePSK();

    // Store certificates
    static bool storeCertificate(const char *name, const unsigned char *cert_pem, size_t cert_len);
    static bool loadCertificate(const char *name, unsigned char *cert_pem, size_t *cert_len);
    static bool deleteCertificate(const char *name);

    // Key rotation history (for gradual migration)
    static bool storePreviousPSK(const unsigned char *key, size_t key_len);
    static bool loadPreviousPSK(unsigned char *key, size_t *key_len);
};
