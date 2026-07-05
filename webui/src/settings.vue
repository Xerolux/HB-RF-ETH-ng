<template>
  <div class="settings-page page-shell">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow">
          <AppIcon name="settings" />
          {{ t('nav.settings') }}
        </span>
        <h1 class="hero-title">{{ t('nav.settings') }}</h1>
        <p class="hero-subtitle">{{ t('settings.networkSettings') }} · {{ t('settings.timeSettings') }} · {{ t('settings.backupRestore') }}</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="shield" /> {{ t('settings.security') }}</span>
        <span class="meta-chip"><AppIcon name="network" /> IPv4 / IPv6</span>
        <span class="meta-chip"><AppIcon name="backup" /> Backup</span>
        <span class="meta-chip"><AppIcon name="alert" /> {{ t('settings.tabExperimental') }}</span>
        <span class="meta-chip" :class="hasUnsavedChanges ? 'warning-chip' : ''">
          <AppIcon :name="hasUnsavedChanges ? 'alert' : 'check'" />
          {{ hasUnsavedChanges ? t('settings.unsavedChanges') : t('settings.allSaved') }}
        </span>
      </div>
    </section>

    <!-- Tabs Navigation (iOS Style Segmented Control) -->
    <div class="tabs-container">
      <div class="segmented-control">
        <button
          v-for="tab in tabs"
          :key="tab.id"
          :class="['segment-btn', { active: activeTab === tab.id }]"
          @click="activeTab = tab.id"
        >
          <AppIcon :name="tab.icon" />
          <span class="segment-label">{{ tab.label }}</span>
        </button>
      </div>
    </div>

    <!-- Tab Content -->
    <div class="settings-content">
      <!-- General Tab -->
      <Transition name="fade" mode="out-in">
        <div v-if="activeTab === 'general'" class="tab-panel">
          <div class="settings-card">
            <div class="card-header">
              <h3>{{ t('settings.security') }}</h3>
            </div>
            <div class="card-body">
              <div class="security-item">
                <div class="security-info">
                  <h4>{{ t('settings.adminUsername') }}</h4>
                  <p>{{ t('settings.adminUsernameHint') }}</p>
                </div>
                <div class="security-control-row">
                  <BFormInput
                    v-model.trim="adminUsername"
                    class="security-control"
                    type="text"
                    autocomplete="username"
                    autocapitalize="none"
                    spellcheck="false"
                    :state="v$.adminUsername.$error ? false : null"
                  />
                  <BButton
                    variant="primary"
                    class="security-save-btn"
                    @click="saveSettingsClick"
                    :disabled="v$.adminUsername.$error || !adminUsernameChanged"
                  >
                    {{ t('common.save') }}
                  </BButton>
                </div>
              </div>

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
                  placeholder="192.168.x.x or 2001:db8::1"
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
                  <AppIcon name="sun" class="brightness-icon dim" />
                  <input
                    type="range"
                    v-model.number="ledBrightness"
                    min="0"
                    max="100"
                    step="5"
                    class="ios-slider"
                  />
                  <AppIcon name="sun" class="brightness-icon" />
                  <span class="brightness-value">{{ ledBrightness }}%</span>
                </div>
              </div>

              <div class="form-group mt-4">
                <label class="form-label fw-bold">{{ t('settings.ledPrograms') }}</label>
                <div class="led-programs-grid">
                  <div class="led-program-item" v-for="program in ledPrograms" :key="program.id">
                    <label class="form-label small">{{ program.label }}</label>
                    <BFormSelect
                      :modelValue="getLedProgramValue(program.id)"
                      @update:modelValue="setLedProgramValue(program.id, $event)"
                      :options="ledPatterns"
                      size="sm"
                    />
                  </div>
                </div>
                <div class="form-text mt-2">
                  <small>{{ t('settings.ledProgramsHelp') }}</small>
                </div>
              </div>
            </div>
          </div>
        </div>
      </Transition>

      <!-- Network Tab -->
      <Transition name="fade" mode="out-in">
        <div v-if="activeTab === 'network'" class="tab-panel">
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
        <div v-if="activeTab === 'time'" class="tab-panel">
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
                  <span class="source-icon"><AppIcon :name="source.icon" /></span>
                  <span class="source-label">{{ source.label }}</span>
                  <span v-if="timesource === source.value" class="check-icon"><AppIcon name="check" /></span>
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
        <div v-if="activeTab === 'backup'" class="tab-panel">
          <div class="settings-card">
            <div class="card-header">
              <h3>{{ t('settings.backupRestore') }}</h3>
            </div>
            <div class="card-body">
              <div class="backup-grid">
                <div class="action-tile" @click="downloadBackup">
                  <span class="tile-icon"><AppIcon name="download" /></span>
                  <div class="tile-content">
                    <h4>{{ t('settings.downloadBackup') }}</h4>
                    <p>{{ t('settings.backupInfo') }}</p>
                  </div>
                  <span class="tile-arrow"><AppIcon name="arrowRight" /></span>
                </div>

                <div class="action-tile" @click="$refs.fileInput.click()">
                  <span class="tile-icon"><AppIcon name="upload" /></span>
                  <div class="tile-content">
                    <h4>{{ t('settings.restore') }}</h4>
                    <p>{{ t('settings.restoreInfo') }}</p>
                  </div>
                  <span class="tile-arrow"><AppIcon name="arrowRight" /></span>
                  <input
                    type="file"
                    ref="fileInput"
                    accept=".json"
                    @change="handleFileSelect"
                    class="file-input"
                  />
                </div>

                <div v-if="restoreFile" class="restore-confirm mt-3 p-3 bg-light rounded">
                  <p class="mb-2">{{ t('settings.selected', { name: restoreFile.name }) }}</p>
                  <BButton variant="warning" class="w-100" @click="restoreSettings">
                    {{ t('settings.restoreBtn') }}
                  </BButton>
                </div>
              </div>
            </div>
          </div>
        </div>
      </Transition>

      <!-- Experimental Tab -->
      <Transition name="fade" mode="out-in">
        <div v-if="activeTab === 'experimental'" class="tab-panel">
          <div class="settings-card">
            <div class="card-header">
              <h3>{{ t('settings.experimentalTitle') }}</h3>
            </div>
            <div class="card-body">
              <div class="experimental-warning">
                <AppIcon name="alert" />
                <div>
                  <strong>{{ t('settings.experimentalWarningTitle') }}</strong>
                  <p>{{ t('settings.experimentalWarningText') }}</p>
                </div>
              </div>

              <div class="switch-row experimental-switch-row">
                <div class="switch-copy">
                  <h4>{{ t('settings.experimentalDesign') }}</h4>
                  <p>{{ t('settings.experimentalDesignHint') }}</p>
                </div>
                <div class="form-check form-switch">
                  <input
                    class="form-check-input"
                    type="checkbox"
                    :checked="experimentalStore.testDesignEnabled"
                    @change="experimentalStore.setTestDesignEnabled($event.target.checked)"
                  >
                </div>
              </div>

              <div class="switch-row experimental-switch-row">
                <div class="switch-copy">
                  <h4>{{ t('settings.flashPause') }}</h4>
                  <p>{{ t('settings.flashPauseHint') }}</p>
                </div>
                <div class="form-check form-switch">
                  <input
                    class="form-check-input"
                    type="checkbox"
                    v-model="flashPause"
                  />
                </div>
              </div>
            </div>
          </div>
        </div>
      </Transition>
    </div>

    <!-- Floating Action Bar for Save -->
    <Transition name="slide-up">
      <div v-if="hasUnsavedChanges || showError" class="floating-footer">
        <div class="footer-container">
          <BAlert
            variant="danger"
            :model-value="!!showError"
            dismissible
            fade
            @update:model-value="showError = null"
            class="footer-alert"
          >{{ showError || t("settings.saveError") }}</BAlert>

          <BButton
            variant="outline-secondary"
            size="lg"
            @click="resetToLoadedState"
            :disabled="!hasUnsavedChanges"
            class="discard-btn"
          >
            {{ t('settings.discard') }}
          </BButton>

          <BButton
            variant="primary"
            size="lg"
            @click="saveSettingsClick"
            :disabled="v$.$error || !hasUnsavedChanges"
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
      <BAlert
        v-if="settingsStore.flashPause"
        variant="info"
        :model-value="true"
        fade
        class="mt-2 mb-0"
      >
        {{ t('firmware.restartFlashPauseHint') }}
      </BAlert>
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
import { ref, computed, onMounted, onBeforeUnmount, watch } from 'vue'
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
import { useExperimentalStore, useSettingsStore, useLoginStore, useUiStore } from './stores.js'
import PasswordChangeModal from './components/PasswordChangeModal.vue'

import { useRoute, useRouter, onBeforeRouteLeave } from 'vue-router'

const { t } = useI18n()
const settingsStore = useSettingsStore()
const loginStore = useLoginStore()
const uiStore = useUiStore()
const experimentalStore = useExperimentalStore()
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
  { id: 'general', label: t('settings.tabGeneral'), icon: 'shield' },
  { id: 'network', label: t('settings.tabNetwork'), icon: 'network' },
  { id: 'time', label: t('settings.tabTime'), icon: 'clock' },
  { id: 'backup', label: t('settings.tabBackup'), icon: 'backup' },
  { id: 'experimental', label: t('settings.tabExperimental'), icon: 'alert' }
])

const timeSources = computed(() => [
  { value: 0, icon: 'globe', label: t('settings.ntp') },
  { value: 1, icon: 'radio', label: t('settings.dcf') },
  { value: 2, icon: 'satellite', label: t('settings.gps') }
])

// Local form state
const restoreFile = ref(null)
const fileInput = ref(null)
const adminUsername = ref('admin')
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
const flashPause = ref(false)

// LED Programs
const ledPrograms = computed(() => [
  { id: 'idle', label: t('settings.ledProgramIdle') },
  { id: 'ccu_disconnected', label: t('settings.ledProgramCcuDisconnected') },
  { id: 'ccu_connected', label: t('settings.ledProgramCcuConnected') },
  { id: 'update_available', label: t('settings.ledProgramUpdateAvailable') },
  { id: 'error', label: t('settings.ledProgramError') },
  { id: 'booting', label: t('settings.ledProgramBooting') },
  { id: 'update_in_progress', label: t('settings.ledProgramUpdateInProgress') }
])

const ledPatterns = computed(() => [
  { value: 0, text: t('settings.ledPatternOff') },
  { value: 1, text: t('settings.ledPatternOn') },
  { value: 2, text: t('settings.ledPatternBlink') },
  { value: 3, text: t('settings.ledPatternBlinkInv') },
  { value: 4, text: t('settings.ledPatternFastBlink') },
  { value: 5, text: t('settings.ledPatternSlowBlink') },
  { value: 6, text: t('settings.ledPatternBlink2x') },
  { value: 7, text: t('settings.ledPatternBlink3x') },
  { value: 8, text: t('settings.ledPatternBreathing') },
  { value: 9, text: t('settings.ledPatternHeartbeat') },
  { value: 10, text: t('settings.ledPatternStrobe') }
])

const ledProgramValues = ref({
  idle: 1,
  ccu_disconnected: 5,
  ccu_connected: 6,
  update_available: 4,
  error: 10,
  booting: 4,
  update_in_progress: 5
})

const getLedProgramValue = (programId) => {
  return ledProgramValues.value[programId] ?? 1
}

const setLedProgramValue = (programId, value) => {
  ledProgramValues.value[programId] = parseInt(value)
}

const showError = ref(null)  // Can be null or a string with error message
const showRestartModal = ref(false)
const isRestarting = ref(false)
const loadedSnapshot = ref('')

// Computed flags
const isNtpActivated = computed(() => timesource.value === 0)
const isDcfActivated = computed(() => timesource.value === 1)
const isGpsActivated = computed(() => timesource.value === 2)
const isIPv6Static = computed(() => enableIPv6.value && ipv6Mode.value === 'static')

const hostname_validator = helpers.regex(/^[a-zA-Z0-9][a-zA-Z0-9.-]{0,62}$/)
const username_validator = helpers.regex(/^[a-zA-Z0-9._-]{1,32}$/)
const domainname_validator = helpers.regex(/^([a-zA-Z0-9_-]{1,63}\.)*[a-zA-Z0-9_-]{1,63}$/)
const ipv6_validator = helpers.regex(/^(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:))$/)

// Custom validator for CCU IP that accepts both IPv4 and IPv6
const ccuIPValidator = (value) => {
  if (!value || value.trim() === '') return true // Allow empty
  // Check if it's a valid IPv4
  const ipv4Regex = /^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/
  if (ipv4Regex.test(value)) return true
  // Check if it's a valid IPv6
  const ipv6Regex = /^(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:))$/
  if (ipv6Regex.test(value)) return true
  return false
}

// Validation rules — wrapped in computed() so the localized validator
// messages (helpers.withMessage) follow locale changes at runtime instead of
// being frozen to whatever language was active when the component mounted.
const rules = computed(() => ({
  adminUsername: {
    required,
    username_validator,
    maxLength: maxLength(32)
  },
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
    ccuIPValidator: helpers.withMessage(t('settings.validation.invalidIpv4OrIpv6'), ccuIPValidator)
  },
  ipv6Address: {
    required: requiredIf(isIPv6Static),
    ipv6_validator: helpers.withMessage(t('settings.validation.invalidIpv6'), ipv6_validator)
  },
  ipv6PrefixLength: {
    required: requiredIf(isIPv6Static),
    numeric,
    minValue: helpers.withMessage(t('settings.validation.minPrefix'), val => val >= 1),
    maxValue: helpers.withMessage(t('settings.validation.maxPrefix'), val => val <= 128)
  },
  ipv6Gateway: {
    required: requiredIf(isIPv6Static),
    ipv6_validator: helpers.withMessage(t('settings.validation.invalidIpv6'), ipv6_validator)
  },
  ipv6Dns1: {
    required: requiredIf(isIPv6Static),
    ipv6_validator: helpers.withMessage(t('settings.validation.invalidIpv6'), ipv6_validator)
  },
  ipv6Dns2: {
    ipv6_validator: helpers.withMessage(t('settings.validation.invalidIpv6'), ipv6_validator)
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
}))

const v$ = useVuelidate(rules, {
  adminUsername,
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

const buildSettingsPayload = () => ({
  adminUsername: adminUsername.value,
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
  ledPrograms: { ...ledProgramValues.value },
  enableIPv6: enableIPv6.value,
  ipv6Mode: ipv6Mode.value,
  ipv6Address: ipv6Address.value,
  ipv6PrefixLength: ipv6PrefixLength.value,
  ipv6Gateway: ipv6Gateway.value,
  ipv6Dns1: ipv6Dns1.value,
  ipv6Dns2: ipv6Dns2.value,
  flashPause: flashPause.value
})

const serializeSettings = () => JSON.stringify(buildSettingsPayload())

const serializedCurrent = ref('')
let dirtyTimer = null
const updateDirtyState = () => {
  clearTimeout(dirtyTimer)
  dirtyTimer = setTimeout(() => {
    serializedCurrent.value = serializeSettings()
  }, 300)
}

watch([adminUsername, hostname, useDHCP, localIP, netmask, gateway, dns1, dns2, ccuIP, timesource, dcfOffset, gpsBaudrate, ntpServer, ledBrightness, ledProgramValues, enableIPv6, ipv6Mode, ipv6Address, ipv6PrefixLength, ipv6Gateway, ipv6Dns1, ipv6Dns2, flashPause], updateDirtyState, { deep: true })

const hasUnsavedChanges = computed(() => loadedSnapshot.value !== '' && serializedCurrent.value !== '' && serializedCurrent.value !== loadedSnapshot.value)
const adminUsernameChanged = computed(() => adminUsername.value !== (settingsStore.adminUsername || 'admin'))

// Load settings from store
const loadSettings = () => {
  adminUsername.value = settingsStore.adminUsername || 'admin'
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

  // Load LED programs if available
  if (settingsStore.ledPrograms !== undefined) {
    ledProgramValues.value = { ...settingsStore.ledPrograms }
  }

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

  // Flash pause (experimental) - keep the toggle in sync with the persisted
  // store value. Without this the local ref stays at its default and the
  // dirty-tracker / save-payload would silently rewrite the stored flag.
  if (settingsStore.flashPause !== undefined) {
    flashPause.value = settingsStore.flashPause
  }

  loadedSnapshot.value = serializeSettings()
  serializedCurrent.value = loadedSnapshot.value
  showError.value = null
  v$.value.$reset()
}

// Watch store changes
watch(() => settingsStore.$state, () => {
  loadSettings()
}, { deep: true })

onMounted(async () => {
  await settingsStore.load()
  loadSettings()
  window.addEventListener('beforeunload', handleBeforeUnload)
})

onBeforeUnmount(() => {
  window.removeEventListener('beforeunload', handleBeforeUnload)
})

onBeforeRouteLeave(() => {
  if (!hasUnsavedChanges.value) return true
  return window.confirm(t('settings.unsavedLeave'))
})

const handleBeforeUnload = (event) => {
  if (!hasUnsavedChanges.value) return
  event.preventDefault()
  event.returnValue = ''
}

const handleFileSelect = (event) => {
  restoreFile.value = event.target.files[0]
}

const saveSettingsClick = async () => {
  v$.value.$touch()
  if (v$.value.$error) return

  showError.value = null

  try {
    const settings = buildSettingsPayload()

    await settingsStore.save(settings)
    loadedSnapshot.value = serializeSettings()

    uiStore.pushToast({
      type: 'success',
      title: t('common.success'),
      message: t('settings.saveSuccess'),
      duration: 2200
    })
    setTimeout(() => {
      showRestartModal.value = true
    }, 700)
  } catch (error) {
    // Extract specific error message from backend response
    const errorMsg = error.response?.data?.error || error.response?.data || t('settings.saveError')
    showError.value = errorMsg
    uiStore.pushToast({
      type: 'error',
      title: t('common.error'),
      message: String(errorMsg)
    })
  }
}

const resetToLoadedState = () => {
  if (!hasUnsavedChanges.value) return
  if (!window.confirm(t('settings.discardConfirm'))) return
  loadSettings()
}

const performRestart = async () => {
  isRestarting.value = true
  try {
    await axios.post('/api/restart')
    showRestartModal.value = false
    uiStore.pushToast({ type: 'info', title: t('common.success'), message: t('firmware.restartingText'), duration: 18000 })
    // The device needs a moment to actually restart plus boot time - an
    // immediate reload just lands on a dead socket.
    setTimeout(() => window.location.reload(), 20000)
  } catch (e) {
    console.error("Restart request failed", e)
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('settings.restartError') })
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
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('settings.backupError') })
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
        uiStore.pushToast({ type: 'success', title: t('common.success'), message: t('settings.restoreSuccess'), duration: 1500 })
        window.location.reload()
      } catch (err) {
        uiStore.pushToast({ type: 'error', title: t('common.error'), message: `${t('settings.restoreError')}: ${err.message}` })
      }
    }
    reader.readAsText(restoreFile.value)
  } catch (e) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('settings.restoreError') })
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
  overflow-x: auto;
  -webkit-overflow-scrolling: touch;
  scrollbar-width: none;
}

.tabs-container::-webkit-scrollbar {
  display: none;
}

.segmented-control {
  display: inline-flex;
  background-color: var(--color-border-light);
  padding: 4px;
  border-radius: 20px;
  position: relative;
  width: 100%;
  max-width: 700px;
  gap: 4px;
}

.segment-btn {
  flex: 1 0 auto;
  min-width: max-content;
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
  gap: 8px;
  display: inline-flex;
  justify-content: center;
  align-items: center;
  white-space: nowrap;
}

.segment-label {
  min-width: 0;
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

.security-control-row {
  display: flex;
  align-items: center;
  gap: 10px;
  width: min(360px, 100%);
}

.security-control {
  min-width: 0;
}

.security-save-btn {
  flex: 0 0 auto;
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

.switch-copy {
  display: flex;
  flex-direction: column;
  gap: 4px;
  padding-right: var(--spacing-md);
}

.switch-copy h4 {
  margin: 0;
  font-size: 1rem;
}

.switch-copy p {
  margin: 0;
  color: var(--color-text-secondary);
  font-size: 0.875rem;
}

.experimental-warning {
  display: flex;
  align-items: flex-start;
  gap: var(--spacing-md);
  margin-bottom: var(--spacing-lg);
  padding: var(--spacing-md);
  border: 1px solid var(--color-warning);
  border-radius: var(--radius-md);
  background: var(--color-warning-soft);
  color: var(--color-text);
}

.experimental-warning .app-icon {
  color: var(--color-warning);
  flex: 0 0 auto;
  margin-top: 2px;
}

.experimental-warning p {
  margin: 4px 0 0;
  color: var(--color-text-secondary);
  font-size: 0.9rem;
}

.experimental-switch-row {
  gap: var(--spacing-md);
  padding: var(--spacing-md);
  border-radius: var(--radius-md);
  background: var(--color-bg);
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

.brightness-icon {
  font-size: 1.4rem;
  color: var(--color-warning);
  flex-shrink: 0;
}

.brightness-icon.dim {
  opacity: 0.4;
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
  color: var(--color-primary);
  margin-bottom: var(--spacing-xs);
  display: inline-flex;
}

.check-icon {
  position: absolute;
  top: 8px;
  right: 8px;
  color: var(--color-primary);
  font-size: 1.1rem;
  display: inline-flex;
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
  font-size: 1.8rem;
  color: var(--color-primary);
  margin-right: var(--spacing-md);
  display: inline-flex;
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
  font-size: 1.2rem;
  display: inline-flex;
}

.file-input {
  display: none;
}

/* LED Programs grid - base definition (was only in mobile MQ, missing on desktop) */
.led-programs-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
  gap: var(--spacing-md);
  margin-top: var(--spacing-md);
}

.led-program-item {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-xs);
}

.led-program-item .form-label.small {
  font-size: 0.875rem;
  font-weight: 500;
  color: var(--color-text-secondary);
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

.discard-btn {
  min-width: 132px;
  box-shadow: var(--shadow-lg);
}

.footer-alert {
  flex: 1;
  margin: 0;
  box-shadow: var(--shadow-lg);
}

.warning-chip {
  color: var(--color-warning);
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
  }

  .segmented-control {
    min-width: max-content;
    width: auto;
    flex-wrap: nowrap;
  }

  .segment-btn {
    padding: 10px 16px;
    font-size: 0.875rem;
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

  .security-control-row {
    width: 100%;
    flex-direction: column;
  }

  .security-control {
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
}

@media (max-width: 576px) {
  .segment-btn {
    padding: 8px 14px;
    font-size: 0.8125rem;
  }
}
</style>
