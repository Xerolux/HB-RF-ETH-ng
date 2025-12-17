/*
 *  dtls_api.cpp is part of the HB-RF-ETH firmware v2.1
 *
 *  Copyright 2025 Xerolux
 *  DTLS API Endpoints Implementation
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 */

#include "dtls_api.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "DTLS_API";

// Global references (set during registration)
static Settings *g_settings = nullptr;
static DTLSEncryption *g_dtls = nullptr;

// Helper: Send JSON response
static esp_err_t send_json_response(httpd_req_t *req, cJSON *json, int status_code = 200)
{
    char *json_str = cJSON_PrintUnformatted(json);
    if (!json_str)
    {
        cJSON_Delete(json);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (status_code != 200)
    {
        char status[16];
        snprintf(status, sizeof(status), "%d", status_code);
        httpd_resp_set_status(req, status);
    }

    esp_err_t ret = httpd_resp_send(req, json_str, strlen(json_str));

    free(json_str);
    cJSON_Delete(json);

    return ret;
}

// GET /api/dtls/status
esp_err_t api_dtls_status_handler(httpd_req_t *req)
{
    if (!g_settings || !g_dtls)
    {
        ESP_LOGE(TAG, "DTLS not initialized");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();

    // Configuration
    cJSON_AddNumberToObject(root, "mode", g_settings->getDTLSMode());
    cJSON_AddNumberToObject(root, "cipherSuite", g_settings->getDTLSCipherSuite());
    cJSON_AddBoolToObject(root, "requireClientCert", g_settings->getDTLSRequireClientCert());
    cJSON_AddBoolToObject(root, "sessionResumption", g_settings->getDTLSSessionResumption());

    // PSK Status
    unsigned char psk_key[64];
    size_t psk_len = 0;
    char psk_identity[64];
    bool psk_configured = g_dtls->getPSK(psk_key, &psk_len, psk_identity);

    cJSON *psk = cJSON_CreateObject();
    cJSON_AddBoolToObject(psk, "configured", psk_configured);
    if (psk_configured)
    {
        cJSON_AddStringToObject(psk, "identity", psk_identity);
        cJSON_AddNumberToObject(psk, "keyLength", psk_len * 8); // in bits
        // NEVER send the actual key!
    }
    cJSON_AddItemToObject(root, "psk", psk);

    // DTLS State
    const char *state_str = "unknown";
    switch (g_dtls->getState())
    {
        case DTLS_STATE_IDLE: state_str = "idle"; break;
        case DTLS_STATE_HANDSHAKING: state_str = "handshaking"; break;
        case DTLS_STATE_CONNECTED: state_str = "connected"; break;
        case DTLS_STATE_ERROR: state_str = "error"; break;
    }
    cJSON_AddStringToObject(root, "state", state_str);

    const char *mode_str = "disabled";
    switch (g_dtls->getMode())
    {
        case DTLS_MODE_DISABLED: mode_str = "disabled"; break;
        case DTLS_MODE_PSK: mode_str = "psk"; break;
        case DTLS_MODE_CERTIFICATE: mode_str = "certificate"; break;
    }
    cJSON_AddStringToObject(root, "modeString", mode_str);

    return send_json_response(req, root);
}

// POST /api/dtls/generate-psk
esp_err_t api_dtls_generate_psk_handler(httpd_req_t *req)
{
    if (!g_settings || !g_dtls)
    {
        ESP_LOGE(TAG, "DTLS not initialized");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Parse request body
    char content[256];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0)
    {
        ESP_LOGE(TAG, "Failed to receive request body");
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (!json)
    {
        ESP_LOGE(TAG, "Invalid JSON");
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }

    // Get key length (default 256)
    cJSON *key_length_item = cJSON_GetObjectItem(json, "keyLength");
    int key_bits = key_length_item ? key_length_item->valueint : 256;
    cJSON_Delete(json);

    // Validate key length
    if (key_bits != 128 && key_bits != 192 && key_bits != 256 &&
        key_bits != 384 && key_bits != 512)
    {
        ESP_LOGE(TAG, "Invalid key length: %d", key_bits);

        cJSON *error = cJSON_CreateObject();
        cJSON_AddStringToObject(error, "error", "Invalid key length");
        cJSON_AddStringToObject(error, "message", "Supported: 128, 192, 256, 384, 512 bits");
        return send_json_response(req, error, 400);
    }

    // Generate PSK
    if (!g_dtls->generatePSK(key_bits))
    {
        ESP_LOGE(TAG, "Failed to generate PSK");

        cJSON *error = cJSON_CreateObject();
        cJSON_AddStringToObject(error, "error", "PSK generation failed");
        return send_json_response(req, error, 500);
    }

    // Get generated PSK
    unsigned char psk_key[64];
    size_t psk_len = 0;
    char psk_identity[64];

    if (!g_dtls->getPSK(psk_key, &psk_len, psk_identity))
    {
        ESP_LOGE(TAG, "Failed to retrieve generated PSK");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Store PSK in NVS
    if (!DTLSKeyStorage::storePSK(psk_key, psk_len, psk_identity))
    {
        ESP_LOGW(TAG, "Failed to store PSK in NVS");
    }

    // Convert PSK to hex string
    char psk_hex[129]; // 64 bytes = 128 hex chars + null
    for (size_t i = 0; i < psk_len; i++)
    {
        snprintf(psk_hex + (i * 2), 3, "%02X", psk_key[i]);
    }
    psk_hex[psk_len * 2] = '\0';

    // Build response
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddStringToObject(root, "identity", psk_identity);
    cJSON_AddStringToObject(root, "key", psk_hex); // ONLY shown ONCE!
    cJSON_AddNumberToObject(root, "keyLength", psk_len * 8);
    cJSON_AddStringToObject(root, "warning", "This key will only be shown once. Copy it now!");

    ESP_LOGI(TAG, "Generated new PSK: identity=%s, length=%d bits", psk_identity, psk_len * 8);

    return send_json_response(req, root);
}

// POST /api/dtls/config
esp_err_t api_dtls_config_handler(httpd_req_t *req)
{
    if (!g_settings || !g_dtls)
    {
        ESP_LOGE(TAG, "DTLS not initialized");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Parse request body
    char content[512];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0)
    {
        ESP_LOGE(TAG, "Failed to receive request body");
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (!json)
    {
        ESP_LOGE(TAG, "Invalid JSON");
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }

    // Extract configuration
    cJSON *mode_item = cJSON_GetObjectItem(json, "mode");
    cJSON *cipher_item = cJSON_GetObjectItem(json, "cipherSuite");
    cJSON *require_cert_item = cJSON_GetObjectItem(json, "requireClientCert");
    cJSON *session_res_item = cJSON_GetObjectItem(json, "sessionResumption");

    int mode = mode_item ? mode_item->valueint : g_settings->getDTLSMode();
    int cipher = cipher_item ? cipher_item->valueint : g_settings->getDTLSCipherSuite();
    bool require_cert = require_cert_item ? cJSON_IsTrue(require_cert_item) : g_settings->getDTLSRequireClientCert();
    bool session_res = session_res_item ? cJSON_IsTrue(session_res_item) : g_settings->getDTLSSessionResumption();

    cJSON_Delete(json);

    // Validate
    if (mode < 0 || mode > 2)
    {
        cJSON *error = cJSON_CreateObject();
        cJSON_AddStringToObject(error, "error", "Invalid mode");
        return send_json_response(req, error, 400);
    }

    if (cipher < 0 || cipher > 2)
    {
        cJSON *error = cJSON_CreateObject();
        cJSON_AddStringToObject(error, "error", "Invalid cipher suite");
        return send_json_response(req, error, 400);
    }

    // Update settings
    g_settings->setDTLSSettings(mode, cipher, require_cert, session_res);
    g_settings->save();

    // Re-initialize DTLS if needed
    if (mode != DTLS_MODE_DISABLED)
    {
        g_dtls->deinit();
        bool init_success = g_dtls->init((dtls_mode_t)mode, (dtls_cipher_suite_t)cipher);

        if (!init_success)
        {
            ESP_LOGE(TAG, "Failed to re-initialize DTLS");

            cJSON *error = cJSON_CreateObject();
            cJSON_AddBoolToObject(error, "success", false);
            cJSON_AddStringToObject(error, "error", "DTLS initialization failed");
            cJSON_AddStringToObject(error, "message", "Check PSK configuration");
            return send_json_response(req, error, 500);
        }

        g_dtls->setSessionResumption(session_res);
        g_dtls->setRequireClientCert(require_cert);
    }

    // Success response
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddStringToObject(root, "message", "DTLS configuration updated. Restart required for changes to take full effect.");
    cJSON_AddNumberToObject(root, "mode", mode);
    cJSON_AddNumberToObject(root, "cipherSuite", cipher);

    ESP_LOGI(TAG, "DTLS config updated: mode=%d, cipher=%d", mode, cipher);

    return send_json_response(req, root);
}

// GET /api/dtls/stats
esp_err_t api_dtls_stats_handler(httpd_req_t *req)
{
    if (!g_settings || !g_dtls)
    {
        ESP_LOGE(TAG, "DTLS not initialized");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    const dtls_stats_t *stats = g_dtls->getStats();
    if (!stats)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "handshakesCompleted", stats->handshakes_completed);
    cJSON_AddNumberToObject(root, "handshakesFailed", stats->handshakes_failed);
    cJSON_AddNumberToObject(root, "packetsEncrypted", stats->packets_encrypted);
    cJSON_AddNumberToObject(root, "packetsDecrypted", stats->packets_decrypted);
    cJSON_AddNumberToObject(root, "encryptionErrors", stats->encryption_errors);
    cJSON_AddNumberToObject(root, "decryptionErrors", stats->decryption_errors);
    cJSON_AddNumberToObject(root, "bytesEncrypted", (double)stats->bytes_encrypted);
    cJSON_AddNumberToObject(root, "bytesDecrypted", (double)stats->bytes_decrypted);
    cJSON_AddNumberToObject(root, "pskAuthFailures", stats->psk_auth_failures);
    cJSON_AddNumberToObject(root, "certAuthFailures", stats->cert_auth_failures);

    return send_json_response(req, root);
}

// Register all DTLS API handlers
void register_dtls_api_handlers(httpd_handle_t server, Settings *settings, DTLSEncryption *dtls)
{
    g_settings = settings;
    g_dtls = dtls;

    httpd_uri_t uri_dtls_status = {
        .uri = "/api/dtls/status",
        .method = HTTP_GET,
        .handler = api_dtls_status_handler,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(server, &uri_dtls_status);

    httpd_uri_t uri_dtls_generate_psk = {
        .uri = "/api/dtls/generate-psk",
        .method = HTTP_POST,
        .handler = api_dtls_generate_psk_handler,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(server, &uri_dtls_generate_psk);

    httpd_uri_t uri_dtls_config = {
        .uri = "/api/dtls/config",
        .method = HTTP_POST,
        .handler = api_dtls_config_handler,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(server, &uri_dtls_config);

    httpd_uri_t uri_dtls_stats = {
        .uri = "/api/dtls/stats",
        .method = HTTP_GET,
        .handler = api_dtls_stats_handler,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(server, &uri_dtls_stats);

    ESP_LOGI(TAG, "DTLS API handlers registered");
}
