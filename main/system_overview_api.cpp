#include "system_overview_api.h"

#include <stdlib.h>

#include "cJSON.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"

#include "security_headers.h"
#include "webui_storage.h"

namespace
{
constexpr const char *TAG = "SystemOverview";

extern esp_err_t validate_auth(httpd_req_t *req);

esp_err_t get_system_overview(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, nullptr);
    }

    esp_chip_info_t chip = {};
    esp_chip_info(&chip);

    uint32_t flash_size = 0;
    const esp_err_t flash_result = esp_flash_get_size(nullptr, &flash_size);
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *next_update = esp_ota_get_next_update_partition(nullptr);
    const WebUIStorageStatus webui = webui_storage_get_status();

    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }

    cJSON_AddStringToObject(root, "idfVersion", esp_get_idf_version());
    cJSON_AddStringToObject(root, "target", CONFIG_IDF_TARGET);
    cJSON_AddNumberToObject(root, "chipModel", static_cast<int>(chip.model));
    cJSON_AddNumberToObject(root, "chipRevision", chip.revision);
    cJSON_AddNumberToObject(root, "chipCores", chip.cores);
    cJSON_AddNumberToObject(root, "chipFeatures", chip.features);
    cJSON_AddNumberToObject(root, "resetReason", static_cast<int>(esp_reset_reason()));

    cJSON_AddNumberToObject(root, "flashBytes",
                            flash_result == ESP_OK ? flash_size : 0);
    cJSON_AddNumberToObject(root, "freeHeap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "minimumFreeHeap",
                            esp_get_minimum_free_heap_size());
    cJSON_AddNumberToObject(root, "largestFreeBlock",
                            heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    cJSON_AddNumberToObject(root, "freeInternalHeap",
                            heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    cJSON_AddStringToObject(root, "runningPartition",
                            running ? running->label : "unknown");
    cJSON_AddNumberToObject(root, "runningPartitionAddress",
                            running ? running->address : 0);
    cJSON_AddNumberToObject(root, "runningPartitionSize",
                            running ? running->size : 0);
    cJSON_AddStringToObject(root, "nextUpdatePartition",
                            next_update ? next_update->label : "unknown");
    cJSON_AddNumberToObject(root, "nextUpdatePartitionSize",
                            next_update ? next_update->size : 0);

    cJSON *webui_object = cJSON_AddObjectToObject(root, "webui");
    if (webui_object)
    {
        cJSON_AddStringToObject(webui_object, "source",
                                webui.valid ? "spiffs" : "embedded");
        cJSON_AddStringToObject(webui_object, "version",
                                webui.version[0] ? webui.version : "embedded");
        cJSON_AddBoolToObject(webui_object, "valid", webui.valid);
        cJSON_AddBoolToObject(webui_object, "mounted", webui.mounted);
        cJSON_AddNumberToObject(webui_object, "partitionBytes",
                                webui.partitionSize);
        cJSON_AddNumberToObject(webui_object, "usedBytes", webui.usedBytes);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "JSON allocation failed");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control",
                       "no-store, no-cache, must-revalidate, max-age=0");
    const esp_err_t result = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return result;
}

httpd_uri_t system_overview_uri = {
    .uri = "/api/system/overview",
    .method = HTTP_GET,
    .handler = get_system_overview,
    .user_ctx = nullptr,
};
} // namespace

esp_err_t system_overview_api_register(httpd_handle_t server)
{
    if (!server) return ESP_ERR_INVALID_ARG;
    const esp_err_t result = httpd_register_uri_handler(server,
                                                         &system_overview_uri);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not register system overview API: %s",
                 esp_err_to_name(result));
    }
    return result;
}
