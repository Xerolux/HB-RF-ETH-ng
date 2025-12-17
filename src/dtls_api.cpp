/*
 *  dtls_api.cpp is part of the HB-RF-ETH firmware v2.1
 *
 *  Copyright 2025 Xerolux
 *  DTLS API Handlers for WebUI
 *  Ported to ESP-IDF 5.x
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *
 */

#include "dtls_api.h"
#include "esp_log.h"
#include "cJSON.h"
#include <cstring>

static const char *TAG = "DTLS_API";

// Global pointers (set by register function)
static Settings *g_settings = nullptr;
static DTLSEncryption *g_dtls = nullptr;

// Forward declarations
static esp_err_t api_dtls_status_handler(httpd_req_t *req);
static esp_err_t api_dtls_generate_psk_handler(httpd_req_t *req);
static esp_err_t api_dtls_config_handler(httpd_req_t *req);
static esp_err_t api_dtls_stats_handler(httpd_req_t *req);

// Helper: Convert bytes to hex string
static void bytes_to_hex(const unsigned char *bytes, size_t len, char *hex_out)
{
    for (size_t i = 0; i < len; i++)
    {
        sprintf(hex_out + (i * 2), "%02X", bytes[i]);
    }
    hex_out[len * 2] = '\0';
}

// ============================================================================
// GET /api/dtls/status
// ============================================================================
static esp_err_t api_dtls_status_handler(httpd_req_t *req)
{
    if (!g_settings || !g_dtls)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "DTLS not initialized");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();

    // Configuration
    cJSON_AddNumberToObject(root, "mode", g_settings->getDTLSMode());
    cJSON_AddNumberToObject(root, "cipherSuite", g_settings->getDTLSCipherSuite());
    cJSON_AddBoolToObject(root, "requireClientCert", g_settings->getDTLSRequireClientCert());
    cJSON_AddBoolToObject(root, "sessionResumption", g_settings->getDTLSSessionResumption());

    // Status
    cJSON_AddBoolToObject(root, "enabled", g_dtls->isEnabled());
    cJSON_AddNumberToObject(root, "state", g_dtls->getState());
    cJSON_AddBoolToObject(root, "hasPSK", DTLSKeyStorage::hasPSK());

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);

    return ESP_OK;
}

// ============================================================================
// POST /api/dtls/generate-psk
// ============================================================================
static esp_err_t api_dtls_generate_psk_handler(httpd_req_t *req)
{
    if (!g_settings || !g_dtls)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "DTLS not initialized");
        return ESP_FAIL;
    }

    // Parse request body
    char content[256];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive request body");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (!json)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Get key length (default 256 bits)
    cJSON *key_length_item = cJSON_GetObjectItem(json, "keyLength");
    int key_bits = key_length_item ? key_length_item->valueint : 256;
    cJSON_Delete(json);

    // Validate key length
    if (key_bits != 128 && key_bits != 192 && key_bits != 256 && key_bits != 384 && key_bits != 512)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid key length");
        return ESP_FAIL;
    }

    // Generate PSK
    if (!g_dtls->generatePSK(key_bits))
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "PSK generation failed");
        return ESP_FAIL;
    }

    // Get the generated PSK (only shown ONCE!)
    unsigned char psk_key[64];
    size_t psk_len;
    char psk_identity[64];

    if (!g_dtls->getPSK(psk_key, &psk_len, psk_identity))
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get PSK");
        return ESP_FAIL;
    }

    // Convert to hex for transmission
    char psk_hex[129];
    bytes_to_hex(psk_key, psk_len, psk_hex);

    // Store in NVS
    esp_err_t err = DTLSKeyStorage::storePSK(psk_key, psk_len, psk_identity);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to store PSK in NVS: %s", esp_err_to_name(err));
    }

    // Build response
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "psk", psk_hex);
    cJSON_AddStringToObject(root, "identity", psk_identity);
    cJSON_AddNumberToObject(root, "keyLength", key_bits);
    cJSON_AddStringToObject(root, "warning", "This PSK is shown only once! Save it securely.");

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);

    ESP_LOGI(TAG, "New PSK generated and sent to client");

    return ESP_OK;
}

// ============================================================================
// POST /api/dtls/config
// ============================================================================
static esp_err_t api_dtls_config_handler(httpd_req_t *req)
{
    if (!g_settings || !g_dtls)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "DTLS not initialized");
        return ESP_FAIL;
    }

    // Parse request body
    char content[512];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive request body");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (!json)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Extract configuration
    cJSON *mode_item = cJSON_GetObjectItem(json, "mode");
    cJSON *cipher_item = cJSON_GetObjectItem(json, "cipherSuite");
    cJSON *require_cert_item = cJSON_GetObjectItem(json, "requireClientCert");
    cJSON *session_resumption_item = cJSON_GetObjectItem(json, "sessionResumption");

    int mode = mode_item ? mode_item->valueint : g_settings->getDTLSMode();
    int cipher = cipher_item ? cipher_item->valueint : g_settings->getDTLSCipherSuite();
    bool require_cert = cJSON_IsTrue(require_cert_item);
    bool session_resumption = session_resumption_item ? cJSON_IsTrue(session_resumption_item) : true;

    cJSON_Delete(json);

    // Validate
    if (mode < 0 || mode > 2)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid mode");
        return ESP_FAIL;
    }

    if (cipher < 0 || cipher > 2)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid cipher suite");
        return ESP_FAIL;
    }

    // Update settings
    g_settings->setDTLSSettings(mode, cipher, require_cert, session_resumption);
    g_settings->save();

    // Update runtime configuration
    g_dtls->setSessionResumption(session_resumption);
    g_dtls->setRequireClientCert(require_cert);

    // Build response
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddStringToObject(root, "message", "DTLS configuration updated. Restart required for mode/cipher changes.");

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);

    return ESP_OK;
}

// ============================================================================
// GET /api/dtls/stats
// ============================================================================
static esp_err_t api_dtls_stats_handler(httpd_req_t *req)
{
    if (!g_settings || !g_dtls)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "DTLS not initialized");
        return ESP_FAIL;
    }

    const dtls_stats_t &stats = g_dtls->getStats();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "encryptedPackets", stats.encrypted_packets);
    cJSON_AddNumberToObject(root, "decryptedPackets", stats.decrypted_packets);
    cJSON_AddNumberToObject(root, "encryptionErrors", stats.encryption_errors);
    cJSON_AddNumberToObject(root, "decryptionErrors", stats.decryption_errors);
    cJSON_AddNumberToObject(root, "handshakeSuccesses", stats.handshake_successes);
    cJSON_AddNumberToObject(root, "handshakeFailures", stats.handshake_failures);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);

    return ESP_OK;
}

// ============================================================================
// Register handlers
// ============================================================================
void register_dtls_api_handlers(httpd_handle_t server, Settings *settings, DTLSEncryption *dtls)
{
    g_settings = settings;
    g_dtls = dtls;

    httpd_uri_t uri_dtls_status = {
        .uri = "/api/dtls/status",
        .method = HTTP_GET,
        .handler = api_dtls_status_handler,
        .user_ctx = nullptr,
        .is_websocket = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = nullptr
    };
    httpd_register_uri_handler(server, &uri_dtls_status);

    httpd_uri_t uri_dtls_generate_psk = {
        .uri = "/api/dtls/generate-psk",
        .method = HTTP_POST,
        .handler = api_dtls_generate_psk_handler,
        .user_ctx = nullptr,
        .is_websocket = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = nullptr
    };
    httpd_register_uri_handler(server, &uri_dtls_generate_psk);

    httpd_uri_t uri_dtls_config = {
        .uri = "/api/dtls/config",
        .method = HTTP_POST,
        .handler = api_dtls_config_handler,
        .user_ctx = nullptr,
        .is_websocket = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = nullptr
    };
    httpd_register_uri_handler(server, &uri_dtls_config);

    httpd_uri_t uri_dtls_stats = {
        .uri = "/api/dtls/stats",
        .method = HTTP_GET,
        .handler = api_dtls_stats_handler,
        .user_ctx = nullptr,
        .is_websocket = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = nullptr
    };
    httpd_register_uri_handler(server, &uri_dtls_stats);

    ESP_LOGI(TAG, "DTLS API handlers registered");
}
