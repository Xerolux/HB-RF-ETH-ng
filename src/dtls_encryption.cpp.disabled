/*
 *  dtls_encryption.cpp is part of the HB-RF-ETH firmware v2.1
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
 */

#include "dtls_encryption.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <cstring>
#include <cstdio>

static const char *TAG = "DTLS";

// NVS keys for secure storage
static const char *NVS_NAMESPACE = "dtls_keys";
static const char *NVS_PSK_KEY = "psk_key";
static const char *NVS_PSK_IDENTITY = "psk_id";
static const char *NVS_PSK_PREVIOUS = "psk_prev";

DTLSEncryption::DTLSEncryption()
    : psk_key_len(0), mode(DTLS_MODE_DISABLED), cipher_suite(DTLS_CIPHER_AES_256_GCM),
      state(DTLS_STATE_IDLE), session_resumption_enabled(true), require_client_cert(false),
      handshake_timeout_us(10000000), session_valid(false)
{
    memset(&stats, 0, sizeof(stats));
    memset(psk_key, 0, sizeof(psk_key));
    memset(psk_identity, 0, sizeof(psk_identity));
    memset(&saved_session, 0, sizeof(saved_session));

    // Initialize mbedTLS structures
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_x509_crt_init(&server_cert);
    mbedtls_pk_init(&server_key);
    mbedtls_x509_crt_init(&ca_cert);
}

DTLSEncryption::~DTLSEncryption()
{
    deinit();
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

    mode = encryption_mode;
    cipher_suite = cipher;

    // Seed random number generator
    const char *pers = "hb_rf_eth_dtls";
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                     (const unsigned char *)pers, strlen(pers));
    if (ret != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed failed: -0x%04x", -ret);
        return false;
    }

    // Configure SSL/DTLS
    ret = mbedtls_ssl_config_defaults(&conf,
                                      MBEDTLS_SSL_IS_SERVER,
                                      MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults failed: -0x%04x", -ret);
        return false;
    }

    // Set minimum DTLS version to 1.2 for security
    mbedtls_ssl_conf_min_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);

    // Configure cipher suites based on selection
    static const int psk_ciphersuites_aes128[] = {
        MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256,
        0
    };
    static const int psk_ciphersuites_aes256[] = {
        MBEDTLS_TLS_PSK_WITH_AES_256_GCM_SHA384,
        0
    };
    static const int psk_ciphersuites_chacha[] = {
        MBEDTLS_TLS_PSK_WITH_CHACHA20_POLY1305_SHA256,
        0
    };

    switch (cipher_suite)
    {
    case DTLS_CIPHER_AES_128_GCM:
        mbedtls_ssl_conf_ciphersuites(&conf, psk_ciphersuites_aes128);
        ESP_LOGI(TAG, "Using AES-128-GCM cipher suite");
        break;
    case DTLS_CIPHER_AES_256_GCM:
        mbedtls_ssl_conf_ciphersuites(&conf, psk_ciphersuites_aes256);
        ESP_LOGI(TAG, "Using AES-256-GCM cipher suite");
        break;
    case DTLS_CIPHER_CHACHA20_POLY1305:
        mbedtls_ssl_conf_ciphersuites(&conf, psk_ciphersuites_chacha);
        ESP_LOGI(TAG, "Using ChaCha20-Poly1305 cipher suite");
        break;
    }

    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    // Setup based on mode
    if (encryption_mode == DTLS_MODE_PSK)
    {
        if (!setupPSK())
        {
            ESP_LOGE(TAG, "Failed to setup PSK");
            return false;
        }
    }
    else if (encryption_mode == DTLS_MODE_CERTIFICATE)
    {
        if (!setupCertificate())
        {
            ESP_LOGE(TAG, "Failed to setup certificate");
            return false;
        }
    }

    // Setup SSL context
    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_setup failed: -0x%04x", -ret);
        return false;
    }

    // Set BIO callbacks (will be set when connection starts)
    // mbedtls_ssl_set_bio(&ssl, this, bio_send, bio_recv, bio_recv_timeout);

    state = DTLS_STATE_IDLE;
    ESP_LOGI(TAG, "DTLS initialized successfully in %s mode",
             encryption_mode == DTLS_MODE_PSK ? "PSK" : "Certificate");

    return true;
}

void DTLSEncryption::deinit()
{
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mbedtls_x509_crt_free(&server_cert);
    mbedtls_pk_free(&server_key);
    mbedtls_x509_crt_free(&ca_cert);

    memset(psk_key, 0, sizeof(psk_key));
    state = DTLS_STATE_IDLE;
}

int DTLSEncryption::setupPSK()
{
    // Try to load PSK from NVS
    unsigned char key[64];
    size_t key_len;
    char identity[64];

    if (DTLSKeyStorage::loadPSK(key, &key_len, identity))
    {
        ESP_LOGI(TAG, "Loaded PSK from NVS, identity: %s, key length: %d bits", identity, key_len * 8);
        return setPSK(key, key_len, identity) ? 0 : -1;
    }

    ESP_LOGW(TAG, "No PSK found in NVS. Please configure PSK via WebUI or generate one.");
    return -1;
}

int DTLSEncryption::setupCertificate()
{
    // Certificate mode setup
    // Load certificates from NVS or generate self-signed for testing

    ESP_LOGW(TAG, "Certificate mode not fully implemented yet. Use PSK mode.");
    return -1;
}

bool DTLSEncryption::setPSK(const unsigned char *key, size_t key_len, const char *identity)
{
    if (key_len > sizeof(psk_key) || strlen(identity) >= sizeof(psk_identity))
    {
        ESP_LOGE(TAG, "PSK key or identity too long");
        return false;
    }

    memcpy(psk_key, key, key_len);
    psk_key_len = key_len;
    strncpy(psk_identity, identity, sizeof(psk_identity) - 1);

    // Configure mbedTLS to use PSK
    int ret = mbedtls_ssl_conf_psk(&conf, psk_key, psk_key_len,
                                   (const unsigned char *)psk_identity, strlen(psk_identity));
    if (ret != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_conf_psk failed: -0x%04x", -ret);
        return false;
    }

    ESP_LOGI(TAG, "PSK configured: identity='%s', key_length=%d bits", psk_identity, psk_key_len * 8);
    return true;
}

bool DTLSEncryption::generatePSK(size_t key_bits)
{
    if (key_bits != 128 && key_bits != 192 && key_bits != 256 && key_bits != 384 && key_bits != 512)
    {
        ESP_LOGE(TAG, "Invalid PSK key size. Supported: 128, 192, 256, 384, 512 bits");
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

    ESP_LOGI(TAG, "Generated new PSK: %d bits, identity: %s", key_bits, identity);

    return setPSK(new_key, key_bytes, identity);
}

bool DTLSEncryption::getPSK(unsigned char *key_out, size_t *key_len_out, char *identity_out)
{
    if (psk_key_len == 0)
    {
        return false;
    }

    if (key_out)
        memcpy(key_out, psk_key, psk_key_len);
    if (key_len_out)
        *key_len_out = psk_key_len;
    if (identity_out)
        strcpy(identity_out, psk_identity);

    return true;
}

int DTLSEncryption::encrypt(const unsigned char *plaintext, size_t plaintext_len,
                            unsigned char *ciphertext, size_t *ciphertext_len)
{
    if (mode == DTLS_MODE_DISABLED)
    {
        // Pass through unencrypted
        if (*ciphertext_len < plaintext_len)
            return -1;
        memcpy(ciphertext, plaintext, plaintext_len);
        *ciphertext_len = plaintext_len;
        return 0;
    }

    if (state != DTLS_STATE_CONNECTED)
    {
        ESP_LOGW(TAG, "Cannot encrypt: not in connected state");
        return -1;
    }

    int ret = mbedtls_ssl_write(&ssl, plaintext, plaintext_len);
    if (ret < 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_write failed: -0x%04x", -ret);
        stats.encryption_errors++;
        return ret;
    }

    *ciphertext_len = ret;
    stats.packets_encrypted++;
    stats.bytes_encrypted += ret;

    return 0;
}

int DTLSEncryption::decrypt(const unsigned char *ciphertext, size_t ciphertext_len,
                            unsigned char *plaintext, size_t *plaintext_len)
{
    if (mode == DTLS_MODE_DISABLED)
    {
        // Pass through unencrypted
        if (*plaintext_len < ciphertext_len)
            return -1;
        memcpy(plaintext, ciphertext, ciphertext_len);
        *plaintext_len = ciphertext_len;
        return 0;
    }

    if (state != DTLS_STATE_CONNECTED)
    {
        ESP_LOGW(TAG, "Cannot decrypt: not in connected state");
        return -1;
    }

    int ret = mbedtls_ssl_read(&ssl, plaintext, *plaintext_len);
    if (ret < 0)
    {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            return 0; // Non-fatal, need more data
        }
        ESP_LOGE(TAG, "mbedtls_ssl_read failed: -0x%04x", -ret);
        stats.decryption_errors++;
        return ret;
    }

    *plaintext_len = ret;
    stats.packets_decrypted++;
    stats.bytes_decrypted += ret;

    return 0;
}

bool DTLSEncryption::isConfigured() const
{
    if (mode == DTLS_MODE_DISABLED)
        return true;

    if (mode == DTLS_MODE_PSK)
        return psk_key_len > 0;

    if (mode == DTLS_MODE_CERTIFICATE)
        return false; // Not implemented yet

    return false;
}

void DTLSEncryption::resetStats()
{
    memset(&stats, 0, sizeof(stats));
}

void DTLSEncryption::enableDebug(bool enable)
{
    if (enable)
    {
        mbedtls_ssl_conf_dbg(&conf, debug_callback, this);
        mbedtls_debug_set_threshold(3);
    }
    else
    {
        mbedtls_debug_set_threshold(0);
    }
}

void DTLSEncryption::debug_callback(void *ctx, int level, const char *file, int line, const char *str)
{
    ESP_LOGI(TAG, "%s:%d: %s", file, line, str);
}

// ========== DTLSKeyStorage Implementation ==========

bool DTLSKeyStorage::storePSK(const unsigned char *key, size_t key_len, const char *identity)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    // Store PSK key
    err = nvs_set_blob(handle, NVS_PSK_KEY, key, key_len);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to store PSK key: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    // Store PSK identity
    err = nvs_set_str(handle, NVS_PSK_IDENTITY, identity);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to store PSK identity: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "PSK stored successfully in NVS");
        return true;
    }

    ESP_LOGE(TAG, "Failed to commit PSK to NVS: %s", esp_err_to_name(err));
    return false;
}

bool DTLSKeyStorage::loadPSK(unsigned char *key, size_t *key_len, char *identity)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return false; // No keys stored yet
    }

    // Load PSK key
    size_t required_size = 64;
    err = nvs_get_blob(handle, NVS_PSK_KEY, key, &required_size);
    if (err != ESP_OK)
    {
        nvs_close(handle);
        return false;
    }
    *key_len = required_size;

    // Load PSK identity
    required_size = 64;
    err = nvs_get_str(handle, NVS_PSK_IDENTITY, identity, &required_size);
    if (err != ESP_OK)
    {
        nvs_close(handle);
        return false;
    }

    nvs_close(handle);
    return true;
}

bool DTLSKeyStorage::deletePSK()
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return false;

    nvs_erase_key(handle, NVS_PSK_KEY);
    nvs_erase_key(handle, NVS_PSK_IDENTITY);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "PSK deleted from NVS");
    return true;
}

bool DTLSKeyStorage::storePreviousPSK(const unsigned char *key, size_t key_len)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return false;

    err = nvs_set_blob(handle, NVS_PSK_PREVIOUS, key, key_len);
    nvs_commit(handle);
    nvs_close(handle);

    return (err == ESP_OK);
}

bool DTLSKeyStorage::loadPreviousPSK(unsigned char *key, size_t *key_len)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
        return false;

    size_t required_size = 64;
    err = nvs_get_blob(handle, NVS_PSK_PREVIOUS, key, &required_size);
    nvs_close(handle);

    if (err == ESP_OK)
    {
        *key_len = required_size;
        return true;
    }

    return false;
}

bool DTLSKeyStorage::storeCertificate(const char *name, const unsigned char *cert_pem, size_t cert_len)
{
    // TODO: Implement certificate storage
    return false;
}

bool DTLSKeyStorage::loadCertificate(const char *name, unsigned char *cert_pem, size_t *cert_len)
{
    // TODO: Implement certificate loading
    return false;
}

bool DTLSKeyStorage::deleteCertificate(const char *name)
{
    // TODO: Implement certificate deletion
    return false;
}
