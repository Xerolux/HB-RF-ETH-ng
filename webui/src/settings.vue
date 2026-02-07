<template>
  <div class="settings-page">
    <!-- Tabs Navigation -->
    <div class="settings-tabs">
      <button
        v-for="tab in tabs"
        :key="tab.id"
        :class="['tab-button', { active: activeTab === tab.id }]"
        @click="activeTab = tab.id"
      >
        <span class="tab-icon">{{ tab.icon }}</span>
        <span class="tab-label">{{ tab.label }}</span>
      </button>
    </div>

    <!-- Tab Content -->
    <div class="settings-content">
      <!-- General Tab -->
      <div v-show="activeTab === 'general'" class="tab-panel">
        <div class="settings-section">
          <div class="section-header">
            <span class="section-icon">🔒</span>
            <h3 class="section-title">{{ t('settings.security') || 'Security' }}</h3>
          </div>
          <div class="security-actions">
            <div class="security-action-card">
              <div class="security-icon">🔑</div>
              <div class="security-content">
                <h4>{{ t('settings.changePassword') || 'Change Admin Password' }}</h4>
                <p>{{ t('settings.changePasswordHint') || 'Change your administrator password for secure access' }}</p>
                <BButton variant="primary" @click="showPasswordModal = true">
                  <span class="btn-icon">🔐</span>
                  {{ t('settings.changePasswordBtn') || 'Change Password' }}
                </BButton>
              </div>
            </div>
            <div class="security-action-card">
              <div class="security-icon">🛡️</div>
              <div class="security-content">
                <h4>{{ t('settings.otaPassword') || 'OTA Password' }}</h4>
                <p>{{ t('settings.otaPasswordHint') || 'Separate password required for firmware updates' }}</p>
                <div class="security-actions-buttons">
                  <BButton variant="success" @click="showOtaPasswordModal = true">
                    <span class="btn-icon">🔒</span>
                    {{ otaPasswordSet ? (t('settings.changeOtaPassword') || 'Change') : (t('settings.setOtaPassword') || 'Set Password') }}
                  </BButton>
                  <BButton v-if="otaPasswordSet" variant="outline-danger" @click="clearOtaPassword">
                    <span class="btn-icon">🗑️</span>
                    {{ t('settings.clearOtaPassword') || 'Clear' }}
                  </BButton>
                </div>
              </div>
            </div>
          </div>
        </div>

        <div class="settings-section">
          <div class="section-header">
            <span class="section-icon">💡</span>
            <h3 class="section-title">{{ t('settings.systemSettings') || 'System' }}</h3>
          </div>
          <div class="form-group">
            <label>{{ t('settings.ledBrightness') }}</label>
            <div class="brightness-selector">
              <input
                type="range"
                v-model.number="ledBrightness"
                min="0"
                max="100"
                step="5"
                class="brightness-slider"
              />
              <span class="brightness-value">{{ ledBrightness }}%</span>
            </div>
          </div>
          <div class="form-group">
            <label class="checkbox-label">
              <input type="checkbox" v-model="checkUpdates" class="custom-checkbox" />
              <span>{{ t('settings.checkUpdates') || 'Automatic update check' }}</span>
            </label>
          </div>
          <div class="form-group" v-if="checkUpdates">
            <label class="checkbox-label">
              <input type="checkbox" v-model="allowPrerelease" class="custom-checkbox" />
              <span>{{ t('settings.allowPrerelease') || 'Include pre-release versions' }}</span>
            </label>
          </div>
        </div>
      </div>

      <!-- Network Tab -->
      <div v-show="activeTab === 'network'" class="tab-panel">
        <div class="settings-section">
          <div class="section-header">
            <span class="section-icon">🌐</span>
            <h3 class="section-title">{{ t('settings.networkSettings') || 'Network' }}</h3>
          </div>
          <div class="form-grid">
            <div class="form-group full-width">
              <label>{{ t('settings.hostname') }}</label>
              <BFormInput
                type="text"
                v-model="hostname"
                trim
                :state="v$.hostname.$error ? false : null"
              />
            </div>
          </div>
          <div class="form-group">
            <label>{{ t('settings.dhcp') }}</label>
            <div class="toggle-group">
              <button
                :class="['toggle-btn', { active: useDHCP }]"
                @click="useDHCP = true"
              >{{ t('common.enabled') }}</button>
              <button
                :class="['toggle-btn', { active: !useDHCP }]"
                @click="useDHCP = false"
              >{{ t('common.disabled') }}</button>
            </div>
          </div>
          <template v-if="!useDHCP">
            <div class="form-grid">
              <div class="form-group">
                <label>{{ t('settings.ipAddress') }}</label>
                <BFormInput type="text" v-model="localIP" trim :state="v$.localIP.$error ? false : null" />
              </div>
              <div class="form-group">
                <label>{{ t('settings.netmask') }}</label>
                <BFormInput type="text" v-model="netmask" trim :state="v$.netmask.$error ? false : null" />
              </div>
              <div class="form-group">
                <label>{{ t('settings.gateway') }}</label>
                <BFormInput type="text" v-model="gateway" trim :state="v$.gateway.$error ? false : null" />
              </div>
              <div class="form-group">
                <label>{{ t('settings.dns1') }}</label>
                <BFormInput type="text" v-model="dns1" trim :state="v$.dns1.$error ? false : null" />
              </div>
              <div class="form-group">
                <label>{{ t('settings.dns2') }}</label>
                <BFormInput type="text" v-model="dns2" trim :state="v$.dns2.$error ? false : null" />
              </div>
            </div>
          </template>
        </div>

        <div class="settings-section">
          <div class="section-header">
            <span class="section-icon">🔷</span>
            <h3 class="section-title">{{ t('settings.ipv6Settings') || 'IPv6' }}</h3>
          </div>
          <div class="form-group">
            <label class="checkbox-label">
              <input type="checkbox" v-model="enableIPv6" class="custom-checkbox" />
              <span>{{ t('settings.enableIPv6') }}</span>
            </label>
          </div>
          <template v-if="enableIPv6">
            <div class="form-group">
              <label>{{ t('settings.ipv6Mode') }}</label>
              <div class="toggle-group">
                <button
                  :class="['toggle-btn', { active: ipv6Mode === 'auto' }]"
                  @click="ipv6Mode = 'auto'"
                >{{ t('settings.ipv6Auto') || 'Auto' }}</button>
                <button
                  :class="['toggle-btn', { active: ipv6Mode === 'static' }]"
                  @click="ipv6Mode = 'static'"
                >{{ t('settings.ipv6Static') || 'Static' }}</button>
              </div>
            </div>
            <template v-if="ipv6Mode === 'static'">
              <div class="form-grid">
                <div class="form-group full-width">
                  <label>{{ t('settings.ipv6Address') }}</label>
                  <BFormInput
                    type="text"
                    v-model="ipv6Address"
                    trim
                    placeholder="2001:db8::1"
                    :state="v$.ipv6Address.$error ? false : null"
                  />
                </div>
                <div class="form-group">
                  <label>{{ t('settings.ipv6PrefixLength') }}</label>
                  <BFormInput
                    type="number"
                    v-model.number="ipv6PrefixLength"
                    min="1"
                    max="128"
                    placeholder="64"
                    :state="v$.ipv6PrefixLength.$error ? false : null"
                  />
                </div>
                <div class="form-group">
                  <label>{{ t('settings.ipv6Gateway') }}</label>
                  <BFormInput
                    type="text"
                    v-model="ipv6Gateway"
                    trim
                    placeholder="2001:db8::1"
                    :state="v$.ipv6Gateway.$error ? false : null"
                  />
                </div>
                <div class="form-group">
                  <label>{{ t('settings.ipv6Dns1') }}</label>
                  <BFormInput
                    type="text"
                    v-model="ipv6Dns1"
                    trim
                    placeholder="2001:4860:4860::8888"
                    :state="v$.ipv6Dns1.$error ? false : null"
                  />
                </div>
                <div class="form-group">
                  <label>{{ t('settings.ipv6Dns2') }}</label>
                  <BFormInput
                    type="text"
                    v-model="ipv6Dns2"
                    trim
                    placeholder="2001:4860:4860::8844"
                    :state="v$.ipv6Dns2.$error ? false : null"
                  />
                </div>
              </div>
            </template>
          </template>
        </div>
      </div>

      <!-- Time Tab -->
      <div v-show="activeTab === 'time'" class="tab-panel">
        <div class="settings-section">
          <div class="section-header">
            <span class="section-icon">⏰</span>
            <h3 class="section-title">{{ t('settings.timeSettings') || 'Time' }}</h3>
          </div>
          <div class="time-source-selector">
            <button
              v-for="source in timeSources"
              :key="source.value"
              :class="['time-source-btn', { active: timesource === source.value }]"
              @click="timesource = source.value"
            >
              <span class="time-icon">{{ source.icon }}</span>
              <span class="time-label">{{ source.label }}</span>
            </button>
          </div>
          <div v-if="isNtpActivated" class="form-group">
            <label>{{ t('settings.ntpServer') }}</label>
            <BFormInput
              type="text"
              v-model="ntpServer"
              trim
              :state="v$.ntpServer.$error ? false : null"
            />
          </div>
          <div v-if="isDcfActivated" class="form-group">
            <label>{{ t('settings.dcfOffset') }} (µs)</label>
            <BFormInput
              type="number"
              v-model.number="dcfOffset"
              min="0"
              :state="v$.dcfOffset.$error ? false : null"
            />
          </div>
          <div v-if="isGpsActivated" class="form-group">
            <label>{{ t('settings.gpsBaudrate') }}</label>
            <BFormSelect v-model.number="gpsBaudrate">
              <BFormSelectOption :value="4800">4800</BFormSelectOption>
              <BFormSelectOption :value="9600">9600</BFormSelectOption>
              <BFormSelectOption :value="19200">19200</BFormSelectOption>
              <BFormSelectOption :value="38400">38400</BFormSelectOption>
              <BFormSelectOption :value="57600">57600</BFormSelectOption>
              <BFormSelectOption :value="115200">115200</BFormSelectOption>
            </BFormSelect>
          </div>
        </div>
      </div>

      <!-- Backup Tab -->
      <div v-show="activeTab === 'backup'" class="tab-panel">
        <div class="settings-section">
          <div class="section-header">
            <span class="section-icon">💾</span>
            <h3 class="section-title">{{ t('settings.backupRestore') || 'Backup & Restore' }}</h3>
          </div>
          <div class="backup-actions">
            <div class="backup-card">
              <div class="backup-icon">⬇️</div>
              <div class="backup-content">
                <h4>{{ t('settings.downloadBackup') || 'Download Backup' }}</h4>
                <p>{{ t('settings.backupInfo') || 'Download your settings as JSON file' }}</p>
                <BButton variant="primary" @click="downloadBackup">
                  {{ t('settings.download') || 'Download' }}
                </BButton>
              </div>
            </div>
            <div class="backup-card">
              <div class="backup-icon">⬆️</div>
              <div class="backup-content">
                <h4>{{ t('settings.restore') || 'Restore Backup' }}</h4>
                <p>{{ t('settings.restoreInfo') || 'Restore settings from a JSON file' }}</p>
                <input
                  type="file"
                  ref="fileInput"
                  accept=".json"
                  @change="handleFileSelect"
                  class="file-input"
                />
                <BButton
                  variant="warning"
                  :disabled="!restoreFile"
                  @click="restoreSettings"
                >
                  {{ t('settings.restoreBtn') || 'Restore' }}
                </BButton>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- Save Button Bar -->
    <div class="settings-footer">
      <div class="footer-content">
        <BAlert
          variant="success"
          :model-value="showSuccess"
          dismissible
          fade
          @update:model-value="showSuccess = null"
          class="footer-alert"
        >{{ t("settings.saveSuccess") }}</BAlert>
        <BAlert
          variant="danger"
          :model-value="showError"
          dismissible
          fade
          @update:model-value="showError = null"
          class="footer-alert"
        >{{ t("settings.saveError") }}</BAlert>
        <BButton
          variant="primary"
          size="lg"
          @click="saveSettingsClick"
          :disabled="v$.$error"
          class="save-btn"
        >
          <span class="save-icon">💾</span>
          {{ t('common.save') }}
        </BButton>
      </div>
    </div>

    <!-- Password Change Modal -->
    <PasswordChangeModal v-model="showPasswordModal" :force-redirect="false" />

    <!-- OTA Password Modal -->
    <OtaPasswordModal v-model="showOtaPasswordModal" @success="onOtaPasswordSet" />
  </div>
</template>

<script setup>
import axios from 'axios'
import { ref, computed, onMounted, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useVuelidate } from '@vuelidate/core'
import {
  required,
  minLength,
  maxLength,
  numeric,
  ipAddress,
  helpers,
  requiredIf,
  requiredUnless
} from '@vuelidate/validators'
import { useSettingsStore, useLoginStore } from './stores.js'
import PasswordChangeModal from './components/PasswordChangeModal.vue'
import OtaPasswordModal from './components/OtaPasswordModal.vue'

const { t } = useI18n()
const settingsStore = useSettingsStore()
const loginStore = useLoginStore()

// Password modal
const showPasswordModal = ref(false)
const showOtaPasswordModal = ref(false)
const otaPasswordSet = ref(false)

// Tab management
const activeTab = ref('general')

const tabs = computed(() => [
  { id: 'general', icon: '⚙️', label: t('settings.tabGeneral') || 'General' },
  { id: 'network', icon: '🌐', label: t('settings.tabNetwork') || 'Network' },
  { id: 'time', icon: '⏰', label: t('settings.tabTime') || 'Time' },
  { id: 'backup', icon: '💾', label: t('settings.tabBackup') || 'Backup' }
])

const timeSources = computed(() => [
  { value: 0, icon: '🌍', label: t('settings.ntp') || 'NTP' },
  { value: 1, icon: '📻', label: t('settings.dcf') || 'DCF' },
  { value: 2, icon: '🛰️', label: t('settings.gps') || 'GPS' }
])

// Local form state
const restoreFile = ref(null)
const fileInput = ref(null)
const hostname = ref('')
const useDHCP = ref(true)
const localIP = ref('')
const netmask = ref('')
const gateway = ref('')
const dns1 = ref('')
const dns2 = ref('')

// IPv6 settings
const enableIPv6 = ref(false)
const ipv6Mode = ref('auto')
const ipv6Address = ref('')
const ipv6PrefixLength = ref(64)
const ipv6Gateway = ref('')
const ipv6Dns1 = ref('')
const ipv6Dns2 = ref('')

const timesource = ref(0)
const dcfOffset = ref(0)
const gpsBaudrate = ref(9600)
const ntpServer = ref('')
const ledBrightness = ref(100)
const checkUpdates = ref(true)
const allowPrerelease = ref(false)

const showSuccess = ref(null)
const showError = ref(null)

// Computed flags
const isNtpActivated = computed(() => timesource.value === 0)
const isDcfActivated = computed(() => timesource.value === 1)
const isGpsActivated = computed(() => timesource.value === 2)
const isIPv6Static = computed(() => enableIPv6.value && ipv6Mode.value === 'static')

const hostname_validator = helpers.regex(/^[a-zA-Z0-9_-]{1,63}$/)
const domainname_validator = helpers.regex(/^([a-zA-Z0-9_-]{1,63}\.)*[a-zA-Z0-9_-]{1,63}$/)
const ipv6_validator = helpers.regex(/^(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:))$/)

// Validation rules
const rules = {
  hostname: {
    required,
    hostname_validator,
    maxLength: maxLength(32)
  },
  localIP: {
    required: requiredUnless(useDHCP),
    ipAddress
  },
  netmask: {
    required: requiredUnless(useDHCP),
    ipAddress
  },
  gateway: {
    required: requiredUnless(useDHCP),
    ipAddress
  },
  dns1: {
    required: requiredUnless(useDHCP),
    ipAddress
  },
  dns2: {
    ipAddress
  },
  ipv6Address: {
    required: requiredIf(isIPv6Static),
    ipv6_validator: helpers.withMessage('Invalid IPv6 address', ipv6_validator)
  },
  ipv6PrefixLength: {
    required: requiredIf(isIPv6Static),
    numeric,
    minValue: helpers.withMessage('Min 1', val => val >= 1),
    maxValue: helpers.withMessage('Max 128', val => val <= 128)
  },
  ipv6Gateway: {
    required: requiredIf(isIPv6Static),
    ipv6_validator: helpers.withMessage('Invalid IPv6 address', ipv6_validator)
  },
  ipv6Dns1: {
    required: requiredIf(isIPv6Static),
    ipv6_validator: helpers.withMessage('Invalid IPv6 address', ipv6_validator)
  },
  ipv6Dns2: {
    ipv6_validator: helpers.withMessage('Invalid IPv6 address', ipv6_validator)
  },
  ntpServer: {
    required: requiredIf(isNtpActivated),
    domainname_validator,
    maxLength: maxLength(64)
  },
  dcfOffset: {
    required: requiredIf(isDcfActivated),
    numeric
  }
}

const v$ = useVuelidate(rules, {
  hostname,
  localIP,
  netmask,
  gateway,
  dns1,
  dns2,
  ipv6Address,
  ipv6PrefixLength,
  ipv6Gateway,
  ipv6Dns1,
  ipv6Dns2,
  ntpServer,
  dcfOffset
})

// Load settings from store
const loadSettings = () => {
  hostname.value = settingsStore.hostname
  useDHCP.value = settingsStore.useDHCP
  localIP.value = settingsStore.localIP
  netmask.value = settingsStore.netmask
  gateway.value = settingsStore.gateway
  dns1.value = settingsStore.dns1
  dns2.value = settingsStore.dns2
  timesource.value = settingsStore.timesource
  dcfOffset.value = settingsStore.dcfOffset
  gpsBaudrate.value = settingsStore.gpsBaudrate
  ntpServer.value = settingsStore.ntpServer
  ledBrightness.value = settingsStore.ledBrightness
  checkUpdates.value = settingsStore.checkUpdates !== undefined ? settingsStore.checkUpdates : true
  allowPrerelease.value = settingsStore.allowPrerelease || false

  // Load IPv6 settings if available
  if (settingsStore.enableIPv6 !== undefined) {
    enableIPv6.value = settingsStore.enableIPv6
    ipv6Mode.value = settingsStore.ipv6Mode || 'auto'
    ipv6Address.value = settingsStore.ipv6Address || ''
    ipv6PrefixLength.value = settingsStore.ipv6PrefixLength || 64
    ipv6Gateway.value = settingsStore.ipv6Gateway || ''
    ipv6Dns1.value = settingsStore.ipv6Dns1 || ''
    ipv6Dns2.value = settingsStore.ipv6Dns2 || ''
  }
}

// Watch store changes
watch(() => settingsStore.$state, () => {
  loadSettings()
}, { deep: true })

onMounted(async () => {
  await settingsStore.load()
  loadSettings()
  // Check if OTA password is set
  checkOtaPasswordSet()
})

const checkOtaPasswordSet = async () => {
  try {
    const response = await axios.get('/api/ota-password-status')
    otaPasswordSet.value = response.data.isSet
  } catch (e) {
    // Assume not set on error
    otaPasswordSet.value = false
  }
}

const onOtaPasswordSet = () => {
  otaPasswordSet.value = true
}

const clearOtaPassword = async () => {
  if (!confirm(t('settings.clearOtaPasswordConfirm') || 'Are you sure you want to remove the OTA password? Firmware updates will not be possible until a new password is set.')) {
    return
  }

  try {
    await axios.post('/api/clear-ota-password')
    otaPasswordSet.value = false
    alert(t('settings.clearOtaPasswordSuccess') || 'OTA password has been removed.')
  } catch (error) {
    alert(t('settings.clearOtaPasswordError') || 'Failed to remove OTA password: ' + (error.response?.data?.error || error.message))
  }
}

const handleFileSelect = (event) => {
  restoreFile.value = event.target.files[0]
}

const saveSettingsClick = async () => {
  v$.value.$touch()
  if (v$.value.$error) return

  showError.value = false
  showSuccess.value = false

  try {
    const settings = {
      hostname: hostname.value,
      useDHCP: useDHCP.value,
      localIP: localIP.value,
      netmask: netmask.value,
      gateway: gateway.value,
      dns1: dns1.value,
      dns2: dns2.value,
      timesource: timesource.value,
      dcfOffset: dcfOffset.value,
      gpsBaudrate: gpsBaudrate.value,
      ntpServer: ntpServer.value,
      ledBrightness: ledBrightness.value,
      checkUpdates: checkUpdates.value,
      allowPrerelease: allowPrerelease.value,
      // IPv6 settings
      enableIPv6: enableIPv6.value,
      ipv6Mode: ipv6Mode.value,
      ipv6Address: ipv6Address.value,
      ipv6PrefixLength: ipv6PrefixLength.value,
      ipv6Gateway: ipv6Gateway.value,
      ipv6Dns1: ipv6Dns1.value,
      ipv6Dns2: ipv6Dns2.value
    }

    await settingsStore.save(settings)
    showSuccess.value = true
  } catch (error) {
    showError.value = true
  }
}

const downloadBackup = async () => {
  try {
    const response = await axios.get('/api/backup', { responseType: 'blob' })
    const url = window.URL.createObjectURL(new Blob([response.data]))
    const link = document.createElement('a')
    link.href = url
    link.setAttribute('download', 'settings.json')
    document.body.appendChild(link)
    link.click()
    document.body.removeChild(link)
    window.URL.revokeObjectURL(url)
  } catch (error) {
    console.error('Backup download failed:', error)
    alert(t('settings.backupError'))
  }
}

const restoreSettings = async () => {
  if (!restoreFile.value) return

  if (!confirm(t('settings.restoreConfirm'))) return

  try {
    const reader = new FileReader()
    reader.onload = async (e) => {
      try {
        const json = JSON.parse(e.target.result)
        await axios.post('/api/restore', json)
        alert(t('settings.restoreSuccess'))
        window.location.reload()
      } catch (err) {
        alert(t('settings.restoreError') + ': ' + err.message)
      }
    }
    reader.readAsText(restoreFile.value)
  } catch (e) {
    alert(t('settings.restoreError'))
  }
}
</script>

<style scoped>
.settings-page {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-lg);
}

/* Tabs Navigation */
.settings-tabs {
  display: flex;
  gap: var(--spacing-sm);
  overflow-x: auto;
  padding: var(--spacing-xs);
}

.tab-button {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
  padding: var(--spacing-md) var(--spacing-lg);
  background: var(--color-surface);
  border: 2px solid var(--color-border);
  border-radius: var(--radius-lg);
  cursor: pointer;
  transition: all var(--transition-fast);
  white-space: nowrap;
  font-weight: 500;
  color: var(--color-text-secondary);
}

.tab-button:hover {
  border-color: var(--color-primary);
  color: var(--color-primary);
}

.tab-button.active {
  background: linear-gradient(135deg, var(--color-primary) 0%, var(--color-primary-dark) 100%);
  border-color: var(--color-primary);
  color: white;
}

.tab-icon {
  font-size: 1.25rem;
}

.tab-label {
  font-size: 0.9375rem;
}

/* Tab Content */
.settings-content {
  background: var(--color-surface);
  border-radius: var(--radius-lg);
  box-shadow: var(--shadow-md);
  overflow: hidden;
}

.tab-panel {
  padding: var(--spacing-lg);
}

/* Settings Section */
.settings-section {
  margin-bottom: var(--spacing-xl);
}

.settings-section:last-child {
  margin-bottom: 0;
}

.section-header {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  margin-bottom: var(--spacing-lg);
  padding-bottom: var(--spacing-md);
  border-bottom: 1px solid var(--color-border);
}

.section-icon {
  font-size: 1.5rem;
  filter: drop-shadow(0 2px 4px rgba(0, 0, 0, 0.1));
}

.section-title {
  font-size: 1.125rem;
  font-weight: 600;
  color: var(--color-text);
  margin: 0;
}

/* Form Grid */
.form-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
  gap: var(--spacing-md);
}

.form-group {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-xs);
}

.form-group.full-width {
  grid-column: 1 / -1;
}

.form-group label {
  font-size: 0.875rem;
  font-weight: 500;
  color: var(--color-text-secondary);
}

/* Toggle Group */
.toggle-group {
  display: flex;
  gap: var(--spacing-xs);
  background: var(--color-bg);
  padding: var(--spacing-xs);
  border-radius: var(--radius-md);
}

.toggle-btn {
  flex: 1;
  padding: var(--spacing-sm) var(--spacing-md);
  background: transparent;
  border: none;
  border-radius: var(--radius-sm);
  font-weight: 500;
  font-size: 0.875rem;
  color: var(--color-text-secondary);
  cursor: pointer;
  transition: all var(--transition-fast);
}

.toggle-btn:hover {
  color: var(--color-text);
}

.toggle-btn.active {
  background: var(--color-surface);
  color: var(--color-primary);
  box-shadow: var(--shadow-sm);
}

/* Time Source Selector */
.time-source-selector {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
  gap: var(--spacing-md);
}

.time-source-btn {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: var(--spacing-sm);
  padding: var(--spacing-lg);
  background: var(--color-bg);
  border: 2px solid var(--color-border);
  border-radius: var(--radius-lg);
  cursor: pointer;
  transition: all var(--transition-fast);
}

.time-source-btn:hover {
  border-color: var(--color-primary);
  transform: translateY(-2px);
}

.time-source-btn.active {
  background: linear-gradient(135deg, var(--color-primary-light) 0%, var(--color-primary) 100%);
  border-color: var(--color-primary);
}

.time-icon {
  font-size: 2rem;
}

.time-label {
  font-weight: 500;
  font-size: 0.9375rem;
}

/* Brightness Slider */
.brightness-selector {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
}

.brightness-slider {
  flex: 1;
  height: 8px;
  border-radius: var(--radius-full);
  background: var(--color-bg);
  outline: none;
  -webkit-appearance: none;
}

.brightness-slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 20px;
  height: 20px;
  border-radius: 50%;
  background: var(--color-primary);
  cursor: pointer;
  box-shadow: var(--shadow-sm);
}

.brightness-slider::-moz-range-thumb {
  width: 20px;
  height: 20px;
  border-radius: 50%;
  background: var(--color-primary);
  cursor: pointer;
  border: none;
  box-shadow: var(--shadow-sm);
}

.brightness-value {
  min-width: 50px;
  text-align: center;
  font-weight: 600;
  color: var(--color-primary);
}

/* Checkbox */
.checkbox-label {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  cursor: pointer;
  font-weight: 500;
}

.custom-checkbox {
  width: 20px;
  height: 20px;
  border: 2px solid var(--color-border);
  border-radius: var(--radius-sm);
  cursor: pointer;
  transition: all var(--transition-fast);
}

.custom-checkbox:checked {
  background: var(--color-primary);
  border-color: var(--color-primary);
}

/* Backup Cards */
.backup-actions {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: var(--spacing-lg);
}

.backup-card {
  display: flex;
  align-items: flex-start;
  gap: var(--spacing-md);
  padding: var(--spacing-lg);
  background: var(--color-bg);
  border-radius: var(--radius-lg);
  border: 1px solid var(--color-border);
}

.backup-icon {
  font-size: 2.5rem;
  filter: drop-shadow(0 2px 4px rgba(0, 0, 0, 0.1));
}

.backup-content {
  flex: 1;
}

.backup-content h4 {
  font-size: 1rem;
  font-weight: 600;
  margin: 0 0 var(--spacing-xs) 0;
}

.backup-content p {
  font-size: 0.875rem;
  color: var(--color-text-secondary);
  margin: 0 0 var(--spacing-md) 0;
}

.file-input {
  display: none;
}

/* Security Actions */
.security-actions {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: var(--spacing-lg);
}

.security-action-card {
  display: flex;
  align-items: flex-start;
  gap: var(--spacing-md);
  padding: var(--spacing-lg);
  background: var(--color-bg);
  border-radius: var(--radius-lg);
  border: 1px solid var(--color-border);
  transition: all var(--transition-fast);
}

.security-action-card:hover {
  border-color: var(--color-primary);
  box-shadow: var(--shadow-md);
}

.security-icon {
  font-size: 2.5rem;
  filter: drop-shadow(0 2px 4px rgba(0, 0, 0, 0.1));
}

.security-content {
  flex: 1;
}

.security-content h4 {
  font-size: 1rem;
  font-weight: 600;
  margin: 0 0 var(--spacing-xs) 0;
}

.security-content p {
  font-size: 0.875rem;
  color: var(--color-text-secondary);
  margin: 0 0 var(--spacing-md) 0;
}

.security-content .btn {
  display: inline-flex;
  align-items: center;
  gap: var(--spacing-sm);
}

.security-actions-buttons {
  display: flex;
  gap: var(--spacing-sm);
  flex-wrap: wrap;
}

.btn-icon {
  font-size: 1.125rem;
}

/* Footer */
.settings-footer {
  position: sticky;
  bottom: 0;
  background: var(--color-surface);
  border-top: 1px solid var(--color-border);
  padding: var(--spacing-md) var(--spacing-lg);
  box-shadow: 0 -4px 6px -1px rgba(0, 0, 0, 0.05);
  z-index: 10;
}

.footer-content {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: var(--spacing-md);
  flex-wrap: wrap;
}

.footer-alert {
  margin: 0;
  flex: 1;
}

.save-btn {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
  padding: var(--spacing-md) var(--spacing-xl);
  font-weight: 600;
}

.save-icon {
  font-size: 1.125rem;
}

/* Mobile adjustments */
@media (max-width: 640px) {
  .settings-tabs {
    flex-wrap: nowrap;
  }

  .tab-button {
    flex: 1;
    justify-content: center;
    padding: var(--spacing-sm) var(--spacing-md);
  }

  .tab-label {
    display: none;
  }

  .footer-content {
    flex-direction: column;
    align-items: stretch;
  }

  .save-btn {
    width: 100%;
    justify-content: center;
  }
}
</style>
