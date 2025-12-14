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
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "string.h"
#include "cJSON.h"

static const char *TAG = "UpdateCheck";

void _update_check_task_func(void *parameter)
{
  {
    ((UpdateCheck *)parameter)->_taskFunc();
  }
}

UpdateCheck::UpdateCheck(SysInfo* sysInfo, LED *statusLED, Settings *settings) : _sysInfo(sysInfo), _statusLED(statusLED), _settings(settings)
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

void UpdateCheck::_updateLatestVersion()
{
  const char* url = "https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/master/webui/package.json";

  esp_http_client_config_t config = {};
  config.url = url;
  config.transport_type = HTTP_TRANSPORT_OVER_SSL;

  esp_http_client_handle_t client = esp_http_client_init(&config);

  if (esp_http_client_perform(client) == ESP_OK)
  {
    char buffer[2048];
    int len = esp_http_client_read(client, buffer, sizeof(buffer) - 1);

    if (len > 0)
    {
      buffer[len] = 0;
      cJSON *json = cJSON_Parse(buffer);

      if (json)
      {
        char *latestVersion = cJSON_GetStringValue(cJSON_GetObjectItem(json, "version"));
        if (latestVersion != NULL)
        {
          strncpy(_latestVersion, latestVersion, sizeof(_latestVersion) - 1);
          _latestVersion[sizeof(_latestVersion) - 1] = 0;
        }
        cJSON_Delete(json);
      }
    }
  }

  esp_http_client_cleanup(client);
}

void UpdateCheck::_taskFunc()
{
  // some time for initial network connection
  vTaskDelay(30000 / portTICK_PERIOD_MS);

  for (;;)
  {
    int intervalDays = _settings->getCheckUpdateInterval();

    if (intervalDays > 0)
    {
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
        _statusLED->setState(LED_STATE_OFF);
      }

      // Sleep for the interval period, checking every hour to allow reacting to configuration changes
      uint32_t intervalHours = intervalDays * 24;
      for (uint32_t i = 0; i < intervalHours; i++) {
          if (_settings->getCheckUpdateInterval() != intervalDays) break; // Setting changed
          vTaskDelay((60 * 60 * 1000) / portTICK_PERIOD_MS); // Sleep 1h
      }
    }
    else
    {
      ESP_LOGI(TAG, "Update check disabled.");
      // Check again in 1 hour if setting changed
      vTaskDelay((60 * 60 * 1000) / portTICK_PERIOD_MS);
    }
  }

  vTaskDelete(NULL);
}

esp_err_t UpdateCheck::performOnlineUpdate()
{
  if (strcmp(_latestVersion, "n/a") == 0)
  {
     _updateLatestVersion();
     if (strcmp(_latestVersion, "n/a") == 0) {
         ESP_LOGE(TAG, "Could not determine latest version for update.");
         return ESP_FAIL;
     }
  }

  char url[256];
  snprintf(url, sizeof(url), "https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/%s/HB-RF-ETH-ng.bin", _latestVersion);

  ESP_LOGI(TAG, "Starting online update from %s", url);

  esp_http_client_config_t config = {};
  config.url = url;
  config.transport_type = HTTP_TRANSPORT_OVER_SSL;
  config.cert_pem = NULL; // We might need a cert bundle here or skip verify
  config.crt_bundle_attach = esp_crt_bundle_attach; // Use ESP-IDF default bundle
  config.keep_alive_enable = true;
  config.buffer_size_tx = 4096; // Increase buffer size for OTA
  config.timeout_ms = 30000;

  _statusLED->setState(LED_STATE_BLINK_FAST);

  esp_err_t ret = esp_https_ota(&config);

  if (ret == ESP_OK)
  {
      ESP_LOGI(TAG, "OTA Update successful");
      _statusLED->setState(LED_STATE_OFF);
      return ESP_OK;
  }
  else
  {
      ESP_LOGE(TAG, "OTA Update failed: %s", esp_err_to_name(ret));
      _statusLED->setState(LED_STATE_OFF);
      return ret;
  }
}