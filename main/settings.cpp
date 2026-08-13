/*
 *  settings.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 6.0 and modern toolchains
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

#include "settings.h"
#include "validation.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_log.h"
#include "nvs_storage_lock.h"
#include <string.h>

Settings::Settings()
{
  _mutex = xSemaphoreCreateMutex();
  if (_mutex == NULL) {
    ESP_LOGE("Settings", "FAILED to create settings mutex - configuration will not be thread-safe");
  }
  const esp_err_t err = load();
  if (err != ESP_OK) {
    ESP_LOGE("Settings", "Settings load failed; using safe RAM defaults: %s",
             esp_err_to_name(err));
  }
}

static const char *TAG = "Settings";
static const char *NVS_NAMESPACE = "HB-RF-ETH";
static const char *SETTINGS_TXN_NVS_NAMESPACE = "settings_txn";
static const char *SETTINGS_TXN_PENDING_KEY = "pending";
static const char *SETTINGS_TXN_AUTH_BACKUP_KEY = "auth_backup";
static const char *MONITORING_NVS_NAMESPACE = "monitoring";
static const char *MONITORING_TXN_NVS_NAMESPACE = "monitoring_txn";
static const char *THEME_NVS_NAMESPACE = "ui_theme";
static const char *RESET_INFO_NVS_NAMESPACE = "reset_info";
static const char *UPDATE_CACHE_NVS_NAMESPACE = "upd_cache";
// Legacy namespace from the removed supporter-key/CRL feature. Kept only so a
// factory reset on devices that once stored a CRL cache can purge the residue.
static const char *LEGACY_SUPPORTER_CRL_NVS_NAMESPACE = "supporter_crl";
static const char *MQTT_CLEANUP_NVS_NAMESPACE = "mqtt_cleanup";

static constexpr uint8_t STORAGE_SCOPE_SETTINGS = 1U << 0;
static constexpr uint8_t STORAGE_SCOPE_THEME = 1U << 1;
static constexpr uint8_t STORAGE_SCOPE_MONITORING = 1U << 2;
static constexpr uint8_t STORAGE_SCOPE_RESTORE =
    STORAGE_SCOPE_SETTINGS | STORAGE_SCOPE_THEME | STORAGE_SCOPE_MONITORING;
// Factory reset deliberately has no authentication rollback image. Keeping a
// distinct value (rather than another bit in STORAGE_SCOPE_RESTORE) prevents
// interrupted reset recovery from ever restoring the credentials being wiped.
static constexpr uint8_t STORAGE_SCOPE_FACTORY_RESET = 1U << 7;

// Access is serialized by NvsStorageLock. This flag only controls nesting in
// the current boot; the durable marker above is the authority after a reset.
static bool s_restore_transaction_active = false;

struct SettingsAuthBackup {
  uint32_t magic;
  uint8_t version;
  uint8_t username_present;
  uint8_t password_present;
  uint8_t password_changed_present;
  uint8_t token_present;
  int8_t password_changed;
  uint8_t reserved[2];
  char username[33];
  char password[33];
  char token[64];
};

static constexpr uint32_t SETTINGS_AUTH_BACKUP_MAGIC = 0x53415554U;
static constexpr uint8_t SETTINGS_AUTH_BACKUP_VERSION = 1;

static esp_err_t erase_nvs_namespace(const char *ns)
{
  nvs_handle_t handle = 0;
  esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGI(TAG, "NVS namespace '%s' not present during factory reset", ns);
    return ESP_OK;
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open('%s') failed in clear(): %s", ns, esp_err_to_name(err));
    return err;
  }

  err = nvs_erase_all(handle);
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }
  nvs_close(handle);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to erase NVS namespace '%s': %s", ns, esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "Erased NVS namespace '%s'", ns);
  }
  return err;
}

static void remember_first_error(esp_err_t candidate, esp_err_t *result)
{
  if (*result == ESP_OK && candidate != ESP_OK) *result = candidate;
}

static esp_err_t read_storage_transaction(uint8_t *scope)
{
  if (scope == nullptr) return ESP_ERR_INVALID_ARG;
  *scope = 0;

  nvs_handle_t handle;
  esp_err_t err = nvs_open(SETTINGS_TXN_NVS_NAMESPACE, NVS_READONLY,
                           &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
  if (err != ESP_OK) return err;

  err = nvs_get_u8(handle, SETTINGS_TXN_PENDING_KEY, scope);
  nvs_close(handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    *scope = 0;
    return ESP_OK;
  }
  return err;
}

static esp_err_t read_auth_backup_source(nvs_handle_t settings_handle,
                                         SettingsAuthBackup *backup)
{
  if (backup == nullptr) return ESP_ERR_INVALID_ARG;
  memset(backup, 0, sizeof(*backup));
  backup->magic = SETTINGS_AUTH_BACKUP_MAGIC;
  backup->version = SETTINGS_AUTH_BACKUP_VERSION;

  size_t length = sizeof(backup->username);
  esp_err_t err = nvs_get_str(settings_handle, "adminUsername",
                              backup->username, &length);
  if (err == ESP_OK) {
    backup->username_present = 1;
  } else if (err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }

  length = sizeof(backup->password);
  err = nvs_get_str(settings_handle, "adminPassword", backup->password,
                    &length);
  if (err == ESP_OK) {
    backup->password_present = 1;
  } else if (err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }

  err = nvs_get_i8(settings_handle, "passwordChanged",
                   &backup->password_changed);
  if (err == ESP_OK) {
    backup->password_changed_present = 1;
  } else if (err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }

  length = sizeof(backup->token);
  err = nvs_get_str(settings_handle, "adminToken", backup->token, &length);
  if (err == ESP_OK) {
    backup->token_present = 1;
  } else if (err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }
  return ESP_OK;
}

static bool valid_auth_backup(const SettingsAuthBackup &backup)
{
  if (backup.magic != SETTINGS_AUTH_BACKUP_MAGIC ||
      backup.version != SETTINGS_AUTH_BACKUP_VERSION) {
    return false;
  }
  if (backup.username_present > 1 || backup.password_present > 1 ||
      backup.password_changed_present > 1 || backup.token_present > 1 ||
      (backup.password_changed_present &&
       backup.password_changed != 0 && backup.password_changed != 1)) {
    return false;
  }
  if (backup.username_present &&
      memchr(backup.username, '\0', sizeof(backup.username)) == nullptr) {
    return false;
  }
  if (backup.password_present &&
      memchr(backup.password, '\0', sizeof(backup.password)) == nullptr) {
    return false;
  }
  if (backup.token_present &&
      memchr(backup.token, '\0', sizeof(backup.token)) == nullptr) {
    return false;
  }
  return true;
}

static esp_err_t load_transaction_auth_backup(SettingsAuthBackup *backup)
{
  if (backup == nullptr) return ESP_ERR_INVALID_ARG;
  nvs_handle_t handle;
  esp_err_t err = nvs_open(SETTINGS_TXN_NVS_NAMESPACE, NVS_READONLY,
                           &handle);
  if (err != ESP_OK) return err;

  size_t length = sizeof(*backup);
  err = nvs_get_blob(handle, SETTINGS_TXN_AUTH_BACKUP_KEY, backup, &length);
  nvs_close(handle);
  if (err != ESP_OK) return err;
  if (length != sizeof(*backup) || !valid_auth_backup(*backup)) {
    return ESP_ERR_INVALID_CRC;
  }
  return ESP_OK;
}

static esp_err_t restore_auth_backup(
    const SettingsAuthBackup &backup)
{
  if (!valid_auth_backup(backup)) return ESP_ERR_INVALID_CRC;
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) return err;

#define RESTORE_AUTH_STEP(expression)             \
  do {                                            \
    if (err == ESP_OK) err = (expression);        \
  } while (0)
  if (backup.username_present) {
    RESTORE_AUTH_STEP(
        nvs_set_str(handle, "adminUsername", backup.username));
  }
  if (backup.password_present) {
    RESTORE_AUTH_STEP(
        nvs_set_str(handle, "adminPassword", backup.password));
  }
  if (backup.password_changed_present) {
    RESTORE_AUTH_STEP(nvs_set_i8(handle, "passwordChanged",
                                 backup.password_changed));
  }
  if (backup.token_present) {
    RESTORE_AUTH_STEP(nvs_set_str(handle, "adminToken", backup.token));
  }
  RESTORE_AUTH_STEP(nvs_commit(handle));
#undef RESTORE_AUTH_STEP
  nvs_close(handle);
  return err;
}

static esp_err_t write_storage_transaction(uint8_t scope,
                                           nvs_handle_t settings_handle)
{
  if (scope == 0) return ESP_ERR_INVALID_ARG;

  uint8_t pending = 0;
  esp_err_t err = read_storage_transaction(&pending);
  if (err != ESP_OK) return err;
  if (pending != 0) {
    ESP_LOGE(TAG,
             "Refusing to overwrite an unfinished storage transaction (scope=0x%02x)",
             pending);
    return ESP_ERR_INVALID_STATE;
  }

  SettingsAuthBackup backup = {};
  err = read_auth_backup_source(settings_handle, &backup);
  if (err != ESP_OK) return err;

  nvs_handle_t handle;
  err = nvs_open(SETTINGS_TXN_NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) return err;

  // The authentication rollback image must be durable before the marker is
  // armed. If it cannot fit, no Settings key has been changed yet.
  err = nvs_set_blob(handle, SETTINGS_TXN_AUTH_BACKUP_KEY, &backup,
                     sizeof(backup));
  if (err == ESP_OK) err = nvs_commit(handle);
  if (err == ESP_OK) err = nvs_set_u8(handle, SETTINGS_TXN_PENDING_KEY, scope);
  if (err == ESP_OK) err = nvs_commit(handle);

  if (err != ESP_OK) {
    // No configuration write has started. Best-effort removal prevents an
    // unarmed backup from consuming the small shared NVS partition forever.
    const esp_err_t cleanup =
        nvs_erase_key(handle, SETTINGS_TXN_AUTH_BACKUP_KEY);
    if (cleanup == ESP_OK) (void)nvs_commit(handle);
  }
  nvs_close(handle);
  return err;
}

static esp_err_t arm_factory_reset_transaction()
{
  nvs_handle_t handle;
  esp_err_t err = nvs_open(SETTINGS_TXN_NVS_NAMESPACE, NVS_READWRITE,
                           &handle);
  if (err != ESP_OK) return err;

  // A physical factory reset is allowed to supersede an unfinished ordinary
  // restore, including one whose auth backup is corrupt. Commit the factory
  // marker first: from that point on every reboot performs a complete wipe and
  // can never consume the old rollback credentials. The stale backup is then
  // removed only as best-effort space cleanup. A crash or cleanup error leaves
  // the already-durable factory marker as the sole recovery authority.
  err = nvs_set_u8(handle, SETTINGS_TXN_PENDING_KEY,
                   STORAGE_SCOPE_FACTORY_RESET);
  if (err == ESP_OK) err = nvs_commit(handle);
  if (err == ESP_OK) {
    esp_err_t cleanup_err =
        nvs_erase_key(handle, SETTINGS_TXN_AUTH_BACKUP_KEY);
    if (cleanup_err == ESP_ERR_NVS_NOT_FOUND) cleanup_err = ESP_OK;
    if (cleanup_err == ESP_OK) cleanup_err = nvs_commit(handle);
    if (cleanup_err != ESP_OK) {
      ESP_LOGW(TAG,
               "Factory marker committed, but stale auth-backup cleanup failed: %s",
               esp_err_to_name(cleanup_err));
    }
  }
  nvs_close(handle);
  return err;
}

static esp_err_t clear_storage_transaction()
{
  nvs_handle_t handle;
  esp_err_t err = nvs_open(SETTINGS_TXN_NVS_NAMESPACE, NVS_READWRITE,
                           &handle);
  if (err != ESP_OK) return err;
  // Marker first: after it is gone the complete new generation is active.
  // A leftover unarmed auth backup is harmless and can be overwritten by the
  // next transaction if cleanup below suffers a flash error.
  err = nvs_erase_key(handle, SETTINGS_TXN_PENDING_KEY);
  if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
  if (err == ESP_OK) err = nvs_commit(handle);
  if (err == ESP_OK) {
    esp_err_t cleanup_err =
        nvs_erase_key(handle, SETTINGS_TXN_AUTH_BACKUP_KEY);
    if (cleanup_err == ESP_ERR_NVS_NOT_FOUND) cleanup_err = ESP_OK;
    if (cleanup_err == ESP_OK) cleanup_err = nvs_commit(handle);
    if (cleanup_err != ESP_OK) {
      ESP_LOGW(TAG,
               "Configuration committed, but stale auth-backup cleanup failed: %s",
               esp_err_to_name(cleanup_err));
    }
  }
  nvs_close(handle);
  return err;
}

static esp_err_t recover_factory_reset_transaction()
{
  esp_err_t result = ESP_OK;
  remember_first_error(erase_nvs_namespace(NVS_NAMESPACE), &result);
  remember_first_error(erase_nvs_namespace(MONITORING_NVS_NAMESPACE),
                       &result);
  remember_first_error(erase_nvs_namespace(MONITORING_TXN_NVS_NAMESPACE),
                       &result);
  remember_first_error(erase_nvs_namespace(THEME_NVS_NAMESPACE), &result);
  remember_first_error(erase_nvs_namespace(RESET_INFO_NVS_NAMESPACE),
                       &result);
  remember_first_error(erase_nvs_namespace(UPDATE_CACHE_NVS_NAMESPACE),
                       &result);
  remember_first_error(erase_nvs_namespace(LEGACY_SUPPORTER_CRL_NVS_NAMESPACE),
                       &result);
  remember_first_error(erase_nvs_namespace(MQTT_CLEANUP_NVS_NAMESPACE),
                       &result);

  // This namespace contains the durable authority to retry. Erase it only
  // after every factory-reset target was committed successfully.
  if (result == ESP_OK) {
    result = erase_nvs_namespace(SETTINGS_TXN_NVS_NAMESPACE);
    if (result == ESP_OK) s_restore_transaction_active = false;
  }
  return result;
}

// Must run before opening/loading the Settings namespace. A malformed or
// unknown marker is indistinguishable from a damaged factory-reset marker, so
// it always selects the credential-free factory recovery path. It must never
// be converted into STORAGE_SCOPE_RESTORE because an unrelated stale
// auth_backup could otherwise reactivate credentials that a reset meant to
// erase.
static esp_err_t recover_storage_transaction()
{
  uint8_t scope = 0;
  esp_err_t err = read_storage_transaction(&scope);
  if (err == ESP_ERR_NVS_TYPE_MISMATCH || err == ESP_ERR_NVS_INVALID_LENGTH) {
    ESP_LOGE(TAG,
             "Corrupt storage transaction marker detected; clearing all user configuration");
    return recover_factory_reset_transaction();
  }
  if (err != ESP_OK || scope == 0) return err;

  if (scope == STORAGE_SCOPE_FACTORY_RESET) {
    ESP_LOGW(TAG,
             "Interrupted factory reset detected; retrying complete configuration wipe");
    return recover_factory_reset_transaction();
  }

  // Only these two rollback scopes are ever written. Do not accept arbitrary
  // subsets of STORAGE_SCOPE_RESTORE: a corrupt value such as SETTINGS|THEME
  // would otherwise consume a stale auth_backup and could resurrect erased
  // credentials.
  if (scope != STORAGE_SCOPE_SETTINGS && scope != STORAGE_SCOPE_RESTORE) {
    ESP_LOGE(TAG,
             "Unknown storage transaction scope 0x%02x; clearing all user configuration",
             scope);
    return recover_factory_reset_transaction();
  }

  ESP_LOGW(TAG,
           "Interrupted configuration write detected (scope=0x%02x); restoring safe defaults",
           scope);

  SettingsAuthBackup auth_backup = {};
  if ((scope & STORAGE_SCOPE_SETTINGS) != 0) {
    err = load_transaction_auth_backup(&auth_backup);
    if (err != ESP_OK) {
      ESP_LOGE(TAG,
               "Cannot recover Settings without the authentication backup: %s",
               esp_err_to_name(err));
      return err;
    }
  }

  esp_err_t recovery_result = ESP_OK;
  if ((scope & STORAGE_SCOPE_SETTINGS) != 0) {
    const esp_err_t erase_result = erase_nvs_namespace(NVS_NAMESPACE);
    remember_first_error(erase_result, &recovery_result);
    if (erase_result == ESP_OK) {
      remember_first_error(restore_auth_backup(auth_backup),
                           &recovery_result);
    }
  }
  if ((scope & STORAGE_SCOPE_MONITORING) != 0) {
    remember_first_error(erase_nvs_namespace(MONITORING_NVS_NAMESPACE),
                         &recovery_result);
    remember_first_error(erase_nvs_namespace(MONITORING_TXN_NVS_NAMESPACE),
                         &recovery_result);
  }
  if ((scope & STORAGE_SCOPE_THEME) != 0) {
    remember_first_error(erase_nvs_namespace(THEME_NVS_NAMESPACE),
                         &recovery_result);
  }

  // Retain the marker unless every affected namespace was safely erased. A
  // later boot can then retry recovery after a transient flash error.
  if (recovery_result == ESP_OK) {
    recovery_result = clear_storage_transaction();
    if (recovery_result == ESP_OK) {
      s_restore_transaction_active = false;
    }
  }
  return recovery_result;
}

#define GET_IP_ADDR(handle, name, var, defaultValue)  \
  if (nvs_get_u32(handle, name, &var.addr) != ESP_OK) \
  {                                                   \
    var.addr = defaultValue;                          \
  }

#define GET_INT(handle, name, var, defaultValue) \
  if (nvs_get_i32(handle, name, &var) != ESP_OK) \
  {                                              \
    var = defaultValue;                          \
  }

#define GET_BOOL(handle, name, var, defaultValue)          \
  int8_t __##var##_temp;                                   \
  if (nvs_get_i8(handle, name, &__##var##_temp) != ESP_OK) \
  {                                                        \
    var = defaultValue;                                    \
  }                                                        \
  else                                                     \
  {                                                        \
    var = (__##var##_temp != 0);                           \
  }

#define SET_IP_ADDR(handle, name, var) ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_u32(handle, name, var.addr));
#define SET_INT(handle, name, var) ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_i32(handle, name, var));
#define SET_STR(handle, name, var) ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_str(handle, name, var));
#define SET_BOOL(handle, name, var) ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_i8(handle, name, var ? 1 : 0));

void Settings::resetToSafeDefaultsLocked()
{
  snprintf(_adminPassword, sizeof(_adminPassword), "%s", "admin");
  snprintf(_adminUsername, sizeof(_adminUsername), "%s", "admin");
  _passwordChanged = false;

  uint8_t base_mac[6] = {};
  if (esp_read_mac(base_mac, ESP_MAC_WIFI_STA) != ESP_OK) {
    ESP_LOGW(TAG, "Could not read base MAC while creating default hostname");
  }
  snprintf(_hostname, sizeof(_hostname), "HB-RF-ETH-%02X%02X%02X",
           base_mac[3], base_mac[4], base_mac[5]);
  _useDHCP = true;
  _localIP.addr = IPADDR_ANY;
  _netmask.addr = IPADDR_ANY;
  _gateway.addr = IPADDR_ANY;
  IP4_ADDR(&_dns1, 1, 1, 1, 1);
  _dns2.addr = IPADDR_ANY;

  _timesource = TIMESOURCE_NTP;
  _dcfOffset = 40000;
  _gpsBaudrate = 9600;
  snprintf(_ntpServer, sizeof(_ntpServer), "%s", "pool.ntp.org");

  static const int32_t default_led_programs[7] = {
      1, 5, 6, 4, 10, 4, 5,
  };
  memcpy(_ledPrograms, default_led_programs, sizeof(_ledPrograms));
  _ledBrightness = 100;

  _enableIPv6 = false;
  snprintf(_ipv6Mode, sizeof(_ipv6Mode), "%s", "auto");
  _ipv6Address[0] = '\0';
  _ipv6PrefixLength = 64;
  _ipv6Gateway[0] = '\0';
  _ipv6Dns1[0] = '\0';
  _ipv6Dns2[0] = '\0';
  _ccuIP[0] = '\0';

  _systemLogEnabled = false;
  _flashPause = true;
  _testDesignEnabled = true;
}

void Settings::lockAuthenticationAfterStorageFailureLocked()
{
  // Never turn a flash/recovery fault into the known admin/admin credential.
  // The random value is intentionally not logged; a successful physical
  // factory reset is required to regain access if durable recovery keeps
  // failing.
  snprintf(_adminUsername, sizeof(_adminUsername), "%s", "admin");
  snprintf(_adminPassword, sizeof(_adminPassword),
           "%08lx%08lx%08lx%08lx",
           (unsigned long)esp_random(), (unsigned long)esp_random(),
           (unsigned long)esp_random(), (unsigned long)esp_random());
  _passwordChanged = true;
}

static bool valid_stored_admin_username(const char *value)
{
  if (value == nullptr) return false;
  const size_t length = strlen(value);
  if (length == 0 || length > 32) return false;
  for (size_t index = 0; index < length; ++index) {
    const char c = value[index];
    const bool valid = (c >= 'A' && c <= 'Z') ||
                       (c >= 'a' && c <= 'z') ||
                       (c >= '0' && c <= '9') ||
                       c == '-' || c == '_' || c == '.';
    if (!valid) return false;
  }
  return true;
}

esp_err_t Settings::load()
{
  NvsStorageLock storage_lock(portMAX_DELAY, "settings.load");
  if (!storage_lock) {
    ESP_LOGE(TAG, "Could not reserve NVS storage while loading settings");
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    resetToSafeDefaultsLocked();
    lockAuthenticationAfterStorageFailureLocked();
    if (_mutex) xSemaphoreGive(_mutex);
    return ESP_ERR_NO_MEM;
  }
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  resetToSafeDefaultsLocked();
  _storageHealthy = false;

  nvs_handle_t handle = 0;

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    const esp_err_t initialization_error = err;
    esp_err_t repair_error = nvs_flash_erase();
    if (repair_error == ESP_OK) repair_error = nvs_flash_init();
    ESP_LOGE(TAG,
             "NVS required destructive initialization recovery; refusing authentication on this boot (repair=%s)",
             esp_err_to_name(repair_error));
    resetToSafeDefaultsLocked();
    lockAuthenticationAfterStorageFailureLocked();
    if (_mutex) xSemaphoreGive(_mutex);
    return repair_error == ESP_OK ? initialization_error : repair_error;
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(err));
    lockAuthenticationAfterStorageFailureLocked();
    if (_mutex) xSemaphoreGive(_mutex);
    return err;
  }

  err = recover_storage_transaction();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Configuration transaction recovery failed: %s",
             esp_err_to_name(err));
    lockAuthenticationAfterStorageFailureLocked();
    if (_mutex) xSemaphoreGive(_mutex);
    return err;
  }

  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open failed in load(): %s", esp_err_to_name(err));
    lockAuthenticationAfterStorageFailureLocked();
    if (_mutex) xSemaphoreGive(_mutex);
    return err;
  }

  bool persist_defaults = false;
  auto fail_auth_load = [&](const char *key, esp_err_t failure) -> esp_err_t {
    ESP_LOGE(TAG, "Could not safely load authentication key '%s': %s", key,
             esp_err_to_name(failure));
    nvs_close(handle);
    resetToSafeDefaultsLocked();
    lockAuthenticationAfterStorageFailureLocked();
    _storageHealthy = false;
    if (_mutex) xSemaphoreGive(_mutex);
    return failure;
  };

  size_t adminUsernameLength = sizeof(_adminUsername);
  err = nvs_get_str(handle, "adminUsername", _adminUsername,
                    &adminUsernameLength);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    snprintf(_adminUsername, sizeof(_adminUsername), "%s", "admin");
    // Force one login after the username feature is introduced. Existing
    // passwords remain valid; only old browser tokens are invalidated.
    err = nvs_erase_key(handle, "adminToken");
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) err = nvs_commit(handle);
    if (err != ESP_OK) return fail_auth_load("adminToken", err);
    persist_defaults = true;
  } else if (err != ESP_OK) {
    return fail_auth_load("adminUsername", err);
  } else if (!valid_stored_admin_username(_adminUsername)) {
    return fail_auth_load("adminUsername", ESP_ERR_INVALID_ARG);
  }

  size_t adminPasswordLength = sizeof(_adminPassword);
  err = nvs_get_str(handle, "adminPassword", _adminPassword,
                    &adminPasswordLength);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    snprintf(_adminPassword, sizeof(_adminPassword), "%s", "admin");
    _passwordChanged = false;
  } else if (err != ESP_OK) {
    return fail_auth_load("adminPassword", err);
  } else if (_adminPassword[0] == '\0') {
    return fail_auth_load("adminPassword", ESP_ERR_INVALID_ARG);
  } else {
    // Check if password was changed (if it's not default "admin")
    _passwordChanged = (strcmp(_adminPassword, "admin") != 0);
  }

  // A missing flag is a genuine legacy/first-boot case. Any other read error
  // (including a wrong NVS type) makes the whole auth record untrustworthy.
  int8_t stored_password_changed = 0;
  err = nvs_get_i8(handle, "passwordChanged", &stored_password_changed);
  if (err == ESP_OK) {
    _passwordChanged = stored_password_changed != 0;
  } else if (err != ESP_ERR_NVS_NOT_FOUND) {
    return fail_auth_load("passwordChanged", err);
  }

  size_t hostnameLength = sizeof(_hostname);
  if (nvs_get_str(handle, "hostname", _hostname, &hostnameLength) != ESP_OK)
  {
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    snprintf(_hostname, sizeof(_hostname) - 1, "HB-RF-ETH-%02X%02X%02X", baseMac[3], baseMac[4], baseMac[5]);
  }
  else if (!validateHostname(_hostname))
  {
    ESP_LOGW(TAG, "Invalid hostname in NVS, restoring default");
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    snprintf(_hostname, sizeof(_hostname), "HB-RF-ETH-%02X%02X%02X", baseMac[3], baseMac[4], baseMac[5]);
  }

  GET_BOOL(handle, "useDHCP", _useDHCP, true);
  GET_IP_ADDR(handle, "localIP", _localIP, IPADDR_ANY);
  GET_IP_ADDR(handle, "netmask", _netmask, IPADDR_ANY);
  GET_IP_ADDR(handle, "gateway", _gateway, IPADDR_ANY);

  // Use Cloudflare DNS when no primary DNS was stored. Existing non-empty
  // user settings remain authoritative. Legacy 0.0.0.0 values are migrated
  // once so static IPv4 installations can resolve NTP and explicitly
  // configured outbound-service hosts.
  ip4_addr_t defaultDns1;
  IP4_ADDR(&defaultDns1, 1, 1, 1, 1);
  GET_IP_ADDR(handle, "dns1", _dns1, defaultDns1.addr);
  GET_IP_ADDR(handle, "dns2", _dns2, IPADDR_ANY);
  if (_dns1.addr == IPADDR_ANY || _dns1.addr == IPADDR_NONE)
  {
    _dns1 = defaultDns1;
    persist_defaults = true;
  }

  GET_INT(handle, "timesource", _timesource, TIMESOURCE_NTP);
  if (_timesource < TIMESOURCE_NTP || _timesource > TIMESOURCE_GPS)
  {
    ESP_LOGW(TAG, "Invalid time source in NVS, restoring NTP");
    _timesource = TIMESOURCE_NTP;
  }
  
  GET_INT(handle, "dcfOffset", _dcfOffset, 40000);
  if (!validateDcfOffset(_dcfOffset))
  {
    _dcfOffset = 40000;
  }

  GET_INT(handle, "gpsBaudrate", _gpsBaudrate, 9600);
  if (!validateGpsBaudrate(_gpsBaudrate))
  {
    _gpsBaudrate = 9600;
  }

  size_t ntpServerLength = sizeof(_ntpServer);
  if (nvs_get_str(handle, "ntpServer", _ntpServer, &ntpServerLength) != ESP_OK)
  {
    strncpy(_ntpServer, "pool.ntp.org", sizeof(_ntpServer) - 1);
  }
  else if (!validateNtpServer(_ntpServer))
  {
    ESP_LOGW(TAG, "Invalid NTP server in NVS, restoring default");
    strncpy(_ntpServer, "pool.ntp.org", sizeof(_ntpServer) - 1);
  }

  // Initialize default LED programs
  int32_t defaults[7] = {
      1,  // IDLE: ON
      5,  // CCU_DISCONNECTED: BLINK_SLOW
      6,  // CCU_CONNECTED: BLINK_2X
      4,  // UPDATE_AVAILABLE: BLINK_FAST
      10, // ERROR: STROBE
      4,  // BOOTING: BLINK_FAST
      5   // UPDATE_IN_PROGRESS: BLINK_SLOW
  };

  // Check for legacy updateLedBlink setting to migrate preference
  bool updateLedBlink;
  GET_BOOL(handle, "updateLedBlink", updateLedBlink, true);
  if (!updateLedBlink) {
      defaults[3] = 0; // Set UPDATE_AVAILABLE to OFF if legacy setting was false
  }

  // Load LED programs
  char key[16];
  for (int i = 0; i < 7; i++) {
      snprintf(key, sizeof(key), "ledProg%d", i);
      GET_INT(handle, key, _ledPrograms[i], defaults[i]);
      if (_ledPrograms[i] < 0 || _ledPrograms[i] > 10)
      {
        ESP_LOGW(TAG, "Invalid LED program %d in NVS, restoring default", i);
        _ledPrograms[i] = defaults[i];
      }
  }

  GET_INT(handle, "ledBrightness", _ledBrightness, 100);
  if (!validateLEDBrightness(_ledBrightness))
  {
    _ledBrightness = 100;
  }

  // Load IPv6 settings
  GET_BOOL(handle, "enableIPv6", _enableIPv6, false);

  size_t len;
  len = sizeof(_ipv6Mode);
  if (nvs_get_str(handle, "ipv6Mode", _ipv6Mode, &len) != ESP_OK) strncpy(_ipv6Mode, "auto", sizeof(_ipv6Mode) - 1);

  len = sizeof(_ipv6Address);
  if (nvs_get_str(handle, "ipv6Address", _ipv6Address, &len) != ESP_OK) _ipv6Address[0] = 0;

  GET_INT(handle, "ipv6Prefix", _ipv6PrefixLength, 64);

  len = sizeof(_ipv6Gateway);
  if (nvs_get_str(handle, "ipv6Gateway", _ipv6Gateway, &len) != ESP_OK) _ipv6Gateway[0] = 0;

  len = sizeof(_ipv6Dns1);
  if (nvs_get_str(handle, "ipv6Dns1", _ipv6Dns1, &len) != ESP_OK) _ipv6Dns1[0] = 0;

  len = sizeof(_ipv6Dns2);
  if (nvs_get_str(handle, "ipv6Dns2", _ipv6Dns2, &len) != ESP_OK) _ipv6Dns2[0] = 0;

  len = sizeof(_ccuIP);
  if (nvs_get_str(handle, "ccuIP", _ccuIP, &len) != ESP_OK) _ccuIP[0] = 0;

  // NVS key max length is 15; do not rename to "systemLogEnabled" (16) — it
  // silently fails with ESP_ERR_NVS_KEY_TOO_LONG and the toggle won't persist.
  GET_BOOL(handle, "sysLogEnabled", _systemLogEnabled, false);
  // Fixed defaults for all devices; legacy false values are ignored.
  _flashPause = true;
  _testDesignEnabled = true;

  // One-time purge of the removed supporter-key/CRL feature. Older firmware
  // versions stored the supporter key string and a CRL cache in this
  // namespace; the feature is gone, so delete any leftover keys so nothing
  // of the old system survives a plain firmware update (no factory reset
  // needed). Best-effort: a failure only logs and leaves stale bytes.
  {
    bool erased_any = false;
    esp_err_t purge_err = ESP_OK;
    const char *legacy_keys[] = {"supporterKey", "supCrl", "cacheValid"};
    for (const char *legacy_key : legacy_keys) {
      esp_err_t erase_result = nvs_erase_key(handle, legacy_key);
      if (erase_result == ESP_OK) {
        erased_any = true;
      } else if (erase_result != ESP_ERR_NVS_NOT_FOUND) {
        purge_err = erase_result;
      }
    }
    if (erased_any && purge_err == ESP_OK) {
      purge_err = nvs_commit(handle);
    }
    if (purge_err != ESP_OK) {
      ESP_LOGW(TAG, "Could not purge legacy supporter-key/CRL NVS residue: %s",
               esp_err_to_name(purge_err));
    } else if (erased_any) {
      ESP_LOGI(TAG, "Purged legacy supporter-key/CRL residue from Settings namespace");
    }
  }

  nvs_close(handle);

  // Purge the dedicated CRL cache namespace of the removed supporter-key
  // feature on every boot. Idempotent and cheap (open + erase-all + commit;
  // NOT_FOUND is success). Runs under the storage lock because it is a flash
  // write, and best-effort so a failure never blocks settings loading.
  {
    NvsStorageLock storage_lock(pdMS_TO_TICKS(50), "settings.purge_legacy_crl");
    if (!storage_lock) {
      ESP_LOGW(TAG, "Could not reserve NVS storage to purge legacy CRL cache");
    } else {
      nvs_handle_t crl_handle = 0;
      esp_err_t crl_open = nvs_open(LEGACY_SUPPORTER_CRL_NVS_NAMESPACE,
                                    NVS_READWRITE, &crl_handle);
      if (crl_open == ESP_OK) {
        esp_err_t crl_erase = nvs_erase_all(crl_handle);
        esp_err_t crl_commit = crl_erase == ESP_OK
            ? nvs_commit(crl_handle) : crl_erase;
        nvs_close(crl_handle);
        if (crl_commit != ESP_OK) {
          ESP_LOGW(TAG, "Could not purge legacy CRL cache namespace: %s",
                   esp_err_to_name(crl_commit));
        } else {
          ESP_LOGI(TAG, "Purged legacy CRL cache namespace '%s'",
                   LEGACY_SUPPORTER_CRL_NVS_NAMESPACE);
        }
      } else if (crl_open != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Could not open legacy CRL cache namespace for purge: %s",
                 esp_err_to_name(crl_open));
      }
    }
  }

  if (!persist_defaults) _storageHealthy = true;

  if (_mutex) xSemaphoreGive(_mutex);

  if (persist_defaults) {
    err = save();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Could not persist migrated Settings defaults: %s",
               esp_err_to_name(err));
      if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
      _storageHealthy = false;
      lockAuthenticationAfterStorageFailureLocked();
      if (_mutex) xSemaphoreGive(_mutex);
      return err;
    }
  }
  return ESP_OK;
}

static esp_err_t add_nvs_string_entries(const char *value, size_t capacity,
                                        size_t *entries)
{
  if (value == nullptr || entries == nullptr || capacity == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  const size_t length = strnlen(value, capacity);
  if (length >= capacity) return ESP_ERR_INVALID_SIZE;

  // An NVS string occupies one metadata entry plus enough 32-byte entries for
  // its terminating-NUL-inclusive payload.
  *entries += 1 + ((length + 1 + 31) / 32);
  return ESP_OK;
}

esp_err_t Settings::validateStorageCapacityLocked(
    uint32_t handle, size_t *requiredEntries)
{
  // 23 fixed-width keys each consume exactly one NVS entry. Keep this list in
  // sync with save(); strings are added below using their actual lengths.
  size_t required_entries = 23;
  esp_err_t err = ESP_OK;

#define COUNT_STRING(member)                                              \
  do {                                                                    \
    if (err == ESP_OK) {                                                  \
      err = add_nvs_string_entries(_##member, sizeof(_##member),           \
                                   &required_entries);                     \
    }                                                                     \
  } while (0)
  COUNT_STRING(adminPassword);
  COUNT_STRING(adminUsername);
  COUNT_STRING(hostname);
  COUNT_STRING(ntpServer);
  COUNT_STRING(ipv6Mode);
  COUNT_STRING(ipv6Address);
  COUNT_STRING(ipv6Gateway);
  COUNT_STRING(ipv6Dns1);
  COUNT_STRING(ipv6Dns2);
  COUNT_STRING(ccuIP);
#undef COUNT_STRING
  if (err != ESP_OK) return err;

  // The authentication token shares the Settings namespace but is managed by
  // its own API. Preserve it across a successful full-generation rewrite.
  size_t admin_token_length = 0;
  err = nvs_get_str(handle, "adminToken", nullptr, &admin_token_length);
  if (err == ESP_OK) {
    static constexpr size_t ADMIN_TOKEN_CAPACITY = 64;
    if (admin_token_length == 0 ||
        admin_token_length > ADMIN_TOKEN_CAPACITY) {
      ESP_LOGE(TAG, "Stored admin token has invalid length %u",
               (unsigned)admin_token_length);
      return ESP_ERR_INVALID_SIZE;
    }
    required_entries += 1 + ((admin_token_length + 31) / 32);
  } else if (err == ESP_ERR_NVS_NOT_FOUND) {
    err = ESP_OK;
  } else {
    return err;
  }

  nvs_stats_t stats = {};
  size_t old_settings_entries = 0;
  err = nvs_get_stats(nullptr, &stats);
  if (err == ESP_OK) {
    err = nvs_get_used_entry_count(handle, &old_settings_entries);
  }
  if (err != ESP_OK) return err;

  // The old namespace is erased only after this preflight succeeds, so its
  // entries are reclaimable for the replacement. Reserve space for the
  // transaction namespace, auth-backup blob, marker and NVS page
  // housekeeping. The auth backup must fit before old Settings entries may be
  // reclaimed, hence the separate available_entries check.
  static constexpr size_t SETTINGS_TXN_REQUIRED_ENTRIES =
      1 + 1 + 2 + ((sizeof(SettingsAuthBackup) + 31) / 32);
  static constexpr size_t SETTINGS_NVS_RESERVE_ENTRIES = 16;
  static constexpr size_t SETTINGS_NVS_ARMED_RESERVE_ENTRIES = 7;
  const size_t reserve_entries = s_restore_transaction_active
      ? SETTINGS_NVS_ARMED_RESERVE_ENTRIES
      : SETTINGS_NVS_RESERVE_ENTRIES;
  const size_t prospective_entries =
      stats.available_entries + old_settings_entries;
  if ((!s_restore_transaction_active &&
       stats.available_entries < SETTINGS_TXN_REQUIRED_ENTRIES) ||
      required_entries + reserve_entries >
          prospective_entries) {
    ESP_LOGE(TAG,
             "Settings generation needs %u NVS entries; only %u are available after reclaim",
             (unsigned)required_entries, (unsigned)prospective_entries);
    return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
  }

  if (requiredEntries != nullptr) *requiredEntries = required_entries;
  return ESP_OK;
}

esp_err_t Settings::validateStorageCapacity()
{
  NvsStorageLock storage_lock(portMAX_DELAY, "settings.capacity_check");
  if (!storage_lock) return ESP_ERR_NO_MEM;
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err == ESP_OK) {
    err = validateStorageCapacityLocked(handle);
    nvs_close(handle);
  }

  if (_mutex) xSemaphoreGive(_mutex);
  return err;
}

esp_err_t Settings::beginRestoreTransaction()
{
  NvsStorageLock storage_lock(portMAX_DELAY, "settings.restore_begin");
  if (!storage_lock) return ESP_ERR_NO_MEM;
  if (s_restore_transaction_active) return ESP_ERR_INVALID_STATE;

  nvs_handle_t settings_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &settings_handle);
  if (err == ESP_OK) {
    err = write_storage_transaction(STORAGE_SCOPE_RESTORE, settings_handle);
    nvs_close(settings_handle);
  }
  if (err == ESP_OK) s_restore_transaction_active = true;
  return err;
}

esp_err_t Settings::finishRestoreTransaction()
{
  NvsStorageLock storage_lock(portMAX_DELAY, "settings.restore_finish");
  if (!storage_lock) return ESP_ERR_NO_MEM;
  if (!s_restore_transaction_active) return ESP_ERR_INVALID_STATE;

  const esp_err_t err = clear_storage_transaction();
  if (err == ESP_OK) s_restore_transaction_active = false;
  return err;
}

esp_err_t Settings::save()
{
  NvsStorageLock storage_lock(portMAX_DELAY, "settings.save");
  if (!storage_lock) return ESP_ERR_NO_MEM;
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);

  // A caller may restore its in-RAM snapshot and retry immediately after a
  // failed standalone Settings save. Finish recovery of that SETTINGS-only
  // generation here so the rollback can be persisted without requiring a
  // reboot. Never auto-recover a cross-namespace restore marker: its theme and
  // monitoring rollback must stay coordinated by the outer caller.
  esp_err_t err = ESP_OK;
  if (!s_restore_transaction_active) {
    uint8_t pending_scope = 0;
    err = read_storage_transaction(&pending_scope);
    if (err == ESP_OK && pending_scope == STORAGE_SCOPE_SETTINGS) {
      err = recover_storage_transaction();
    } else if (err == ESP_OK && pending_scope != 0) {
      ESP_LOGE(TAG,
               "Settings save blocked by cross-namespace transaction scope 0x%02x",
               pending_scope);
      err = ESP_ERR_INVALID_STATE;
    }
  }
  if (err != ESP_OK) {
    if (_mutex) xSemaphoreGive(_mutex);
    return err;
  }

  nvs_handle_t handle;
  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open failed in save(): %s - settings not persisted",
             esp_err_to_name(err));
    if (_mutex) xSemaphoreGive(_mutex);
    return err;
  }

  // Capture the one independently managed key before erase_all(). No heap is
  // allocated after the durable marker has been written.
  char admin_token[64] = {};
  size_t admin_token_length = sizeof(admin_token);
  bool preserve_admin_token = false;
  err = nvs_get_str(handle, "adminToken", admin_token,
                    &admin_token_length);
  if (err == ESP_OK) {
    preserve_admin_token = true;
  } else if (err == ESP_ERR_NVS_NOT_FOUND) {
    err = ESP_OK;
  }

  if (err == ESP_OK) err = validateStorageCapacityLocked(handle);

  const bool owns_transaction = !s_restore_transaction_active;
  if (err == ESP_OK && owns_transaction) {
    err = write_storage_transaction(STORAGE_SCOPE_SETTINGS, handle);
  }

#define SAVE_STEP(expression)                     \
  do {                                            \
    if (err == ESP_OK) err = (expression);        \
  } while (0)

  // ESP-IDF 6 makes nvs_set_* durable immediately. Erase and rebuild the
  // namespace only after capacity and marker writes succeeded; every failure
  // thereafter retains the marker for boot-time safe-default recovery.
  SAVE_STEP(nvs_erase_all(handle));
  SAVE_STEP(nvs_commit(handle));

  SAVE_STEP(nvs_set_str(handle, "adminPassword", _adminPassword));
  SAVE_STEP(nvs_set_str(handle, "adminUsername", _adminUsername));
  SAVE_STEP(nvs_set_i8(handle, "passwordChanged", _passwordChanged ? 1 : 0));

  SAVE_STEP(nvs_set_str(handle, "hostname", _hostname));
  SAVE_STEP(nvs_set_i8(handle, "useDHCP", _useDHCP ? 1 : 0));
  SAVE_STEP(nvs_set_u32(handle, "localIP", _localIP.addr));
  SAVE_STEP(nvs_set_u32(handle, "netmask", _netmask.addr));
  SAVE_STEP(nvs_set_u32(handle, "gateway", _gateway.addr));
  SAVE_STEP(nvs_set_u32(handle, "dns1", _dns1.addr));
  SAVE_STEP(nvs_set_u32(handle, "dns2", _dns2.addr));

  SAVE_STEP(nvs_set_i32(handle, "timesource", _timesource));
  SAVE_STEP(nvs_set_i32(handle, "dcfOffset", _dcfOffset));
  SAVE_STEP(nvs_set_i32(handle, "gpsBaudrate", _gpsBaudrate));
  SAVE_STEP(nvs_set_str(handle, "ntpServer", _ntpServer));

  char key[16];
  for (int i = 0; i < 7; i++) {
    snprintf(key, sizeof(key), "ledProg%d", i);
    SAVE_STEP(nvs_set_i32(handle, key, _ledPrograms[i]));
  }

  SAVE_STEP(nvs_set_i32(handle, "ledBrightness", _ledBrightness));
  SAVE_STEP(nvs_set_i8(handle, "enableIPv6", _enableIPv6 ? 1 : 0));
  SAVE_STEP(nvs_set_str(handle, "ipv6Mode", _ipv6Mode));
  SAVE_STEP(nvs_set_str(handle, "ipv6Address", _ipv6Address));
  SAVE_STEP(nvs_set_i32(handle, "ipv6Prefix", _ipv6PrefixLength));
  SAVE_STEP(nvs_set_str(handle, "ipv6Gateway", _ipv6Gateway));
  SAVE_STEP(nvs_set_str(handle, "ipv6Dns1", _ipv6Dns1));
  SAVE_STEP(nvs_set_str(handle, "ipv6Dns2", _ipv6Dns2));
  SAVE_STEP(nvs_set_str(handle, "ccuIP", _ccuIP));
  SAVE_STEP(nvs_set_i8(handle, "sysLogEnabled", _systemLogEnabled ? 1 : 0));
  SAVE_STEP(nvs_set_i8(handle, "flashPause", _flashPause ? 1 : 0));
  SAVE_STEP(nvs_set_i8(handle, "testDesign", _testDesignEnabled ? 1 : 0));
  if (preserve_admin_token) {
    SAVE_STEP(nvs_set_str(handle, "adminToken", admin_token));
  }
  SAVE_STEP(nvs_commit(handle));

  if (err == ESP_OK && owns_transaction) {
    err = clear_storage_transaction();
  }
#undef SAVE_STEP

  nvs_close(handle);
  _storageHealthy = (err == ESP_OK);
  if (_mutex) xSemaphoreGive(_mutex);

  if (err != ESP_OK) {
    ESP_LOGE(TAG,
             "Settings save failed; recovery marker retained when armed: %s",
             esp_err_to_name(err));
  }
  return err;
}

void Settings::snapshot(settings_snapshot_t *out)
{
  if (!out) return;
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);

#define SNAPSHOT_COPY(member) memcpy(&out->member, &_##member, sizeof(out->member))
  SNAPSHOT_COPY(adminPassword);
  SNAPSHOT_COPY(adminUsername);
  SNAPSHOT_COPY(passwordChanged);
  SNAPSHOT_COPY(hostname);
  SNAPSHOT_COPY(useDHCP);
  SNAPSHOT_COPY(localIP);
  SNAPSHOT_COPY(netmask);
  SNAPSHOT_COPY(gateway);
  SNAPSHOT_COPY(dns1);
  SNAPSHOT_COPY(dns2);
  SNAPSHOT_COPY(timesource);
  SNAPSHOT_COPY(dcfOffset);
  SNAPSHOT_COPY(gpsBaudrate);
  SNAPSHOT_COPY(ntpServer);
  SNAPSHOT_COPY(ledBrightness);
  SNAPSHOT_COPY(ledPrograms);
  SNAPSHOT_COPY(enableIPv6);
  SNAPSHOT_COPY(ipv6Mode);
  SNAPSHOT_COPY(ipv6Address);
  SNAPSHOT_COPY(ipv6PrefixLength);
  SNAPSHOT_COPY(ipv6Gateway);
  SNAPSHOT_COPY(ipv6Dns1);
  SNAPSHOT_COPY(ipv6Dns2);
  SNAPSHOT_COPY(ccuIP);
  SNAPSHOT_COPY(systemLogEnabled);
  SNAPSHOT_COPY(flashPause);
  SNAPSHOT_COPY(testDesignEnabled);
#undef SNAPSHOT_COPY

  if (_mutex) xSemaphoreGive(_mutex);
}

void Settings::restoreSnapshot(const settings_snapshot_t *snapshot)
{
  if (!snapshot) return;
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);

#define SNAPSHOT_RESTORE(member) \
  memcpy(&_##member, &snapshot->member, sizeof(_##member))
  SNAPSHOT_RESTORE(adminPassword);
  SNAPSHOT_RESTORE(adminUsername);
  SNAPSHOT_RESTORE(passwordChanged);
  SNAPSHOT_RESTORE(hostname);
  SNAPSHOT_RESTORE(useDHCP);
  SNAPSHOT_RESTORE(localIP);
  SNAPSHOT_RESTORE(netmask);
  SNAPSHOT_RESTORE(gateway);
  SNAPSHOT_RESTORE(dns1);
  SNAPSHOT_RESTORE(dns2);
  SNAPSHOT_RESTORE(timesource);
  SNAPSHOT_RESTORE(dcfOffset);
  SNAPSHOT_RESTORE(gpsBaudrate);
  SNAPSHOT_RESTORE(ntpServer);
  SNAPSHOT_RESTORE(ledBrightness);
  SNAPSHOT_RESTORE(ledPrograms);
  SNAPSHOT_RESTORE(enableIPv6);
  SNAPSHOT_RESTORE(ipv6Mode);
  SNAPSHOT_RESTORE(ipv6Address);
  SNAPSHOT_RESTORE(ipv6PrefixLength);
  SNAPSHOT_RESTORE(ipv6Gateway);
  SNAPSHOT_RESTORE(ipv6Dns1);
  SNAPSHOT_RESTORE(ipv6Dns2);
  SNAPSHOT_RESTORE(ccuIP);
  SNAPSHOT_RESTORE(systemLogEnabled);
  SNAPSHOT_RESTORE(flashPause);
  SNAPSHOT_RESTORE(testDesignEnabled);
#undef SNAPSHOT_RESTORE

  if (_mutex) xSemaphoreGive(_mutex);
}

esp_err_t Settings::clear()
{
  esp_err_t marker_result = ESP_OK;
  esp_err_t erase_result = ESP_OK;

  // A factory reset must remove every user-controlled or user-visible value.
  // Keep the "webui" namespace intact because it only tracks the installed
  // WebUI image/transaction metadata; factory reset must not uninstall the
  // firmware or the separately installed WebUI.
  {
    NvsStorageLock storage_lock(portMAX_DELAY, "settings.factory_reset");
    if (!storage_lock) {
      ESP_LOGE(TAG, "Could not reserve NVS storage for factory reset");
      return ESP_ERR_NO_MEM;
    }
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);

    // Do not recover an older ordinary restore first: its auth backup may be
    // precisely the corrupt state the physical reset is intended to escape.
    // The committed factory marker safely supersedes it without preserving
    // credentials, and recovery retries the same complete wipe after a reset.
    marker_result = arm_factory_reset_transaction();
    if (marker_result == ESP_OK) {
      _storageHealthy = false;
      s_restore_transaction_active = false;
      erase_result = recover_factory_reset_transaction();
    }
    if (_mutex) xSemaphoreGive(_mutex);
  }

  esp_err_t result = marker_result;
  remember_first_error(erase_result, &result);

  if (result != ESP_OK) {
    ESP_LOGE(TAG,
             "Factory reset failed; recovery marker retained "
             "(marker=%s, erase=%s)",
             esp_err_to_name(marker_result), esp_err_to_name(erase_result));
  }

  // Reload defaults into RAM after releasing the mutex; load() takes it again.
  const esp_err_t load_result = load();
  remember_first_error(load_result, &result);
  return result;
}

char *Settings::getAdminPassword()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _adminPassword;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

char *Settings::getAdminUsername()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _adminUsername;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

bool Settings::setAdminPassword(const char *adminPassword)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (adminPassword == nullptr || adminPassword[0] == '\0' ||
      strlen(adminPassword) >= sizeof(_adminPassword))
  {
    ESP_LOGE(TAG, "Invalid admin password length, keeping current password");
    if (_mutex) xSemaphoreGive(_mutex);
    return false;
  }

  snprintf(_adminPassword, sizeof(_adminPassword), "%s", adminPassword);
  // Mark password as changed when it's explicitly set
  _passwordChanged = true;
  if (_mutex) xSemaphoreGive(_mutex);
  return true;
}

bool Settings::restoreAdminPassword(const char *adminPassword,
                                    bool passwordChanged)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (adminPassword == nullptr || adminPassword[0] == '\0' ||
      strlen(adminPassword) >= sizeof(_adminPassword))
  {
    ESP_LOGE(TAG, "Invalid restored admin password length");
    if (_mutex) xSemaphoreGive(_mutex);
    return false;
  }

  snprintf(_adminPassword, sizeof(_adminPassword), "%s", adminPassword);
  _passwordChanged = passwordChanged;
  if (_mutex) xSemaphoreGive(_mutex);
  return true;
}

bool Settings::setAdminUsername(const char *adminUsername)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (adminUsername == nullptr || adminUsername[0] == '\0' || strlen(adminUsername) >= sizeof(_adminUsername))
  {
    ESP_LOGE(TAG, "Invalid admin username length, keeping current username");
    if (_mutex) xSemaphoreGive(_mutex);
    return false;
  }

  const size_t len = strlen(adminUsername);
  for (size_t i = 0; i < len; i++)
  {
    const char c = adminUsername[i];
    const bool valid = (c >= 'A' && c <= 'Z') ||
                       (c >= 'a' && c <= 'z') ||
                       (c >= '0' && c <= '9') ||
                       c == '-' || c == '_' || c == '.';
    if (!valid)
    {
      ESP_LOGE(TAG, "Invalid character in admin username, keeping current username");
      if (_mutex) xSemaphoreGive(_mutex);
      return false;
    }
  }

  snprintf(_adminUsername, sizeof(_adminUsername), "%s", adminUsername);
  if (_mutex) xSemaphoreGive(_mutex);
  return true;
}

bool Settings::getPasswordChanged()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  bool result = _passwordChanged;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

char *Settings::getHostname()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _hostname;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

bool Settings::getUseDHCP()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  bool result = _useDHCP;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

ip4_addr_t Settings::getLocalIP()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  ip4_addr_t result = _localIP;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

ip4_addr_t Settings::getNetmask()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  ip4_addr_t result = _netmask;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

ip4_addr_t Settings::getGateway()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  ip4_addr_t result = _gateway;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

ip4_addr_t Settings::getDns1()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  ip4_addr_t result = _dns1;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

ip4_addr_t Settings::getDns2()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  ip4_addr_t result = _dns2;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

bool Settings::setNetworkSettings(const char *hostname, bool useDHCP, ip4_addr_t localIP, ip4_addr_t netmask, ip4_addr_t gateway, ip4_addr_t dns1, ip4_addr_t dns2)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  // Validate hostname
  if (!validateHostname(hostname))
  {
    ESP_LOGE(TAG, "Invalid hostname provided, keeping current value");
    if (_mutex) xSemaphoreGive(_mutex);
    return false;
  }

  // Validate IP addresses if not using DHCP
  if (!useDHCP)
  {
    if (!validateIPAddress(localIP))
    {
      ESP_LOGE(TAG, "Invalid local IP address, keeping current settings");
      if (_mutex) xSemaphoreGive(_mutex);
      return false;
    }
    if (!validateNetmask(netmask))
    {
      ESP_LOGE(TAG, "Invalid netmask, keeping current settings");
      if (_mutex) xSemaphoreGive(_mutex);
      return false;
    }
    if (!validateIPAddress(gateway))
    {
      ESP_LOGE(TAG, "Invalid gateway, keeping current settings");
      if (_mutex) xSemaphoreGive(_mutex);
      return false;
    }
  }

  // Validate DNS servers (optional, can be 0.0.0.0)
  if (dns1.addr != IPADDR_ANY && !validateIPAddress(dns1))
  {
    ESP_LOGE(TAG, "Invalid DNS1 address, keeping current settings");
    if (_mutex) xSemaphoreGive(_mutex);
    return false;
  }
  if (dns2.addr != IPADDR_ANY && !validateIPAddress(dns2))
  {
    ESP_LOGE(TAG, "Invalid DNS2 address, keeping current settings");
    if (_mutex) xSemaphoreGive(_mutex);
    return false;
  }

  // All validations passed, update settings
  strncpy(_hostname, hostname, sizeof(_hostname) - 1);
  _useDHCP = useDHCP;
  _localIP = localIP;
  _netmask = netmask;
  _gateway = gateway;
  _dns1 = dns1;
  _dns2 = dns2;

  if (_mutex) xSemaphoreGive(_mutex);
  return true;
}

int Settings::getDcfOffset()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  int result = _dcfOffset;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setDcfOffset(int dcfOffset)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (!validateDcfOffset(dcfOffset))
  {
    ESP_LOGE(TAG, "Invalid DCF offset, keeping current value");
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }
  _dcfOffset = dcfOffset;
  if (_mutex) xSemaphoreGive(_mutex);
}

int Settings::getGpsBaudrate()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  int result = _gpsBaudrate;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setGpsBaudrate(int gpsBaudrate)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (!validateGpsBaudrate(gpsBaudrate))
  {
    ESP_LOGE(TAG, "Invalid GPS baudrate, keeping current value");
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }
  _gpsBaudrate = gpsBaudrate;
  if (_mutex) xSemaphoreGive(_mutex);
}

timesource_t Settings::getTimesource()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  timesource_t result = (timesource_t)_timesource;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setTimesource(timesource_t timesource)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (timesource < TIMESOURCE_NTP || timesource > TIMESOURCE_GPS)
  {
    ESP_LOGE(TAG, "Invalid time source, keeping current value");
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }
  _timesource = timesource;
  if (_mutex) xSemaphoreGive(_mutex);
}

char *Settings::getNtpServer()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _ntpServer;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setNtpServer(const char *ntpServer)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (!validateNtpServer(ntpServer))
  {
    ESP_LOGE(TAG, "Invalid NTP server, keeping current value");
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }
  strncpy(_ntpServer, ntpServer, sizeof(_ntpServer) - 1);
  if (_mutex) xSemaphoreGive(_mutex);
}

int Settings::getLEDBrightness()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  int result = _ledBrightness;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setLEDBrightness(int ledBrightness)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (!validateLEDBrightness(ledBrightness))
  {
    ESP_LOGE(TAG, "Invalid LED brightness, keeping current value");
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }
  _ledBrightness = ledBrightness;
  if (_mutex) xSemaphoreGive(_mutex);
}

int Settings::getLedProgram(int program)
{
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    if (program < 0 || program >= 7) {
      if (_mutex) xSemaphoreGive(_mutex);
      return 0;
    }
    int result = _ledPrograms[program];
    if (_mutex) xSemaphoreGive(_mutex);
    return result;
}

void Settings::setLedProgram(int program, int state)
{
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    if (program >= 0 && program < 7) {
        // Validate state (0-10)
        if (state >= 0 && state <= 10) {
            _ledPrograms[program] = state;
        }
    }
    if (_mutex) xSemaphoreGive(_mutex);
}

// IPv6 Getters
bool Settings::getEnableIPv6() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  bool result = _enableIPv6;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}
char *Settings::getIPv6Mode() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _ipv6Mode;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}
char *Settings::getIPv6Address() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _ipv6Address;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}
int Settings::getIPv6PrefixLength() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  int result = _ipv6PrefixLength;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}
char *Settings::getIPv6Gateway() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _ipv6Gateway;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}
char *Settings::getIPv6Dns1() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _ipv6Dns1;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}
char *Settings::getIPv6Dns2() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _ipv6Dns2;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

// IPv6 Setter
void Settings::setIPv6Settings(bool enableIPv6, const char *ipv6Mode, const char *ipv6Address, int ipv6PrefixLength, const char *ipv6Gateway, const char *ipv6Dns1, const char *ipv6Dns2)
{
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    _enableIPv6 = enableIPv6;

    // Validate IPv6 mode
    if (ipv6Mode != NULL && (strcmp(ipv6Mode, "auto") == 0 || strcmp(ipv6Mode, "static") == 0 || strcmp(ipv6Mode, "disabled") == 0))
    {
        strncpy(_ipv6Mode, ipv6Mode, sizeof(_ipv6Mode) - 1);
    }
    else
    {
        ESP_LOGE(TAG, "Invalid IPv6 mode, defaulting to 'auto'");
        strncpy(_ipv6Mode, "auto", sizeof(_ipv6Mode) - 1);
    }

    // Validate IPv6 address if provided
    if (ipv6Address != NULL && strlen(ipv6Address) > 0)
    {
        if (!validateIPv6Address(ipv6Address))
        {
            ESP_LOGE(TAG, "Invalid IPv6 address, keeping current value");
            // Don't update _ipv6Address if invalid
        }
        else
        {
            strncpy(_ipv6Address, ipv6Address, sizeof(_ipv6Address) - 1);
        }
    }

    // Validate and clamp prefix length
    if (ipv6PrefixLength < 1) ipv6PrefixLength = 1;
    if (ipv6PrefixLength > 128) ipv6PrefixLength = 128;
    _ipv6PrefixLength = ipv6PrefixLength;

    // Validate IPv6 gateway if provided
    if (ipv6Gateway != NULL && strlen(ipv6Gateway) > 0)
    {
        if (!validateIPv6Address(ipv6Gateway))
        {
            ESP_LOGE(TAG, "Invalid IPv6 gateway, keeping current value");
        }
        else
        {
            strncpy(_ipv6Gateway, ipv6Gateway, sizeof(_ipv6Gateway) - 1);
        }
    }

    // Validate IPv6 DNS servers if provided
    if (ipv6Dns1 != NULL && strlen(ipv6Dns1) > 0)
    {
        if (!validateIPv6Address(ipv6Dns1))
        {
            ESP_LOGE(TAG, "Invalid IPv6 DNS1, keeping current value");
        }
        else
        {
            strncpy(_ipv6Dns1, ipv6Dns1, sizeof(_ipv6Dns1) - 1);
        }
    }

    if (ipv6Dns2 != NULL && strlen(ipv6Dns2) > 0)
    {
        if (!validateIPv6Address(ipv6Dns2))
        {
            ESP_LOGE(TAG, "Invalid IPv6 DNS2, keeping current value");
        }
        else
        {
            strncpy(_ipv6Dns2, ipv6Dns2, sizeof(_ipv6Dns2) - 1);
        }
    }
    if (_mutex) xSemaphoreGive(_mutex);
}

char *Settings::getCCUIP()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _ccuIP;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setCCUIP(const char *ip)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (ip == NULL || ip[0] == '\0') {
    _ccuIP[0] = 0;
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }

  if (!validateCcuAddress(ip)) {
    ESP_LOGE(TAG, "Invalid CCU address, keeping current value");
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }

  strncpy(_ccuIP, ip, sizeof(_ccuIP) - 1);
  if (_mutex) xSemaphoreGive(_mutex);
}

bool Settings::getSystemLogEnabled()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  bool result = _systemLogEnabled;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setSystemLogEnabled(bool enabled)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  _systemLogEnabled = enabled;
  if (_mutex) xSemaphoreGive(_mutex);
}

bool Settings::getFlashPause()
{
  return true;
}

void Settings::setFlashPause(bool enabled)
{
  (void)enabled;
  _flashPause = true;
}

bool Settings::getTestDesignEnabled()
{
  return true;
}

void Settings::setTestDesignEnabled(bool enabled)
{
  (void)enabled;
  _testDesignEnabled = true;
}

// ---- Admin token persistence (survives reboots) ---------------------------

bool Settings::loadAdminToken(char *out, size_t size)
{
    NvsStorageLock storage_lock(portMAX_DELAY, "settings.load_admin_token");
    if (!storage_lock) return false;
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    if (!out || size == 0) {
      if (_mutex) xSemaphoreGive(_mutex);
      return false;
    }
    out[0] = '\0';
    if (!_storageHealthy) {
      if (_mutex) xSemaphoreGive(_mutex);
      return false;
    }
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
      if (_mutex) xSemaphoreGive(_mutex);
      return false;
    }
    size_t len = size;
    esp_err_t err = nvs_get_str(handle, "adminToken", out, &len);
    nvs_close(handle);
    if (_mutex) xSemaphoreGive(_mutex);
    return (err == ESP_OK && len > 1);
}

esp_err_t Settings::saveAdminToken(const char *token)
{
    NvsStorageLock storage_lock(portMAX_DELAY, "settings.save_admin_token");
    if (!storage_lock) {
      return ESP_ERR_NO_MEM;
    }
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    if (!token || token[0] == '\0' || strnlen(token, 64) >= 64) {
      if (_mutex) xSemaphoreGive(_mutex);
      return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
      if (_mutex) xSemaphoreGive(_mutex);
      return err;
    }
    err = nvs_set_str(handle, "adminToken", token);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (_mutex) xSemaphoreGive(_mutex);
    return err;
}

esp_err_t Settings::clearAdminToken()
{
    NvsStorageLock storage_lock(portMAX_DELAY, "settings.clear_admin_token");
    if (!storage_lock) {
      return ESP_ERR_NO_MEM;
    }
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
      if (_mutex) xSemaphoreGive(_mutex);
      return err;
    }
    err = nvs_erase_key(handle, "adminToken");
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (_mutex) xSemaphoreGive(_mutex);
    return err;
}
