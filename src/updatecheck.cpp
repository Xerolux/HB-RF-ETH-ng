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
#include "string.h"
#include "cJSON.h"
#include "reset_info.h"

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

const char *UpdateCheck::getLatestVersion()
{
  return _latestVersion;
}

// Helper function to compare semantic versions
// Returns: negative if v1 < v2, 0 if v1 == v2, positive if v1 > v2
static int compareVersions(const char* v1, const char* v2)
{
    int major1 = 0, minor1 = 0, patch1 = 0;
    int major2 = 0, minor2 = 0, patch2 = 0;

    sscanf(v1, "%d.%d.%d", &major1, &minor1, &patch1);
    sscanf(v2, "%d.%d.%d", &major2, &minor2, &patch2);

    if (major1 != major2) return major1 - major2;
    if (minor1 != minor2) return minor1 - minor2;
    return patch1 - patch2;
}

void UpdateCheck::_updateLatestVersion()
{
    // Fetch version from version.txt on GitHub
    const char* url = "https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/refs/heads/main/version.txt";

    esp_http_client_config_t config = {};
    config.url = url;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.buffer_size = 1024;
    config.buffer_size_tx = 1024;
    config.timeout_ms = 10000;
    config.user_agent = "HB-RF-ETH-ng";

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (esp_http_client_perform(client) == ESP_OK)
    {
        int content_length = esp_http_client_content_length(client);
        int read_len = 0;
        char buffer[64] = {0};

        if (content_length > 0 && content_length < (int)sizeof(buffer))
        {
            read_len = esp_http_client_read(client, buffer, sizeof(buffer) - 1);
        }
        else
        {
            read_len = esp_http_client_read(client, buffer, sizeof(buffer) - 1);
        }

        if (read_len > 0)
        {
            buffer[read_len] = 0;

            // Trim whitespace and newlines
            char* start = buffer;
            while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;

            char* end = start + strlen(start) - 1;
            while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
                *end = 0;
                end--;
            }

            strncpy(_latestVersion, start, sizeof(_latestVersion) - 1);
            _latestVersion[sizeof(_latestVersion) - 1] = 0;

            ESP_LOGI(TAG, "Latest version from server: %s", _latestVersion);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to read version from response");
        }
    }
    else
    {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGE(TAG, "Failed to check for updates: HTTP status %d", status);
    }

    esp_http_client_cleanup(client);
}

void UpdateCheck::_taskFunc()
{
  // some time for initial network connection
  vTaskDelay(30000 / portTICK_PERIOD_MS);

  for (;;)
  {
    if (_settings->getCheckUpdates()) {
      ESP_LOGI(TAG, "Checking for firmware updates...");
      ESP_LOGI(TAG, "Current version: %s", _sysInfo->getCurrentVersion());

      _updateLatestVersion();

      if (strcmp(_latestVersion, "n/a") != 0)
      {
        ESP_LOGI(TAG, "Latest available version: %s", _latestVersion);

        // Use semantic version comparison instead of string comparison
        if (compareVersions(_sysInfo->getCurrentVersion(), _latestVersion) < 0)
        {
          ESP_LOGW(TAG, "An updated firmware with version %s is available!", _latestVersion);
          _statusLED->setState(LED_STATE_BLINK_SLOW);
        }
        else if (compareVersions(_sysInfo->getCurrentVersion(), _latestVersion) > 0)
        {
          ESP_LOGI(TAG, "Running version (%s) is newer than available version (%s)",
                   _sysInfo->getCurrentVersion(), _latestVersion);
        }
        else
        {
          ESP_LOGI(TAG, "Firmware is up to date (version %s)", _latestVersion);
        }
      }
      else
      {
        ESP_LOGE(TAG, "Failed to determine latest version");
      }
    } else {
        ESP_LOGI(TAG, "Update check is disabled.");
    }

    vTaskDelay((8 * 60 * 60000) / portTICK_PERIOD_MS); // 8h
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
    // Construct URL: https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v<version>/firmware_<version>.bin
    // Note: We assume the tag has 'v' prefix, but version string might not.
    // If _latestVersion is "2.1.1", tag is "v2.1.1" and file is "firmware_2.1.1.bin"
    snprintf(url, sizeof(url), "https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v%s/firmware_%s.bin",
            _latestVersion, _latestVersion);

    ESP_LOGI(TAG, "Starting OTA update from %s", url);
    _statusLED->setState(LED_STATE_BLINK_FAST);

    esp_http_client_config_t config = {};
    config.url = url;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.keep_alive_enable = true;

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Update successful, restarting...");
        ResetInfo::storeResetReason(RESET_REASON_FIRMWARE_UPDATE);
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA Update failed");
        ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
        _statusLED->setState(LED_STATE_ON); // Reset LED or to previous state?
    }
}