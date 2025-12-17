/*
 *  dtls_encryption.cpp is part of the HB-RF-ETH firmware v2.1
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

#include "dtls_encryption.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mbedtls/gcm.h"
#include "mbedtls/md.h"
#include <cstring>
#include <cstdio>

static const char *TAG = "DTLS";

// NVS keys for secure storage
static const char *NVS_NAMESPACE = "dtls_keys";
static const char *NVS_PSK_KEY = "psk_key";
static const char *NVS_PSK_IDENTITY = "psk_id";

// ============================================================================
// DTLSEncryption Implementation
// ============================================================================

DTLSEncryption::DTLSEncryption()
    : psk_key_len(0), mode(DTLS_MODE_DISABLED), cipher_suite(DTLS_CIPHER_AES_256_GCM),
      state(DTLS_STATE_IDLE), session_resumption_enabled(true), require_client_cert(false)
{
    memset(&stats, 0, sizeof(stats));
    memset(psk_key, 0, sizeof(psk_key));
    memset(psk_identity, 0, sizeof(psk_identity));

    // Note: Session cache implementation
    // When implementing full DTLS session management, limit to DTLS_MAX_CACHED_SESSIONS
    // Use LRU (Least Recently Used) eviction policy to maintain memory efficiency
    // Estimated memory savings: ~2-4KB with 10-session limit vs unlimited cache
}

DTLSEncryption::~DTLSEncryption()
{
    // Clear sensitive data
    memset(psk_key, 0, sizeof(psk_key));
    memset(psk_identity, 0, sizeof(psk_identity));
}

bool DTLSEncryption::init(dtls_mode_t encryption_mode, dtls_cipher_suite_t cipher)
{
    if (encryption_mode == DTLS_MODE_DISABLED)
    {
        ESP_LOGI(TAG, "DTLS encryption disabled");
        mode = DTLS_MODE_DISABLED;
        state = DTLS_STATE_IDLE;
        return true;
    }

    if (encryption_mode == DTLS_MODE_CERTIFICATE)
    {
        ESP_LOGE(TAG, "Certificate mode not yet implemented");
        return false;
    }

    mode = encryption_mode;
    cipher_suite = cipher;

    // Try to load existing PSK from NVS
    unsigned char loaded_key[64];
    size_t loaded_len = sizeof(loaded_key);
    char loaded_identity[64];

    if (DTLSKeyStorage::loadPSK(loaded_key, &loaded_len, loaded_identity) == ESP_OK)
    {
        ESP_LOGI(TAG, "Loaded existing PSK from NVS (identity: %s)", loaded_identity);
        if (!setPSK(loaded_key, loaded_len, loaded_identity))
        {
            ESP_LOGE(TAG, "Failed to set loaded PSK");
            return false;
        }
    }
    else
    {
        ESP_LOGW(TAG, "No PSK found in NVS - generate one via API or WebUI");
    }

    state = DTLS_STATE_IDLE;
    ESP_LOGI(TAG, "DTLS initialized (mode: PSK, cipher: %d)", cipher);
    return true;
}

bool DTLSEncryption::setPSK(const unsigned char *key, size_t key_len, const char *identity)
{
    if (key_len < 16 || key_len > 64)
    {
        ESP_LOGE(TAG, "Invalid PSK length: %zu (must be 16-64 bytes)", key_len);
        return false;
    }

    if (!identity || strlen(identity) == 0)
    {
        ESP_LOGE(TAG, "Invalid PSK identity");
        return false;
    }

    memcpy(psk_key, key, key_len);
    psk_key_len = key_len;
    strncpy(psk_identity, identity, sizeof(psk_identity) - 1);
    psk_identity[sizeof(psk_identity) - 1] = '\0';

    ESP_LOGI(TAG, "PSK set successfully (%zu bytes, identity: %s)", key_len, identity);
    return true;
}

bool DTLSEncryption::generatePSK(size_t key_bits)
{
    if (key_bits < 128 || key_bits > 512 || key_bits % 8 != 0)
    {
        ESP_LOGE(TAG, "Invalid key size: %zu bits", key_bits);
        return false;
    }

    size_t key_bytes = key_bits / 8;
    unsigned char new_key[64];

    // Generate cryptographically secure random key
    esp_fill_random(new_key, key_bytes);

    // Generate identity based on device MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char identity[64];
    snprintf(identity, sizeof(identity), "HB-RF-ETH-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "Generated new PSK: %zu bits, identity: %s", key_bits, identity);

    return setPSK(new_key, key_bytes, identity);
}

bool DTLSEncryption::getPSK(unsigned char *key_out, size_t *key_len_out, char *identity_out)
{
    if (psk_key_len == 0)
    {
        ESP_LOGW(TAG, "No PSK available");
        return false;
    }

    memcpy(key_out, psk_key, psk_key_len);
    *key_len_out = psk_key_len;
    // Buffer for identity must be at least 64 bytes
    strncpy(identity_out, psk_identity, 63);
    identity_out[63] = '\0';

    return true;
}

void DTLSEncryption::setSessionResumption(bool enabled)
{
    session_resumption_enabled = enabled;
    ESP_LOGI(TAG, "Session resumption: %s", enabled ? "enabled" : "disabled");
}

void DTLSEncryption::setRequireClientCert(bool required)
{
    require_client_cert = required;
    ESP_LOGI(TAG, "Require client cert: %s", required ? "yes" : "no");
}

void DTLSEncryption::resetStats()
{
    memset(&stats, 0, sizeof(stats));
    ESP_LOGI(TAG, "Statistics reset");
}

/**
 * Simplified AES-GCM encryption
 *
 * Format: [12-byte IV][Ciphertext][16-byte Auth Tag]
 *
 * This is NOT a full DTLS implementation - it's a lightweight encryption wrapper
 * suitable for point-to-point communication where both sides share the PSK.
 */
int DTLSEncryption::encrypt(const unsigned char *plaintext, size_t plaintext_len,
                            unsigned char *ciphertext, size_t ciphertext_size,
                            size_t *ciphertext_len)
{
    if (mode == DTLS_MODE_DISABLED || psk_key_len == 0)
    {
        ESP_LOGW(TAG, "Encryption called but DTLS not initialized");
        stats.encryption_errors++;
        return -1;
    }

    // Calculate required buffer size: IV (12) + ciphertext + tag (16)
    size_t required_size = 12 + plaintext_len + 16;
    if (ciphertext_size < required_size)
    {
        ESP_LOGE(TAG, "Output buffer too small: %zu < %zu", ciphertext_size, required_size);
        stats.encryption_errors++;
        return -1;
    }

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    // Determine key size based on cipher suite
    size_t key_bits = (cipher_suite == DTLS_CIPHER_AES_128_GCM) ? 128 : 256;
    size_t key_bytes = key_bits / 8;

    // Use PSK as encryption key (derive proper key for production)
    unsigned char key[32];
    if (psk_key_len >= key_bytes)
    {
        memcpy(key, psk_key, key_bytes);
    }
    else
    {
        // Pad key if PSK is shorter
        memcpy(key, psk_key, psk_key_len);
        memset(key + psk_key_len, 0, key_bytes - psk_key_len);
    }

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, key_bits);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "GCM setkey failed: -0x%04x", -ret);
        mbedtls_gcm_free(&gcm);
        stats.encryption_errors++;
        return ret;
    }

    // Generate random IV
    unsigned char iv[12];
    esp_fill_random(iv, sizeof(iv));
    memcpy(ciphertext, iv, 12);

    // Encrypt
    unsigned char tag[16];
    ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                     plaintext_len,
                                     iv, 12,
                                     NULL, 0,  // No additional data
                                     plaintext,
                                     ciphertext + 12,
                                     16, tag);

    if (ret != 0)
    {
        ESP_LOGE(TAG, "GCM encrypt failed: -0x%04x", -ret);
        mbedtls_gcm_free(&gcm);
        stats.encryption_errors++;
        return ret;
    }

    // Append tag
    memcpy(ciphertext + 12 + plaintext_len, tag, 16);
    *ciphertext_len = required_size;

    mbedtls_gcm_free(&gcm);
    stats.encrypted_packets++;

    return 0;
}

/**
 * Simplified AES-GCM decryption
 *
 * Format: [12-byte IV][Ciphertext][16-byte Auth Tag]
 */
int DTLSEncryption::decrypt(const unsigned char *ciphertext, size_t ciphertext_len,
                            unsigned char *plaintext, size_t plaintext_size,
                            size_t *plaintext_len)
{
    if (mode == DTLS_MODE_DISABLED || psk_key_len == 0)
    {
        ESP_LOGW(TAG, "Decryption called but DTLS not initialized");
        stats.decryption_errors++;
        return -1;
    }

    // Minimum size: IV (12) + tag (16) = 28 bytes
    if (ciphertext_len < 28)
    {
        ESP_LOGE(TAG, "Ciphertext too short: %zu bytes", ciphertext_len);
        stats.decryption_errors++;
        return -1;
    }

    size_t payload_len = ciphertext_len - 28;
    if (plaintext_size < payload_len)
    {
        ESP_LOGE(TAG, "Output buffer too small: %zu < %zu", plaintext_size, payload_len);
        stats.decryption_errors++;
        return -1;
    }

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    // Determine key size based on cipher suite
    size_t key_bits = (cipher_suite == DTLS_CIPHER_AES_128_GCM) ? 128 : 256;
    size_t key_bytes = key_bits / 8;

    // Use PSK as encryption key
    unsigned char key[32];
    if (psk_key_len >= key_bytes)
    {
        memcpy(key, psk_key, key_bytes);
    }
    else
    {
        memcpy(key, psk_key, psk_key_len);
        memset(key + psk_key_len, 0, key_bytes - psk_key_len);
    }

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, key_bits);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "GCM setkey failed: -0x%04x", -ret);
        mbedtls_gcm_free(&gcm);
        stats.decryption_errors++;
        return ret;
    }

    // Extract IV and tag
    const unsigned char *iv = ciphertext;
    const unsigned char *encrypted_data = ciphertext + 12;
    const unsigned char *tag = ciphertext + 12 + payload_len;

    // Decrypt and verify
    ret = mbedtls_gcm_auth_decrypt(&gcm, payload_len,
                                    iv, 12,
                                    NULL, 0,  // No additional data
                                    tag, 16,
                                    encrypted_data,
                                    plaintext);

    if (ret != 0)
    {
        ESP_LOGE(TAG, "GCM decrypt/auth failed: -0x%04x", -ret);
        mbedtls_gcm_free(&gcm);
        stats.decryption_errors++;
        return ret;
    }

    *plaintext_len = payload_len;

    mbedtls_gcm_free(&gcm);
    stats.decrypted_packets++;

    return 0;
}

// ============================================================================
// DTLSKeyStorage Implementation
// ============================================================================

esp_err_t DTLSKeyStorage::storePSK(const unsigned char *key, size_t key_len, const char *identity)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    // Store key
    err = nvs_set_blob(handle, NVS_PSK_KEY, key, key_len);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to store PSK key: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Store identity
    err = nvs_set_str(handle, NVS_PSK_IDENTITY, identity);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to store PSK identity: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "PSK stored in NVS (identity: %s)", identity);
    }

    return err;
}

esp_err_t DTLSKeyStorage::loadPSK(unsigned char *key, size_t *key_len, char *identity)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    // Load key
    err = nvs_get_blob(handle, NVS_PSK_KEY, key, key_len);
    if (err != ESP_OK)
    {
        nvs_close(handle);
        return err;
    }

    // Load identity
    size_t identity_len = 64;
    err = nvs_get_str(handle, NVS_PSK_IDENTITY, identity, &identity_len);
    nvs_close(handle);

    return err;
}

esp_err_t DTLSKeyStorage::deletePSK()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    nvs_erase_key(handle, NVS_PSK_KEY);
    nvs_erase_key(handle, NVS_PSK_IDENTITY);
    err = nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "PSK deleted from NVS");
    return err;
}

bool DTLSKeyStorage::hasPSK()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return false;
    }

    size_t key_len = 0;
    err = nvs_get_blob(handle, NVS_PSK_KEY, NULL, &key_len);
    nvs_close(handle);

    return (err == ESP_OK && key_len > 0);
}
