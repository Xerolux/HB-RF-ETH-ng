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
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_task_wdt.h"
#include "string.h"
#include "cJSON.h"

static const char *TAG = "UpdateCheck";

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

void UpdateCheck::_updateLatestVersion()
{
  // Always fetch releases list to properly filter by type
  char url[256];
  snprintf(url, sizeof(url), "https://api.github.com/repos/Xerolux/HB-RF-ETH-ng/releases?per_page=10");

  esp_http_client_config_t config = {};
  config.url = url;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.transport_type = HTTP_TRANSPORT_OVER_SSL;
  config.buffer_size = 16384;  // Increased for multiple releases
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
  char *buffer = (char*)malloc(16384);
  if (!buffer) {
      ESP_LOGE(TAG, "Failed to allocate buffer for update check");
      esp_http_client_cleanup(client);
      return;
  }

  int total_read = 0;
  int read_len = 0;

  // Read response in chunks
  while (total_read < 16383 && (read_len = esp_http_client_read(client, buffer + total_read, 16383 - total_read)) > 0) {
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
      if (tag_name[0] == 'v' || tag_name[0] == 'V') {
          tag_name++;
      }

      strncpy(_latestVersion, tag_name, sizeof(_latestVersion) - 1);
      _latestVersion[sizeof(_latestVersion) - 1] = '\0';
      found_release = true;
      ESP_LOGI(TAG, "Set latest version to: %s", _latestVersion);
      break;
  }

  if (!found_release) {
      ESP_LOGW(TAG, "No suitable release found (allow_prerelease=%d)", allow_prerelease);
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

      if (strcmp(_latestVersion, "n/a") != 0 && strcmp(_sysInfo->getCurrentVersion(), _latestVersion) < 0)
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

  vTaskDelete(NULL);
}

void UpdateCheck::performOnlineUpdate()
{
    if (strcmp(_latestVersion, "n/a") == 0) {
        ESP_LOGE(TAG, "No update version available");
        return;
    }

    char url[256];
    // Construct URL: https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v<version>/firmware.bin
    // Note: We assume the tag has 'v' prefix, but version string might not.
    // If _latestVersion is "2.1.1", tag is "v2.1.1".
    snprintf(url, sizeof(url), "https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v%s/firmware.bin", _latestVersion);

    ESP_LOGI(TAG, "Starting OTA update from %s", url);
    _statusLED->setState(LED_STATE_BLINK_FAST);

    // Disable task watchdog for this task during OTA update
    // OTA updates can take several seconds and may trigger watchdog timeout
    esp_task_wdt_delete(xTaskGetCurrentTaskHandle());

    esp_http_client_config_t config = {};
    config.url = url;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.keep_alive_enable = true;

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Update successful, restarting...");
        esp_restart();  // System restart - no need to re-enable watchdog
    } else {
        ESP_LOGE(TAG, "OTA Update failed: %s", esp_err_to_name(ret));
        _statusLED->setState(LED_STATE_ON);
        // Re-enable watchdog for this task
        esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    }
}