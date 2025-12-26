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

void UpdateCheck::_updateLatestVersion()
{
  // Always fetch releases list to properly filter by type
  char url[256];
  snprintf(url, sizeof(url), "https://api.github.com/repos/Xerolux/HB-RF-ETH-ng/releases?per_page=5");

  esp_http_client_config_t config = {};
  config.url = url;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.transport_type = HTTP_TRANSPORT_OVER_SSL;
  config.buffer_size = 4096;  // Internal buffer for chunks
  config.buffer_size_tx = 2048;
  config.timeout_ms = 15000;   // 15 second timeout

  esp_http_client_handle_t client = esp_http_client_init(&config);

  if (!client) {
      ESP_LOGE(TAG, "Failed to initialize HTTP client for update check");
      return;
  }

  // Set required headers for GitHub API
  esp_http_client_set_header(client, "Accept", "application/vnd.github.v3+json");
  esp_http_client_set_header(client, "User-Agent", "HB-RF-ETH-ng");

  esp_err_t err = esp_http_client_perform(client);
  if (err != ESP_OK) {
      ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
      esp_http_client_cleanup(client);
      return;
  }

  int status_code = esp_http_client_get_status_code(client);
  if (status_code != 200) {
      ESP_LOGE(TAG, "GitHub API returned status code: %d", status_code);
      esp_http_client_cleanup(client);
      return;
  }

  int content_length = esp_http_client_get_content_length(client);
  ESP_LOGI(TAG, "GitHub API response: status=%d, content_length=%d", status_code, content_length);

  // Allocate buffer for response
  const int MAX_BUFFER_SIZE = 32768;
  char *buffer = (char*)malloc(MAX_BUFFER_SIZE);
  if (!buffer) {
      ESP_LOGE(TAG, "Failed to allocate buffer for update check");
      esp_http_client_cleanup(client);
      return;
  }

  int total_read = 0;
  int read_len = 0;

  // Read response in chunks
  while (total_read < (MAX_BUFFER_SIZE - 1) && (read_len = esp_http_client_read(client, buffer + total_read, (MAX_BUFFER_SIZE - 1) - total_read)) > 0) {
      total_read += read_len;
  }
  buffer[total_read] = '\0';

  if (total_read <= 0) {
      ESP_LOGE(TAG, "Failed to read response from GitHub API");
      free(buffer);
      esp_http_client_cleanup(client);
      return;
  }

  ESP_LOGI(TAG, "Read %d bytes from GitHub API", total_read);

  // Parse JSON response
  cJSON *json = cJSON_Parse(buffer);
  free(buffer);  // Free buffer after parsing

  if (!json) {
      ESP_LOGE(TAG, "Failed to parse JSON response from GitHub API");
      esp_http_client_cleanup(client);
      return;
  }

  if (!cJSON_IsArray(json)) {
      ESP_LOGE(TAG, "GitHub API response is not an array");
      cJSON_Delete(json);
      esp_http_client_cleanup(client);
      return;
  }

  int array_size = cJSON_GetArraySize(json);
  ESP_LOGI(TAG, "Found %d releases on GitHub", array_size);

  bool allow_prerelease = _settings->getAllowPrerelease();
  bool found_release = false;

  // Reset cached data
  _releaseNotes[0] = '\0';
  _downloadUrl[0] = '\0';

  // Iterate through releases to find the appropriate one
  for (int i = 0; i < array_size; i++) {
      cJSON *release = cJSON_GetArrayItem(json, i);
      if (!release) {
          ESP_LOGW(TAG, "Release at index %d is null", i);
          continue;
      }

      cJSON *draft_obj = cJSON_GetObjectItem(release, "draft");
      cJSON *prerelease_obj = cJSON_GetObjectItem(release, "prerelease");
      cJSON *tag_name_obj = cJSON_GetObjectItem(release, "tag_name");

      if (!tag_name_obj || !cJSON_IsString(tag_name_obj)) {
          ESP_LOGW(TAG, "Release at index %d has no valid tag_name", i);
          continue;
      }

      const char *tag_name = tag_name_obj->valuestring;

      // Check draft status - default to false if not present
      bool is_draft = false;
      if (draft_obj && cJSON_IsBool(draft_obj)) {
          is_draft = cJSON_IsTrue(draft_obj);
      }

      // Check prerelease status - default to false if not present
      bool is_prerelease = false;
      if (prerelease_obj && cJSON_IsBool(prerelease_obj)) {
          is_prerelease = cJSON_IsTrue(prerelease_obj);
      }

      ESP_LOGI(TAG, "Examining release %d: %s (draft=%d, prerelease=%d)", i, tag_name, is_draft, is_prerelease);

      // Skip drafts (they're not meant for public consumption)
      if (is_draft) {
          ESP_LOGI(TAG, "Skipping draft release: %s", tag_name);
          continue;
      }

      // Filter based on prerelease setting
      if (!allow_prerelease && is_prerelease) {
          ESP_LOGI(TAG, "Skipping prerelease: %s (allow_prerelease=%d)", tag_name, allow_prerelease);
          continue;
      }

      // Found a suitable release
      ESP_LOGI(TAG, "Found suitable release: %s (prerelease=%d)", tag_name, is_prerelease);

      // Remove 'v' prefix if present
      const char* version_str = tag_name;
      if ((tag_name[0] == 'v' || tag_name[0] == 'V') && tag_name[1] != '\0') {
          version_str = tag_name + 1;
      }

      strncpy(_latestVersion, version_str, sizeof(_latestVersion) - 1);
      _latestVersion[sizeof(_latestVersion) - 1] = '\0';
      found_release = true;
      ESP_LOGI(TAG, "Set latest version to: %s", _latestVersion);

      // Read release notes (body) if present
      cJSON *body_obj = cJSON_GetObjectItem(release, "body");
      if (body_obj && cJSON_IsString(body_obj) && body_obj->valuestring) {
          strncpy(_releaseNotes, body_obj->valuestring, RELEASE_NOTES_SIZE - 1);
          _releaseNotes[RELEASE_NOTES_SIZE - 1] = '\0';
      } else {
          strncpy(_releaseNotes, FALLBACK_RELEASE_NOTES, RELEASE_NOTES_SIZE - 1);
          _releaseNotes[RELEASE_NOTES_SIZE - 1] = '\0';
      }

      // Find firmware asset download URL
      _downloadUrl[0] = '\0';
      cJSON *assets = cJSON_GetObjectItem(release, "assets");
      if (assets && cJSON_IsArray(assets)) {
          int asset_count = cJSON_GetArraySize(assets);
          for (int j = 0; j < asset_count; j++) {
              cJSON *asset = cJSON_GetArrayItem(assets, j);
              if (!asset) {
                  continue;
              }

              cJSON *name = cJSON_GetObjectItem(asset, "name");
              cJSON *browser_download_url = cJSON_GetObjectItem(asset, "browser_download_url");

              if (name && cJSON_IsString(name) && browser_download_url && cJSON_IsString(browser_download_url)) {
                  // Prefer firmware*.bin
                  if (strstr(name->valuestring, "firmware") != NULL && strstr(name->valuestring, ".bin") != NULL) {
                      strncpy(_downloadUrl, browser_download_url->valuestring, DOWNLOAD_URL_SIZE - 1);
                      _downloadUrl[DOWNLOAD_URL_SIZE - 1] = '\0';
                      ESP_LOGI(TAG, "Found firmware asset URL: %s", _downloadUrl);
                      break;
                  }
              }
          }
      }

      // Fallback: construct default download URL if not found in assets
      if (strlen(_downloadUrl) == 0) {
          snprintf(_downloadUrl, DOWNLOAD_URL_SIZE, "https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v%s/firmware.bin", _latestVersion);
          ESP_LOGW(TAG, "Asset download URL not found, using fallback: %s", _downloadUrl);
      }

      break;
  }

  if (!found_release) {
      ESP_LOGW(TAG, "No suitable release found (allow_prerelease=%d)", allow_prerelease);
      strncpy(_latestVersion, "n/a", sizeof(_latestVersion) - 1);
      _latestVersion[sizeof(_latestVersion) - 1] = '\0';
      strncpy(_releaseNotes, FALLBACK_RELEASE_NOTES, RELEASE_NOTES_SIZE - 1);
      _releaseNotes[RELEASE_NOTES_SIZE - 1] = '\0';
      _downloadUrl[0] = '\0';
  } else {
      ESP_LOGI(TAG, "Latest version: %s", _latestVersion);
  }

  cJSON_Delete(json);
  esp_http_client_cleanup(client);
}

void UpdateCheck::_taskFunc()
{
  // Reduced initial delay from 30s to 10s to improve startup responsiveness
  // Network should be ready by then
  vTaskDelay(10000 / portTICK_PERIOD_MS);

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

    // Set GitHub headers
    esp_http_client_handle_t http_client = esp_http_client_init(&config);
    if (!http_client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for OTA");
        _statusLED->setState(LED_STATE_ON);
        // Re-enable watchdog
        esp_task_wdt_add(current_task);
        return;
    }

    esp_http_client_set_header(http_client, "User-Agent", "HB-RF-ETH-ng");
    esp_http_client_set_header(http_client, "Accept", "application/octet-stream");

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;

    ESP_LOGI(TAG, "Beginning HTTPS OTA update...");
    esp_err_t ret = esp_https_ota(&ota_config);

    esp_http_client_cleanup(http_client);

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
