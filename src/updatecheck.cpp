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
  char url[256];
  if (_settings->getAllowPrerelease()) {
      // Get the latest release (which can be a prerelease) by listing 1 per page
      snprintf(url, sizeof(url), "https://api.github.com/repos/Xerolux/HB-RF-ETH-ng/releases?per_page=1");
  } else {
      // Get the latest stable release
      snprintf(url, sizeof(url), "https://api.github.com/repos/Xerolux/HB-RF-ETH-ng/releases/latest");
  }

  esp_http_client_config_t config = {};
  config.url = url;
  config.crt_bundle_attach = esp_crt_bundle_attach; // Important for GitHub API
  config.transport_type = HTTP_TRANSPORT_OVER_SSL;
  // Increased buffer sizes for larger API responses
  config.buffer_size = 8192;
  config.buffer_size_tx = 2048;
  // Add timeout to prevent hanging
  config.timeout_ms = 10000;  // 10 second timeout

  esp_http_client_handle_t client = esp_http_client_init(&config);

  if (!client) {
      ESP_LOGE(TAG, "Failed to initialize HTTP client");
      return;
  }

  // Set headers
  esp_http_client_set_header(client, "Accept", "application/vnd.github.v3+json");
  esp_http_client_set_header(client, "User-Agent", "HB-RF-ETH-ng");

  esp_err_t err = esp_http_client_perform(client);
  if (err == ESP_OK)
  {
    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "GitHub API returned status code: %d", status_code);
        esp_http_client_cleanup(client);
        return;
    }

    // We read into a buffer. Note: Release API response can be large.
    // We only need the tag_name.
    // If it's a list (allowPrerelease=true), it starts with '['.
    // If it's a single object (allowPrerelease=false), it starts with '{'.

    // Using cJSON to parse might be memory intensive if the JSON is huge.
    // Let's try to read enough to find "tag_name".

    char *buffer = (char*)malloc(8192);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer for update check");
        esp_http_client_cleanup(client);
        return;
    }

    int len = esp_http_client_read(client, buffer, 8191);

    if (len > 0)
    {
      buffer[len] = 0;

      // Simple string search to avoid full JSON parsing overhead if possible,
      // but cJSON is safer. Let's try cJSON. If it fails, we might need a stream parser.
      cJSON *json = cJSON_Parse(buffer);

      if (json)
      {
          cJSON *targetObj = json;

          if (cJSON_IsArray(json)) {
              targetObj = cJSON_GetArrayItem(json, 0);
          }

          if (targetObj) {
              char *tagName = cJSON_GetStringValue(cJSON_GetObjectItem(targetObj, "tag_name"));
              if (tagName != NULL)
              {
                  // Tag name usually starts with 'v', e.g. "v2.1.0".
                  // Our system expects "2.1.0".
                  if (tagName[0] == 'v') {
                      strncpy(_latestVersion, tagName + 1, sizeof(_latestVersion) - 1);
                  } else {
                      strncpy(_latestVersion, tagName, sizeof(_latestVersion) - 1);
                  }
                  _latestVersion[sizeof(_latestVersion) - 1] = 0;
                  ESP_LOGI(TAG, "Latest version found: %s", _latestVersion);
              }
          }
          cJSON_Delete(json);
      } else {
          ESP_LOGW(TAG, "Failed to parse JSON from GitHub API (might be incomplete)");
          // Fallback manual parsing if cJSON failed due to incomplete JSON in buffer
          const char* tagKey = "\"tag_name\":\"";
          char* pos = strstr(buffer, tagKey);
          if (pos) {
              pos += strlen(tagKey);
              char* end = strchr(pos, '"');
              if (end) {
                  *end = 0;
                   if (pos[0] == 'v') {
                      strncpy(_latestVersion, pos + 1, sizeof(_latestVersion) - 1);
                  } else {
                      strncpy(_latestVersion, pos, sizeof(_latestVersion) - 1);
                  }
                   _latestVersion[sizeof(_latestVersion) - 1] = 0;
                   ESP_LOGI(TAG, "Latest version found (fallback): %s", _latestVersion);
              }
          }
      }
    } else {
        ESP_LOGE(TAG, "Failed to read response from GitHub API");
    }

    free(buffer);
  } else {
      ESP_LOGE(TAG, "Failed to check for updates: %s", esp_err_to_name(err));
  }

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