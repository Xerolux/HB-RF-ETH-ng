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
#include "system_reset.h"
#include "semver.h"

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

// Buffer for HTTP event handler to collect response body
struct http_response_data {
    char buffer[64];
    int len;
};

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    struct http_response_data *resp = (struct http_response_data *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && resp) {
        int copy_len = evt->data_len;
        if (resp->len + copy_len >= (int)sizeof(resp->buffer)) {
            copy_len = sizeof(resp->buffer) - 1 - resp->len;
        }
        if (copy_len > 0) {
            memcpy(resp->buffer + resp->len, evt->data, copy_len);
            resp->len += copy_len;
            resp->buffer[resp->len] = 0;
        }
    }
    return ESP_OK;
}

void UpdateCheck::_updateLatestVersion()
{
    // Fetch version from version.txt on GitHub
    const char* url = "https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/refs/heads/main/version.txt";

    struct http_response_data resp = {};

    esp_http_client_config_t config = {};
    config.url = url;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.buffer_size = 1024;
    config.buffer_size_tx = 1024;
    config.timeout_ms = 10000;
    config.user_agent = "HB-RF-ETH-ng";
    config.event_handler = _http_event_handler;
    config.user_data = &resp;

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        int status = esp_http_client_get_status_code(client);

        if (status == 200 && resp.len > 0)
        {
            // Trim whitespace and newlines
            char* pStart = resp.buffer;
            while (*pStart == ' ' || *pStart == '\t' || *pStart == '\r' || *pStart == '\n') pStart++;

            char* end = pStart + strlen(pStart) - 1;
            while (end > pStart && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
                *end = 0;
                end--;
            }

            strncpy(_latestVersion, pStart, sizeof(_latestVersion) - 1);
            _latestVersion[sizeof(_latestVersion) - 1] = 0;

            ESP_LOGI(TAG, "Latest version from server: %s", _latestVersion);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to read version: HTTP %d, body_len=%d", status, resp.len);
        }
    }
    else
    {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

void UpdateCheck::_taskFunc()
{
  // some time for initial network connection
  vTaskDelay(30000 / portTICK_PERIOD_MS);

  for (;;)
  {
    ESP_LOGI(TAG, "Checking for firmware updates...");
    ESP_LOGI(TAG, "Current version: %s", _sysInfo->getCurrentVersion());

    _updateLatestVersion();

    if (strcmp(_latestVersion, "n/a") != 0)
    {
      ESP_LOGI(TAG, "Latest available version: %s", _latestVersion);

      if (compareVersions(_sysInfo->getCurrentVersion(), _latestVersion) < 0)
      {
        ESP_LOGW(TAG, "An updated firmware with version %s is available!", _latestVersion);
        _statusLED->setState(LED::getProgram(LED_PROG_UPDATE_AVAILABLE));
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
    config.timeout_ms = 60000; // 60-second timeout to prevent indefinite stall

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Update successful, restarting...");
        ResetInfo::storeResetReason(RESET_REASON_FIRMWARE_UPDATE);
        full_system_restart();
    } else {
        ESP_LOGE(TAG, "OTA Update failed");
        ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
        _statusLED->setState(LED_STATE_ON); // Reset LED or to previous state?
    }
}