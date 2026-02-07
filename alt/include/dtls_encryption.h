/*
 *  dtls_encryption.h is part of the HB-RF-ETH firmware v2.1
 *
 *  Copyright 2025 Xerolux
 *  DTLS/TLS Encryption Layer for Raw-UART UDP Protocol
 *  Ported to ESP-IDF 5.x / mbedTLS 3.x API
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "esp_err.h"

// DTLS modes
typedef enum {
    DTLS_MODE_DISABLED = 0,
    DTLS_MODE_PSK = 1,
    DTLS_MODE_CERTIFICATE = 2  // Not yet implemented
} dtls_mode_t;

// Cipher suites
typedef enum {
    DTLS_CIPHER_AES_128_GCM = 0,
    DTLS_CIPHER_AES_256_GCM = 1,
    DTLS_CIPHER_CHACHA20_POLY1305 = 2
} dtls_cipher_suite_t;

// DTLS connection state
typedef enum {
    DTLS_STATE_IDLE = 0,
    DTLS_STATE_HANDSHAKING = 1,
    DTLS_STATE_CONNECTED = 2,
    DTLS_STATE_ERROR = 3
} dtls_state_t;

// Statistics
typedef struct {
    uint32_t encrypted_packets;
    uint32_t decrypted_packets;
    uint32_t encryption_errors;
    uint32_t decryption_errors;
    uint32_t handshake_successes;
    uint32_t handshake_failures;
    uint32_t active_sessions;
    uint32_t session_cache_evictions;
} dtls_stats_t;

#define DTLS_MAX_CACHED_SESSIONS 10

/**
 * Simplified DTLS Encryption Class for ESP-IDF 5.x
 *
 * Note: This is a simplified implementation focused on PSK mode.
 * Full DTLS handshake is complex - this provides basic encryption wrapper.
 */
class DTLSEncryption
{
private:
    // PSK credentials
    unsigned char psk_key[64];
    size_t psk_key_len;
    char psk_identity[64];

    // Configuration
    dtls_mode_t mode;
    dtls_cipher_suite_t cipher_suite;
    dtls_state_t state;

    // Options
    bool session_resumption_enabled;
    bool require_client_cert;

    // Statistics
    dtls_stats_t stats;

public:
    DTLSEncryption();
    ~DTLSEncryption();

    /**
     * Initialize DTLS with specified mode and cipher
     */
    bool init(dtls_mode_t encryption_mode, dtls_cipher_suite_t cipher);

    /**
     * Set Pre-Shared Key credentials
     * @param key PSK key bytes
     * @param key_len Length of key (16-64 bytes)
     * @param identity PSK identity string
     */
    bool setPSK(const unsigned char *key, size_t key_len, const char *identity);

    /**
     * Generate a new random PSK
     * @param key_bits Key size in bits (128, 192, 256, 384, 512)
     */
    bool generatePSK(size_t key_bits = 256);

    /**
     * Get current PSK (for displaying to user ONCE)
     * @param key_out Buffer for key (min 64 bytes)
     * @param key_len_out Output: actual key length
     * @param identity_out Buffer for identity (min 64 bytes)
     */
    bool getPSK(unsigned char *key_out, size_t *key_len_out, char *identity_out);

    /**
     * Enable/disable session resumption
     */
    void setSessionResumption(bool enabled);

    /**
     * Require client certificate (for cert mode)
     */
    void setRequireClientCert(bool required);

    /**
     * Get current state
     */
    dtls_state_t getState() const { return state; }

    /**
     * Get statistics
     */
    const dtls_stats_t& getStats() const { return stats; }

    /**
     * Reset statistics
     */
    void resetStats();

    /**
     * Check if DTLS is enabled and ready
     */
    bool isEnabled() const { return mode != DTLS_MODE_DISABLED; }

    /**
     * Simple encrypt wrapper (uses AES-GCM directly, not full DTLS handshake)
     * For production use, implement full DTLS handshake state machine
     */
    int encrypt(const unsigned char *plaintext, size_t plaintext_len,
                unsigned char *ciphertext, size_t ciphertext_size,
                size_t *ciphertext_len);

    /**
     * Simple decrypt wrapper (uses AES-GCM directly, not full DTLS handshake)
     * For production use, implement full DTLS handshake state machine
     */
    int decrypt(const unsigned char *ciphertext, size_t ciphertext_len,
                unsigned char *plaintext, size_t plaintext_size,
                size_t *plaintext_len);
};

/**
 * DTLS Key Storage in NVS (Non-Volatile Storage)
 */
class DTLSKeyStorage
{
public:
    /**
     * Store PSK in NVS
     */
    static esp_err_t storePSK(const unsigned char *key, size_t key_len, const char *identity);

    /**
     * Load PSK from NVS
     */
    static esp_err_t loadPSK(unsigned char *key, size_t *key_len, char *identity);

    /**
     * Delete PSK from NVS
     */
    static esp_err_t deletePSK();

    /**
     * Check if PSK exists in NVS
     */
    static bool hasPSK();
};
