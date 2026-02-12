<template>
  <div class="settings-page">
    <!-- Tabs Navigation (iOS Style Segmented Control) -->
    <div class="tabs-container">
      <div class="segmented-control">
        <button
          v-for="tab in tabs"
          :key="tab.id"
          :class="['segment-btn', { active: activeTab === tab.id }]"
          @click="activeTab = tab.id"
        >
          <span class="segment-label">{{ tab.label }}</span>
        </button>
      </div>
    </div>

    <!-- Tab Content -->
    <div class="settings-content">
      <!-- General Tab -->
      <Transition name="fade" mode="out-in">
        <div v-show="activeTab === 'general'" class="tab-panel">
          <div class="settings-card">
            <div class="card-header">
              <h3>{{ t('settings.security') }}</h3>
            </div>
            <div class="card-body">
              <div class="security-item">
                <div class="security-info">
                  <h4>{{ t('settings.changePassword') }}</h4>
                  <p>{{ t('settings.changePasswordHint') }}</p>
                </div>
                <BButton variant="primary" @click="showPasswordModal = true">
                  {{ t('settings.changePasswordBtn') }}
                </BButton>
              </div>
            </div>
          </div>

          <div class="settings-card">
            <div class="card-header">
              <h3>{{ t('settings.ccuSettings') }}</h3>
            </div>
            <div class="card-body">
              <div class="form-group">
                <label class="form-label">{{ t('settings.ccuIpAddress') }}</label>
                <BFormInput
                  type="text"
                  v-model="ccuIP"
                  trim
                  placeholder="192.168.x.x"
                  :state="v$.ccuIP.$error ? false : null"
                />
                <div class="form-text text-warning mt-2">
                  <small>{{ t('settings.ccuIpHint') }}</small>
                </div>
              </div>
            </div>
          </div>

          <div class="settings-card">
            <div class="card-header">
              <h3>{{ t('settings.systemSettings') }}</h3>
            </div>
            <div class="card-body">
              <div class="form-group">
                <label class="form-label">{{ t('settings.ledBrightness') }}</label>
                <div class="brightness-control">
                  <span class="brightness-icon">🔅</span>
                  <input
                    type="range"
                    v-model.number="ledBrightness"
                    min="0"
                    max="100"
                    step="5"
                    class="ios-slider"
                  />
                  <span class="brightness-icon">🔆</span>
                  <span class="brightness-value">{{ ledBrightness }}%</span>
                </div>
              </div>

              <div class="form-group mt-3">
                <div class="switch-row">
                  <label class="switch-label">{{ t('settings.updateLedBlink') }}</label>
                  <div class="form-check form-switch">
                    <input class="form-check-input" type="checkbox" v-model="updateLedBlink">
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
      </Transition>

      <!-- Network Tab -->
      <Transition name="fade" mode="out-in">
        <div v-show="activeTab === 'network'" class="tab-panel">
          <div class="settings-card">
            <div class="card-header">
              <h3>{{ t('settings.networkSettings') }}</h3>
            </div>
            <div class="card-body">
              <div class="form-group mb-3">
                <label class="form-label">{{ t('settings.hostname') }}</label>
                <BFormInput
                  type="text"
                  v-model="hostname"
                  trim
                  :state="v$.hostname.$error ? false : null"
                />
              </div>

              <div class="form-group mb-3">
                <div class="switch-row">
                  <label class="switch-label">{{ t('settings.dhcp') }}</label>
                  <div class="form-check form-switch">
                    <input class="form-check-input" type="checkbox" v-model="useDHCP">
                  </div>
                </div>
              </div>

              <div v-if="!useDHCP" class="manual-network-settings">
                <div class="row g-3">
                  <div class="col-md-6">
                    <label class="form-label">{{ t('settings.ipAddress') }}</label>
                    <BFormInput type="text" v-model="localIP" trim :state="v$.localIP.$error ? false : null" />
                  </div>
                  <div class="col-md-6">
                    <label class="form-label">{{ t('settings.netmask') }}</label>
                    <BFormInput type="text" v-model="netmask" trim :state="v$.netmask.$error ? false : null" />
                  </div>
                  <div class="col-md-6">
                    <label class="form-label">{{ t('settings.gateway') }}</label>
                    <BFormInput type="text" v-model="gateway" trim :state="v$.gateway.$error ? false : null" />
                  </div>
                  <div class="col-md-6">
                    <label class="form-label">{{ t('settings.dns1') }}</label>
                    <BFormInput type="text" v-model="dns1" trim :state="v$.dns1.$error ? false : null" />
                  </div>
                  <div class="col-md-6">
                    <label class="form-label">{{ t('settings.dns2') }}</label>
                    <BFormInput type="text" v-model="dns2" trim :state="v$.dns2.$error ? false : null" />
                  </div>
                </div>
              </div>
            </div>
          </div>

          <div class="settings-card">
            <div class="card-header">
              <h3>{{ t('settings.ipv6Settings') }}</h3>
            </div>
            <div class="card-body">
              <div class="form-group">
                <div class="switch-row">
                  <label class="switch-label">{{ t('settings.enableIPv6') }}</label>
                  <div class="form-check form-switch">
                    <input class="form-check-input" type="checkbox" v-model="enableIPv6">
                  </div>
                </div>
              </div>

              <template v-if="enableIPv6">
                <div class="form-group mt-3">
                  <label class="form-label">{{ t('settings.ipv6Mode') }}</label>
                  <div class="mode-selector">
                    <button
                      :class="['mode-btn', { active: ipv6Mode === 'auto' }]"
                      @click="ipv6Mode = 'auto'"
                    >{{ t('settings.ipv6Auto') }}</button>
                    <button
                      :class="['mode-btn', { active: ipv6Mode === 'static' }]"
                      @click="ipv6Mode = 'static'"
                    >{{ t('settings.ipv6Static') }}</button>
                  </div>
                </div>

                <template v-if="ipv6Mode === 'static'">
                  <div class="row g-3 mt-2">
                    <div class="col-12">
                      <label class="form-label">{{ t('settings.ipv6Address') }}</label>
                      <BFormInput
                        type="text"
                        v-model="ipv6Address"
                        trim
                        placeholder="2001:db8::1"
                        :state="v$.ipv6Address.$error ? false : null"
                      />
                    </div>
                    <div class="col-md-4">
                      <label class="form-label">{{ t('settings.ipv6PrefixLength') }}</label>
                      <BFormInput
                        type="number"
                        v-model.number="ipv6PrefixLength"
                        min="1"
                        max="128"
                        placeholder="64"
                        :state="v$.ipv6PrefixLength.$error ? false : null"
                      />
                    </div>
                    <div class="col-md-8">
                      <label class="form-label">{{ t('settings.ipv6Gateway') }}</label>
                      <BFormInput
                        type="text"
                        v-model="ipv6Gateway"
                        trim
                        placeholder="2001:db8::1"
                        :state="v$.ipv6Gateway.$error ? false : null"
                      />
                    </div>
                    <div class="col-md-6">
                      <label class="form-label">{{ t('settings.ipv6Dns1') }}</label>
                      <BFormInput
                        type="text"
                        v-model="ipv6Dns1"
                        trim
                        placeholder="2001:4860:4860::8888"
                        :state="v$.ipv6Dns1.$error ? false : null"
                      />
                    </div>
                    <div class="col-md-6">
                      <label class="form-label">{{ t('settings.ipv6Dns2') }}</label>
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
        </div>
      </Transition>

      <!-- Time Tab -->
      <Transition name="fade" mode="out-in">
        <div v-show="activeTab === 'time'" class="tab-panel">
          <div class="settings-card">
            <div class="card-header">
              <h3>{{ t('settings.timeSettings') }}</h3>
            </div>
            <div class="card-body">
              <label class="form-label mb-3">{{ t('settings.timesource') }}</label>
              <div class="source-selector">
                <button
                  v-for="source in timeSources"
                  :key="source.value"
                  :class="['source-card', { active: timesource === source.value }]"
                  @click="timesource = source.value"
                >
                  <span class="source-icon">{{ source.icon }}</span>
                  <span class="source-label">{{ source.label }}</span>
                  <span v-if="timesource === source.value" class="check-icon">✓</span>
                </button>
              </div>

              <div v-if="isNtpActivated" class="form-group mt-4">
                <label class="form-label">{{ t('settings.ntpServer') }}</label>
                <BFormInput
                  type="text"
                  v-model="ntpServer"
                  trim
                  :state="v$.ntpServer.$error ? false : null"
                />
              </div>
              <div v-if="isDcfActivated" class="form-group mt-4">
                <label class="form-label">{{ t('settings.dcfOffset') }} (µs)</label>
                <BFormInput
                  type="number"
                  v-model.number="dcfOffset"
                  min="0"
                  :state="v$.dcfOffset.$error ? false : null"
                />
              </div>
              <div v-if="isGpsActivated" class="form-group mt-4">
                <label class="form-label">{{ t('settings.gpsBaudrate') }}</label>
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
        </div>
      </Transition>

      <!-- Backup Tab -->
      <Transition name="fade" mode="out-in">
        <div v-show="activeTab === 'backup'" class="tab-panel">
          <div class="settings-card">
            <div class="card-header">
              <h3>{{ t('settings.backupRestore') }}</h3>
            </div>
            <div class="card-body">
              <div class="backup-grid">
                <div class="action-tile" @click="downloadBackup">
                  <div class="tile-icon">⬇️</div>
                  <div class="tile-content">
                    <h4>{{ t('settings.downloadBackup') }}</h4>
                    <p>{{ t('settings.backupInfo') }}</p>
                  </div>
                  <span class="tile-arrow">➜</span>
                </div>

                <div class="action-tile" @click="$refs.fileInput.click()">
                  <div class="tile-icon">⬆️</div>
                  <div class="tile-content">
                    <h4>{{ t('settings.restore') }}</h4>
                    <p>{{ t('settings.restoreInfo') }}</p>
                  </div>
                  <span class="tile-arrow">➜</span>
                  <input
                    type="file"
                    ref="fileInput"
                    accept=".json"
                    @change="handleFileSelect"
                    class="file-input"
                  />
                </div>

                <div v-if="restoreFile" class="restore-confirm mt-3 p-3 bg-light rounded">
                  <p class="mb-2">Selected: <strong>{{ restoreFile.name }}</strong></p>
                  <BButton variant="warning" class="w-100" @click="restoreSettings">
                    {{ t('settings.restoreBtn') }}
                  </BButton>
                </div>
              </div>
            </div>
          </div>
        </div>
      </Transition>
    </div>

    <!-- Floating Action Bar for Save -->
    <Transition name="slide-up">
      <div class="floating-footer">
        <div class="footer-container">
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
            {{ t('common.save') }}
          </BButton>
        </div>
      </div>
    </Transition>

    <!-- Password Change Modal -->
    <PasswordChangeModal v-model="showPasswordModal" :force-redirect="false" />

    <!-- Restart Confirmation Modal -->
    <BModal
      v-model="showRestartModal"
      :title="t('settings.restartTitle')"
      centered
      no-fade
    >
      <p>{{ t('settings.restartMessage') }}</p>
      <template #footer>
        <BButton variant="secondary" @click="showRestartModal = false">
          {{ t('settings.restartLater') }}
        </BButton>
        <BButton variant="danger" @click="performRestart" :disabled="isRestarting">
          <span v-if="isRestarting" class="spinner-border spinner-border-sm me-2"></span>
          {{ t('settings.restartNow') }}
        </BButton>
      </template>
    </BModal>
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

import { useRoute, useRouter } from 'vue-router'

const { t } = useI18n()
const settingsStore = useSettingsStore()
const loginStore = useLoginStore()
const route = useRoute()
const router = useRouter()

// Password modal
const showPasswordModal = ref(false)

// Tab management
const activeTab = ref(route.query.tab || 'general')

// Sync URL with tab change
watch(activeTab, (newTab) => {
  router.replace({ query: { ...route.query, tab: newTab } })
})

// Sync tab with URL change
watch(() => route.query.tab, (newTab) => {
  if (newTab && tabs.value.some(t => t.id === newTab)) {
    activeTab.value = newTab
  }
})

const tabs = computed(() => [
  { id: 'general', label: t('settings.tabGeneral') },
  { id: 'network', label: t('settings.tabNetwork') },
  { id: 'time', label: t('settings.tabTime') },
  { id: 'backup', label: t('settings.tabBackup') }
])

const timeSources = computed(() => [
  { value: 0, icon: '🌍', label: t('settings.ntp') },
  { value: 1, icon: '📻', label: t('settings.dcf') },
  { value: 2, icon: '🛰️', label: t('settings.gps') }
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
const ccuIP = ref('')

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
const updateLedBlink = ref(true)

const showError = ref(null)
const showRestartModal = ref(false)
const isRestarting = ref(false)

// Computed flags
const isNtpActivated = computed(() => timesource.value === 0)
const isDcfActivated = computed(() => timesource.value === 1)
const isGpsActivated = computed(() => timesource.value === 2)
const isIPv6Static = computed(() => enableIPv6.value && ipv6Mode.value === 'static')

const hostname_validator = helpers.regex(/^[a-zA-Z0-9][a-zA-Z0-9.-]{0,62}$/)
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
  ccuIP: {
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
  ccuIP,
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
  ccuIP.value = settingsStore.ccuIP || ''
  timesource.value = settingsStore.timesource
  dcfOffset.value = settingsStore.dcfOffset
  gpsBaudrate.value = settingsStore.gpsBaudrate
  ntpServer.value = settingsStore.ntpServer
  ledBrightness.value = settingsStore.ledBrightness
  updateLedBlink.value = settingsStore.updateLedBlink !== undefined ? settingsStore.updateLedBlink : true

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
})

const handleFileSelect = (event) => {
  restoreFile.value = event.target.files[0]
}

const saveSettingsClick = async () => {
  v$.value.$touch()
  if (v$.value.$error) return

  showError.value = false

  try {
    const settings = {
      hostname: hostname.value,
      useDHCP: useDHCP.value,
      localIP: localIP.value,
      netmask: netmask.value,
      gateway: gateway.value,
      dns1: dns1.value,
      dns2: dns2.value,
      ccuIP: ccuIP.value,
      timesource: timesource.value,
      dcfOffset: dcfOffset.value,
      gpsBaudrate: gpsBaudrate.value,
      ntpServer: ntpServer.value,
      ledBrightness: ledBrightness.value,
      updateLedBlink: updateLedBlink.value,
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
    showRestartModal.value = true
  } catch (error) {
    showError.value = true
  }
}

const performRestart = async () => {
  isRestarting.value = true
  try {
    await axios.post('/api/restart')
    showRestartModal.value = false
    alert(t('firmware.restartingText'))
    window.location.reload()
  } catch (e) {
    console.error("Restart request failed", e)
    alert("Error restarting device")
    isRestarting.value = false
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
  padding-bottom: 80px; /* Space for floating footer */
}

/* iOS Segmented Control */
.tabs-container {
  display: flex;
  justify-content: center;
  margin-bottom: var(--spacing-xl);
}

.segmented-control {
  display: inline-flex;
  background-color: var(--color-border-light);
  padding: 4px;
  border-radius: var(--radius-lg);
  position: relative;
  width: 100%;
  max-width: 600px;
}

.segment-btn {
  flex: 1;
  border: none;
  background: transparent;
  padding: 8px 16px;
  border-radius: var(--radius-md);
  font-weight: 600;
  font-size: 0.9375rem;
  color: var(--color-text-secondary);
  cursor: pointer;
  transition: all 0.2s ease;
  position: relative;
  z-index: 1;
  text-align: center;
}

.segment-btn.active {
  background-color: var(--color-surface);
  color: var(--color-text);
  box-shadow: 0 2px 8px rgba(0,0,0,0.12);
}

/* Settings Cards (Apple Style Grouped) */
.settings-card {
  background: var(--color-surface);
  border-radius: var(--radius-xl);
  box-shadow: var(--shadow-md);
  margin-bottom: var(--spacing-lg);
  overflow: hidden;
}

.card-header {
  padding: var(--spacing-lg) var(--spacing-lg) 0;
  background: transparent;
  border: none;
}

.card-header h3 {
  font-size: 1.125rem;
  margin: 0;
  color: var(--color-text-secondary);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

.card-body {
  padding: var(--spacing-lg);
}

/* Security Items */
.security-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: var(--spacing-sm) 0;
}

.security-info h4 {
  margin: 0;
  font-size: 1rem;
}

.security-info p {
  margin: 0;
  font-size: 0.875rem;
  color: var(--color-text-secondary);
}

hr {
  margin: var(--spacing-md) 0;
  border-color: var(--color-border);
}

/* Controls */
.switch-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.switch-label {
  font-size: 1rem;
  font-weight: 500;
}

/* Brightness Slider */
.brightness-control {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  background: var(--color-bg);
  padding: var(--spacing-sm) var(--spacing-md);
  border-radius: var(--radius-lg);
}

.ios-slider {
  flex: 1;
  height: 6px;
  -webkit-appearance: none;
  background: #d1d1d6;
  border-radius: 3px;
}

.ios-slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 28px;
  height: 28px;
  border-radius: 50%;
  background: white;
  box-shadow: 0 2px 6px rgba(0,0,0,0.2);
  cursor: pointer;
}

/* Mode Selector */
.mode-selector {
  display: flex;
  background: var(--color-bg);
  padding: 4px;
  border-radius: var(--radius-lg);
  margin-top: var(--spacing-xs);
}

.mode-btn {
  flex: 1;
  border: none;
  background: transparent;
  padding: 8px;
  border-radius: var(--radius-md);
  font-weight: 500;
  transition: all 0.2s;
}

.mode-btn.active {
  background: var(--color-surface);
  box-shadow: var(--shadow-sm);
  color: var(--color-primary);
}

/* Source Cards */
.source-selector {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: var(--spacing-md);
}

.source-card {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: var(--spacing-lg);
  background: var(--color-bg);
  border: 2px solid transparent;
  border-radius: var(--radius-lg);
  cursor: pointer;
  position: relative;
  transition: all 0.2s;
}

.source-card.active {
  background: var(--color-surface);
  border-color: var(--color-primary);
  box-shadow: var(--shadow-md);
}

.source-icon {
  font-size: 2rem;
  margin-bottom: var(--spacing-xs);
}

.check-icon {
  position: absolute;
  top: 8px;
  right: 8px;
  color: var(--color-primary);
  font-weight: bold;
}

/* Action Tiles */
.backup-grid {
  display: grid;
  gap: var(--spacing-md);
}

.action-tile {
  display: flex;
  align-items: center;
  padding: var(--spacing-md);
  background: var(--color-bg);
  border-radius: var(--radius-lg);
  cursor: pointer;
  transition: background 0.2s;
}

.action-tile:hover {
  background: var(--color-border-light);
}

.tile-icon {
  font-size: 2rem;
  margin-right: var(--spacing-md);
}

.tile-content {
  flex: 1;
}

.tile-content h4 {
  font-size: 1rem;
  margin: 0;
}

.tile-content p {
  margin: 0;
  font-size: 0.8125rem;
  color: var(--color-text-secondary);
}

.tile-arrow {
  color: var(--color-text-secondary);
  font-size: 1.25rem;
}

.file-input {
  display: none;
}

/* Floating Footer */
.floating-footer {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  padding: var(--spacing-md);
  background: linear-gradient(to top, var(--color-surface) 80%, transparent);
  display: flex;
  justify-content: center;
  z-index: 100;
  pointer-events: none;
}

.footer-container {
  width: 100%;
  max-width: 600px;
  pointer-events: auto;
  display: flex;
  gap: var(--spacing-md);
  align-items: center;
}

.save-btn {
  flex: 1;
  box-shadow: var(--shadow-lg);
}

.footer-alert {
  flex: 1;
  margin: 0;
  box-shadow: var(--shadow-lg);
}

/* Success Overlay */
.success-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0,0,0,0.2);
  backdrop-filter: blur(4px);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
}

.success-card {
  background: var(--color-surface);
  padding: var(--spacing-xl);
  border-radius: var(--radius-xl);
  text-align: center;
  box-shadow: var(--shadow-xl);
  min-width: 300px;
}

.success-icon {
  font-size: 3rem;
  color: var(--color-success);
  margin-bottom: var(--spacing-md);
}

/* Transitions */
.fade-enter-active,
.fade-leave-active {
  transition: opacity 0.2s ease;
}

.fade-enter-from,
.fade-leave-to {
  opacity: 0;
}

.slide-up-enter-active,
.slide-up-leave-active {
  transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);
}

.slide-up-enter-from,
.slide-up-leave-to {
  transform: translateY(100%);
}

/* ===== Mobile Responsive ===== */
@media (max-width: 768px) {
  .settings-page {
    padding-bottom: 100px;
  }

  .tabs-container {
    margin-bottom: var(--spacing-lg);
    margin-left: calc(-1 * var(--spacing-sm));
    margin-right: calc(-1 * var(--spacing-sm));
    padding: 0 var(--spacing-sm);
    overflow-x: auto;
    -webkit-overflow-scrolling: touch;
    scrollbar-width: none;
  }

  .tabs-container::-webkit-scrollbar {
    display: none;
  }

  .segmented-control {
    min-width: max-content;
    width: auto;
    flex-wrap: nowrap;
  }

  .segment-btn {
    padding: 10px 16px;
    font-size: 0.875rem;
    white-space: nowrap;
    flex: 0 0 auto;
  }

  .settings-card {
    border-radius: var(--radius-lg);
    margin-bottom: var(--spacing-md);
  }

  .card-header {
    padding: var(--spacing-md) var(--spacing-md) 0;
  }

  .card-body {
    padding: var(--spacing-md);
  }

  .security-item {
    flex-direction: column;
    align-items: flex-start;
    gap: var(--spacing-sm);
  }

  .security-item .btn {
    width: 100%;
  }

  .source-selector {
    grid-template-columns: 1fr;
    gap: var(--spacing-sm);
  }

  .source-card {
    flex-direction: row;
    padding: var(--spacing-md);
    gap: var(--spacing-md);
  }

  .source-icon {
    font-size: 1.5rem;
    margin-bottom: 0;
  }

  .brightness-control {
    flex-wrap: wrap;
    gap: var(--spacing-sm);
  }

  .brightness-value {
    min-width: 40px;
    text-align: right;
  }

  .floating-footer {
    padding: var(--spacing-sm) var(--spacing-md);
  }

  .footer-container {
    flex-direction: column;
  }

  .success-card {
    min-width: 0;
    width: 90%;
    max-width: 300px;
  }
}

@media (max-width: 576px) {
  .segment-btn {
    padding: 8px 14px;
    font-size: 0.8125rem;
  }
}
</style>
