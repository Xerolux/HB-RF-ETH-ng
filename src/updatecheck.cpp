/*
 *  updatecheck.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 5.1 and modern toolchains
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

#include "updatecheck.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "spi_flash_mmap.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"
#include "string.h"
#include "cJSON.h"
#include "semver.h"

static const char *TAG = "UpdateCheck";
static const char *FALLBACK_RELEASE_NOTES = "Release notes are not available.";

void _update_check_task_func(void *parameter)
{
  {
    ((UpdateCheck *)parameter)->_taskFunc();
  }
}

UpdateCheck::UpdateCheck(Settings* settings, SysInfo* sysInfo, LED *statusLED) : _sysInfo(sysInfo), _statusLED(statusLED), _settings(settings)
{
}

void UpdateCheck::start()
{
  xTaskCreate(_update_check_task_func, "UpdateCheck", 8192, this, 3, &_tHandle);
}

void UpdateCheck::stop()
{
  vTaskDelete(_tHandle);
}

void UpdateCheck::checkNow()
{
  _updateLatestVersion();
}

const char *UpdateCheck::getLatestVersion()
{
  return _latestVersion;
}

const char* UpdateCheck::getReleaseNotes()
{
  return _releaseNotes;
}

const char* UpdateCheck::getDownloadUrl()
{
  return _downloadUrl;
}

bool UpdateCheck::hasDownloadUrl()
{
  return strlen(_downloadUrl) > 0;
}

// Extract a JSON string value by key from raw JSON text (lightweight, no full parse needed)
static bool _extractJsonString(const char *json, const char *key, char *out, size_t out_size)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos) return false;

    // Skip past key and find the colon + opening quote
    pos += strlen(search);
    while (*pos == ' ' || *pos == ':' || *pos == '\t' || *pos == '\n' || *pos == '\r') pos++;
    if (*pos != '"') return false;
    pos++; // skip opening quote

    size_t i = 0;
    while (*pos && *pos != '"' && i < out_size - 1) {
        if (*pos == '\\' && *(pos + 1)) {
            pos++; // skip escape char
        }
        out[i++] = *pos++;
    }
    out[i] = '\0';
    return i > 0;
}

void UpdateCheck::_updateLatestVersion()
{
  ESP_LOGI(TAG, "Free heap before update check: %lu bytes", esp_get_free_heap_size());

  // Use /releases/latest - tag_name appears in the first ~500 bytes of response
  esp_http_client_config_t config = {};
  config.url = "https://api.github.com/repos/Xerolux/HB-RF-ETH-ng/releases/latest";
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.transport_type = HTTP_TRANSPORT_OVER_SSL;
  config.buffer_size = 1536;
  config.buffer_size_tx = 512;
  config.timeout_ms = 15000;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
      ESP_LOGE(TAG, "Failed to initialize HTTP client");
      return;
  }

  esp_http_client_set_header(client, "Accept", "application/vnd.github.v3+json");
  esp_http_client_set_header(client, "User-Agent", "HB-RF-ETH-ng");

  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
      ESP_LOGE(TAG, "HTTP connection failed: %s", esp_err_to_name(err));
      esp_http_client_cleanup(client);
      return;
  }

  esp_http_client_fetch_headers(client);
  int status_code = esp_http_client_get_status_code(client);

  if (status_code != 200) {
      ESP_LOGE(TAG, "GitHub API returned status code: %d", status_code);
      esp_http_client_close(client);
      esp_http_client_cleanup(client);
      return;
  }

  // Only read first 2KB - tag_name is near the top of the JSON response
  const int BUFFER_SIZE = 2048;
  char *buffer = (char*)malloc(BUFFER_SIZE);
  if (!buffer) {
      ESP_LOGE(TAG, "Failed to allocate %d bytes (free: %lu)", BUFFER_SIZE, esp_get_free_heap_size());
      esp_http_client_close(client);
      esp_http_client_cleanup(client);
      return;
  }

  int total_read = 0;
  int read_len;
  while (total_read < (BUFFER_SIZE - 1)) {
      read_len = esp_http_client_read(client, buffer + total_read, (BUFFER_SIZE - 1) - total_read);
      if (read_len <= 0) break;
      total_read += read_len;
  }
  buffer[total_read] = '\0';

  // Close TLS connection immediately to free ~40KB
  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  if (total_read <= 0) {
      ESP_LOGE(TAG, "Failed to read response from GitHub API");
      free(buffer);
      return;
  }

  ESP_LOGI(TAG, "Read %d bytes from GitHub API (heap: %lu)", total_read, esp_get_free_heap_size());

  // Extract tag_name using lightweight string search (no JSON parser needed)
  _releaseNotes[0] = '\0';
  _downloadUrl[0] = '\0';

  char tag_name[33] = {0};
  if (_extractJsonString(buffer, "tag_name", tag_name, sizeof(tag_name))) {
      ESP_LOGI(TAG, "Latest release: %s", tag_name);

      const char* version_str = tag_name;
      if ((tag_name[0] == 'v' || tag_name[0] == 'V') && tag_name[1] != '\0') {
          version_str = tag_name + 1;
      }

      strncpy(_latestVersion, version_str, sizeof(_latestVersion) - 1);
      _latestVersion[sizeof(_latestVersion) - 1] = '\0';

      // Construct download URL from version and variant
      const char* current_variant = _sysInfo->getFirmwareVariant();
      snprintf(_downloadUrl, DOWNLOAD_URL_SIZE,
               "https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v%s/firmware-%s-v%s.bin",
               _latestVersion, current_variant, _latestVersion);

      strncpy(_releaseNotes, FALLBACK_RELEASE_NOTES, RELEASE_NOTES_SIZE - 1);
      _releaseNotes[RELEASE_NOTES_SIZE - 1] = '\0';

      ESP_LOGI(TAG, "Latest version: %s, download URL: %s", _latestVersion, _downloadUrl);
  } else {
      ESP_LOGW(TAG, "Could not find tag_name in API response");
      strncpy(_latestVersion, "n/a", sizeof(_latestVersion) - 1);
      _latestVersion[sizeof(_latestVersion) - 1] = '\0';
      strncpy(_releaseNotes, FALLBACK_RELEASE_NOTES, RELEASE_NOTES_SIZE - 1);
      _releaseNotes[RELEASE_NOTES_SIZE - 1] = '\0';
      _downloadUrl[0] = '\0';
  }

  free(buffer);
}

void UpdateCheck::_taskFunc()
{
  // Wait 30s before first check to let system stabilize and free heap
  vTaskDelay(30000 / portTICK_PERIOD_MS);

  uint32_t check_count = 0;

  for (;;)
  {
    if (_settings->getCheckUpdates()) {
      ESP_LOGI(TAG, "Start checking for the latest available firmware.");
      _updateLatestVersion();

      if (strcmp(_latestVersion, "n/a") != 0 && compareVersions(_latestVersion, _sysInfo->getCurrentVersion()) > 0)
      {
        ESP_LOGW(TAG, "An updated firmware with version %s is available.", _latestVersion);
        _statusLED->setState(LED_STATE_BLINK_SLOW);
      }
      else
      {
        ESP_LOGI(TAG, "There is no newer firmware available.");
      }
    } else {
        ESP_LOGI(TAG, "Update check is disabled.");
    }

    // Sleep in smaller chunks to allow task yielding
    for (int i = 0; i < 480; i++) {  // 480 * 1min = 8h
      vTaskDelay(60000 / portTICK_PERIOD_MS);

      // Yield every iteration to prevent watchdog timeout
      check_count++;
      if (check_count % 5 == 0) {
        taskYIELD();
      }
    }
  }
}

void UpdateCheck::performOnlineUpdate()
{
    ESP_LOGI(TAG, "performOnlineUpdate called");

    // First check if there's a version available
    if (strcmp(_latestVersion, "n/a") == 0) {
        ESP_LOGE(TAG, "No update version available - cannot proceed with update");
        _statusLED->setState(LED_STATE_ON);
        return;
    }

    ESP_LOGI(TAG, "Latest version available: %s", _latestVersion);

    // Verify we have a download URL
    if (!hasDownloadUrl()) {
        ESP_LOGE(TAG, "No download URL available for online update");
        _statusLED->setState(LED_STATE_ON);
        return;
    }

    char url[DOWNLOAD_URL_SIZE];
    strncpy(url, _downloadUrl, sizeof(url) - 1);
    url[sizeof(url) - 1] = '\0';

    ESP_LOGI(TAG, "Starting OTA update from: %s", url);
    _statusLED->setState(LED_STATE_BLINK_FAST);

    // Disable task watchdog for this task during OTA update
    // OTA updates can take several seconds and may trigger watchdog timeout
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    esp_err_t wdt_err = esp_task_wdt_delete(current_task);
    if (wdt_err != ESP_OK && wdt_err != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to remove task from watchdog: %s (continuing anyway)", esp_err_to_name(wdt_err));
    } else {
        ESP_LOGI(TAG, "Task watchdog disabled for OTA update");
    }

    // Configure HTTP client with proper timeouts
    esp_http_client_config_t config = {};
    config.url = url;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.keep_alive_enable = true;
    config.timeout_ms = 30000;  // 30 second timeout for OTA operations
    config.buffer_size = 4096;
    config.buffer_size_tx = 2048;
    config.user_agent = "HB-RF-ETH-ng";

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;

    ESP_LOGI(TAG, "Beginning HTTPS OTA update...");
    esp_err_t ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Update successful! Restarting in 2 seconds...");
        _statusLED->setState(LED_STATE_OFF);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        esp_restart();  // System restart - no need to re-enable watchdog
    } else {
        ESP_LOGE(TAG, "OTA Update failed with error: %s (0x%x)", esp_err_to_name(ret), ret);

        // Provide more detailed error information
        switch(ret) {
            case ESP_ERR_INVALID_ARG:
                ESP_LOGE(TAG, "Invalid argument - check URL format");
                break;
            case ESP_ERR_OTA_VALIDATE_FAILED:
                ESP_LOGE(TAG, "OTA validation failed - firmware image invalid");
                break;
            case ESP_ERR_NO_MEM:
                ESP_LOGE(TAG, "Insufficient memory for OTA update");
                break;
            case ESP_ERR_FLASH_OP_TIMEOUT:
            case ESP_ERR_FLASH_OP_FAIL:
                ESP_LOGE(TAG, "Flash operation failed");
                break;
            case ESP_FAIL:
                ESP_LOGE(TAG, "Generic failure - check network connection and URL");
                break;
            default:
                ESP_LOGE(TAG, "Unknown error occurred during OTA");
                break;
        }

        _statusLED->setState(LED_STATE_ON);

        // Re-enable watchdog for this task
        esp_err_t wdt_add_err = esp_task_wdt_add(current_task);
        if (wdt_add_err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to re-add task to watchdog: %s", esp_err_to_name(wdt_add_err));
        }
    }
}
