<template>
  <BCard
    :header="t('settings.title')"
    header-tag="h6"
    header-bg-variant="secondary"
    header-text-variant="white"
    class="mb-3"
  >
    <BForm @submit.stop.prevent>
      <BFormGroup :label="t('settings.changePassword')" label-cols-sm="4">
        <BButton variant="outline-primary" @click="showPasswordModal = true">
          {{ t('settings.changePassword') }}
        </BButton>
      </BFormGroup>
      <hr />
      <h6 class="text-secondary">{{ t('settings.networkSettings') }}</h6>
      <BFormGroup :label="t('settings.hostname')" label-cols-sm="4">
        <BFormInput
          type="text"
          v-model="hostname"
          trim
          :state="v$.hostname.$error ? false : null"
        />
      </BFormGroup>
      <BFormGroup :label="t('settings.dhcp')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="useDHCP" required>
          <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
          <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>
      <BFormGroup :label="t('settings.ipAddress')" label-cols-sm="4" v-if="!useDHCP">
        <BFormInput
          type="text"
          v-model="localIP"
          trim
          :state="v$.localIP.$error ? false : null"
        />
      </BFormGroup>
      <BFormGroup :label="t('settings.netmask')" label-cols-sm="4" v-if="!useDHCP">
        <BFormInput
          type="text"
          v-model="netmask"
          trim
          :state="v$.netmask.$error ? false : null"
        />
      </BFormGroup>
      <BFormGroup :label="t('settings.gateway')" label-cols-sm="4" v-if="!useDHCP">
        <BFormInput
          type="text"
          v-model="gateway"
          trim
          :state="v$.gateway.$error ? false : null"
        />
      </BFormGroup>
      <BFormGroup :label="t('settings.dns1')" label-cols-sm="4" v-if="!useDHCP">
        <BFormInput
          type="text"
          v-model="dns1"
          trim
          :state="v$.dns1.$error ? false : null"
        />
      </BFormGroup>
      <BFormGroup :label="t('settings.dns2')" label-cols-sm="4" v-if="!useDHCP">
        <BFormInput
          type="text"
          v-model="dns2"
          trim
          :state="v$.dns2.$error ? false : null"
        />
      </BFormGroup>
      <hr />
      <h6 class="text-secondary">{{ t('settings.ipv6Settings') }}</h6>
      <BFormGroup :label="t('settings.enableIPv6')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="enableIPv6" required>
          <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
          <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>
      <template v-if="enableIPv6">
        <BFormGroup :label="t('settings.ipv6Mode')" label-cols-sm="4">
          <BFormRadioGroup buttons v-model="ipv6Mode" required>
            <BFormRadio value="auto">{{ t('settings.ipv6Auto') }}</BFormRadio>
            <BFormRadio value="static">{{ t('settings.ipv6Static') }}</BFormRadio>
          </BFormRadioGroup>
        </BFormGroup>
        <template v-if="ipv6Mode === 'static'">
          <BFormGroup :label="t('settings.ipv6Address')" label-cols-sm="4">
            <BFormInput
              type="text"
              v-model="ipv6Address"
              trim
              placeholder="2001:db8::1"
              :state="v$.ipv6Address.$error ? false : null"
            />
          </BFormGroup>
          <BFormGroup :label="t('settings.ipv6PrefixLength')" label-cols-sm="4">
            <BFormInput
              type="number"
              v-model.number="ipv6PrefixLength"
              min="1"
              max="128"
              placeholder="64"
              :state="v$.ipv6PrefixLength.$error ? false : null"
            />
          </BFormGroup>
          <BFormGroup :label="t('settings.ipv6Gateway')" label-cols-sm="4">
            <BFormInput
              type="text"
              v-model="ipv6Gateway"
              trim
              placeholder="2001:db8::1"
              :state="v$.ipv6Gateway.$error ? false : null"
            />
          </BFormGroup>
          <BFormGroup :label="t('settings.ipv6Dns1')" label-cols-sm="4">
            <BFormInput
              type="text"
              v-model="ipv6Dns1"
              trim
              placeholder="2001:4860:4860::8888"
              :state="v$.ipv6Dns1.$error ? false : null"
            />
          </BFormGroup>
          <BFormGroup :label="t('settings.ipv6Dns2')" label-cols-sm="4">
            <BFormInput
              type="text"
              v-model="ipv6Dns2"
              trim
              placeholder="2001:4860:4860::8844"
              :state="v$.ipv6Dns2.$error ? false : null"
            />
          </BFormGroup>
        </template>
      </template>
      <hr />
      <h6 class="text-secondary">{{ t('settings.timeSettings') }}</h6>
      <BFormGroup :label="t('settings.timesource')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="timesource" required>
          <BFormRadio :value="0">{{ t('settings.ntp') }}</BFormRadio>
          <BFormRadio :value="1">{{ t('settings.dcf') }}</BFormRadio>
          <BFormRadio :value="2">{{ t('settings.gps') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>
      <BFormGroup :label="t('settings.ntpServer')" label-cols-sm="4" v-if="isNtpActivated">
        <BFormInput
          type="text"
          v-model="ntpServer"
          trim
          :state="v$.ntpServer.$error ? false : null"
        />
      </BFormGroup>
      <BFormGroup :label="t('settings.dcfOffset')" label-cols-sm="4" v-if="isDcfActivated">
        <BInputGroup append="µs">
          <BFormInput
            type="number"
            v-model.number="dcfOffset"
            min="0"
            :state="v$.dcfOffset.$error ? false : null"
          />
        </BInputGroup>
      </BFormGroup>
      <BFormGroup :label="t('settings.gpsBaudrate')" label-cols-sm="4" v-if="isGpsActivated">
        <BFormSelect v-model.number="gpsBaudrate">
          <BFormSelectOption :value="4800">4800</BFormSelectOption>
          <BFormSelectOption :value="9600">9600</BFormSelectOption>
          <BFormSelectOption :value="19200">19200</BFormSelectOption>
          <BFormSelectOption :value="38400">38400</BFormSelectOption>
          <BFormSelectOption :value="57600">57600</BFormSelectOption>
          <BFormSelectOption :value="115200">115200</BFormSelectOption>
        </BFormSelect>
      </BFormGroup>
      <hr />
      <h6 class="text-secondary">{{ t('settings.systemSettings') }}</h6>
      <BFormGroup :label="t('settings.ledBrightness')" label-cols-sm="4">
        <BInputGroup append="%">
          <BFormSelect v-model.number="ledBrightness">
            <BFormSelectOption :value="0">0</BFormSelectOption>
            <BFormSelectOption :value="5">5</BFormSelectOption>
            <BFormSelectOption :value="10">10</BFormSelectOption>
            <BFormSelectOption :value="25">25</BFormSelectOption>
            <BFormSelectOption :value="50">50</BFormSelectOption>
            <BFormSelectOption :value="75">75</BFormSelectOption>
            <BFormSelectOption :value="100">100</BFormSelectOption>
          </BFormSelect>
        </BInputGroup>
      </BFormGroup>
      <BFormGroup :label="t('settings.checkUpdates')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="checkUpdates" required>
          <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
          <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>
      <BFormGroup :label="t('settings.allowPrerelease')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="allowPrerelease" required>
          <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
          <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>
      <hr />
      <h6 class="text-secondary">{{ t('settings.hmlgwSettings') }}</h6>
      <BFormGroup :label="t('settings.enableHmlgw')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="hmlgwEnabled" required>
          <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
          <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>
      <template v-if="hmlgwEnabled">
        <BFormGroup :label="t('settings.hmlgwPort')" label-cols-sm="4">
          <BFormInput
            type="number"
            v-model.number="hmlgwPort"
            min="1"
            max="65535"
            :state="v$.hmlgwPort.$error ? false : null"
          />
        </BFormGroup>
        <BFormGroup :label="t('settings.hmlgwKeepAlivePort')" label-cols-sm="4">
          <BFormInput
            type="number"
            v-model.number="hmlgwKeepAlivePort"
            min="1"
            max="65535"
            :state="v$.hmlgwKeepAlivePort.$error ? false : null"
          />
        </BFormGroup>
      </template>
      <hr />
      <h6 class="text-secondary">{{ t('settings.analyzerSettings') }}</h6>
      <BFormGroup :label="t('settings.enableAnalyzer')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="analyzerEnabled" required>
          <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
          <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>

      <hr />
      <h6 class="text-secondary">{{ t('settings.dtls.title') }}</h6>
      <BFormGroup :label="t('settings.dtls.mode')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="dtlsMode" required>
          <BFormRadio :value="0">{{ t('common.disabled') }}</BFormRadio>
          <BFormRadio :value="1">{{ t('settings.dtls.modePsk') }}</BFormRadio>
          <BFormRadio :value="2">{{ t('settings.dtls.modeCert') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>
      <template v-if="dtlsMode > 0">
        <BFormGroup :label="t('settings.dtls.cipherSuite')" label-cols-sm="4">
          <BFormSelect v-model="dtlsCipherSuite">
            <BFormSelectOption :value="0">AES-128-GCM</BFormSelectOption>
            <BFormSelectOption :value="1">AES-256-GCM</BFormSelectOption>
            <BFormSelectOption :value="2">ChaCha20-Poly1305</BFormSelectOption>
          </BFormSelect>
        </BFormGroup>
        <BFormGroup :label="t('settings.dtls.sessionResumption')" label-cols-sm="4">
          <BFormRadioGroup buttons v-model="dtlsSessionResumption" required>
            <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
            <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
          </BFormRadioGroup>
        </BFormGroup>
        <template v-if="dtlsMode === 2">
          <BFormGroup :label="t('settings.dtls.requireClientCert')" label-cols-sm="4">
            <BFormRadioGroup buttons v-model="dtlsRequireClientCert" required>
              <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
              <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
            </BFormRadioGroup>
          </BFormGroup>
        </template>
        <BAlert variant="info" :model-value="true" class="mt-2">
            {{ t('settings.dtls.restartNote') }}
        </BAlert>
      </template>

      <BAlert
        variant="success"
        :model-value="showSuccess"
        dismissible
        fade
        @update:model-value="showSuccess = null"
      >{{ t("settings.saveSuccess") }}</BAlert>
      <BAlert
        variant="danger"
        :model-value="showError"
        dismissible
        fade
        @update:model-value="showError = null"
      >{{ t("settings.saveError") }}</BAlert>

      <BFormGroup label-cols-sm="9">
        <BButton
          variant="primary"
          block
          @click="saveSettingsClick"
          :disabled="v$.$error || loading"
        >
          <BSpinner small v-if="loading" class="me-2" />
          {{ t('common.save') }}
        </BButton>
      </BFormGroup>
    </BForm>
  </BCard>

  <BCard
    :header="t('settings.backupRestore')"
    header-tag="h6"
    header-bg-variant="info"
    header-text-variant="white"
    class="mb-3"
  >
    <BForm @submit.stop.prevent>
        <p>{{ t('settings.backupInfo') }}</p>
        <BButton
          variant="outline-primary"
          class="mb-3"
          @click="downloadBackup"
          :disabled="backupLoading"
        >
          <BSpinner small v-if="backupLoading" class="me-2" />
          {{ t('settings.downloadBackup') }}
        </BButton>

        <hr/>
        <p>{{ t('settings.restoreInfo') }}</p>
        <BFormFile
          v-model="restoreFile"
          accept=".json"
          :placeholder="t('settings.noFileChosen')"
          :browse-text="t('settings.browse')"
          class="mb-2"
        />
        <BButton
            variant="warning"
            :disabled="!restoreFile || restoreLoading"
            @click="restoreSettingsClick"
        >
          <BSpinner small v-if="restoreLoading" class="me-2" />
          {{ t('settings.restore') }}
        </BButton>
    </BForm>
  </BCard>

  <BCard
    :header="t('settings.systemMaintenance')"
    header-tag="h6"
    header-bg-variant="danger"
    header-text-variant="white"
    class="mb-3"
  >
    <BButton variant="warning" block class="me-2" @click="showRebootModal = true">{{ t('settings.reboot') }}</BButton>
    <BButton variant="danger" block @click="showFactoryResetModal = true">{{ t('settings.factoryReset') }}</BButton>
  </BCard>

  <!-- Loading Overlay -->
  <LoadingOverlay :visible="isSystemBusy" :message="systemBusyMessage" />

  <!-- Reboot Confirmation Modal -->
  <BModal
    v-model="showRebootModal"
    :title="t('settings.reboot')"
    @ok="rebootConfirmed"
    ok-variant="warning"
    :ok-title="t('common.yes')"
    :cancel-title="t('common.no')"
  >
    <p>{{ t('common.confirmReboot') }}</p>
  </BModal>

  <!-- Factory Reset Confirmation Modal -->
  <BModal
    v-model="showFactoryResetModal"
    :title="t('settings.factoryReset')"
    @ok="factoryResetConfirmed"
    ok-variant="danger"
    :ok-title="t('common.yes')"
    :cancel-title="t('common.no')"
  >
    <p>{{ t('common.confirmFactoryReset') }}</p>
  </BModal>

  <!-- Restore Confirmation Modal -->
  <BModal
    v-model="showRestoreModal"
    :title="t('settings.restore')"
    @ok="restoreConfirmed"
    ok-variant="warning"
    :ok-title="t('common.yes')"
    :cancel-title="t('common.no')"
  >
    <p>{{ t('settings.restoreConfirm') }}</p>
  </BModal>

  <!-- Password Change Modal -->
  <BModal
    v-model="showPasswordModal"
    :title="t('settings.changePassword')"
    @ok="handlePasswordChange"
    @cancel="resetPasswordModal"
    ok-variant="primary"
    :ok-title="t('common.save')"
    :cancel-title="t('common.cancel')"
  >
    <BFormGroup :label="t('settings.changePassword')" label-cols-sm="4">
      <PasswordInput
        v-model="adminPassword"
        :state="passwordValidation.password.$error ? false : null"
      />
    </BFormGroup>
    <BFormGroup :label="t('settings.repeatPassword')" label-cols-sm="4">
      <PasswordInput
        v-model="adminPasswordRepeat"
        :state="passwordValidation.passwordRepeat.$error ? false : null"
      />
    </BFormGroup>
    <BAlert
      variant="danger"
      :model-value="passwordError"
      dismissible
      fade
      @update:model-value="passwordError = null"
    >{{ passwordError }}</BAlert>
  </BModal>
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
  sameAs,
  helpers,
  requiredIf,
  requiredUnless
} from '@vuelidate/validators'
import { useSettingsStore } from './stores.js'
import PasswordInput from './components/PasswordInput.vue'
import LoadingOverlay from './components/LoadingOverlay.vue'

const hostname_validator = helpers.regex(/^[a-zA-Z0-9_-]{1,63}$/)
const domainname_validator = helpers.regex(/^([a-zA-Z0-9_-]{1,63}\.)*[a-zA-Z0-9_-]{1,63}$/)
const ipv6_validator = helpers.regex(/^(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:))$/)

const { t } = useI18n()

const settingsStore = useSettingsStore()

// Local form state
const restoreFile = ref(null)
const adminPassword = ref('')
const adminPasswordRepeat = ref('')
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

const hmlgwEnabled = ref(false)
const hmlgwPort = ref(2000)
const hmlgwKeepAlivePort = ref(2001)

const analyzerEnabled = ref(false)

// DTLS
const dtlsMode = ref(0)
const dtlsCipherSuite = ref(1)
const dtlsRequireClientCert = ref(false)
const dtlsSessionResumption = ref(true)

const showSuccess = ref(null)
const showError = ref(null)
const loading = ref(false)
const backupLoading = ref(false)
const restoreLoading = ref(false)

// Modals & Overlay state
const showPasswordModal = ref(false)
const passwordError = ref(null)
const showRebootModal = ref(false)
const showFactoryResetModal = ref(false)
const showRestoreModal = ref(false)
const isSystemBusy = ref(false)
const systemBusyMessage = ref('')

// Computed flags
const isNtpActivated = computed(() => timesource.value === 0)
const isDcfActivated = computed(() => timesource.value === 1)
const isGpsActivated = computed(() => timesource.value === 2)
const isIPv6Static = computed(() => enableIPv6.value && ipv6Mode.value === 'static')

const password_validator = helpers.regex(/^(?=.*[A-Za-z])(?=.*\d).{6,}$/)

// Validation rules
const passwordRules = {
  password: {
    required,
    minLength: minLength(6),
    maxLength: maxLength(32),
    password_validator: helpers.withMessage('Must contain letters and numbers', password_validator)
  },
  passwordRepeat: {
    required,
    sameAsPassword: helpers.withMessage('Passwords do not match', sameAs(adminPassword))
  }
}

const passwordValidation = useVuelidate(passwordRules, {
  password: adminPassword,
  passwordRepeat: adminPasswordRepeat
})

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
  },
  hmlgwPort: {
    required: requiredIf(hmlgwEnabled),
    numeric,
    minValue: helpers.withMessage('Min 1', val => val >= 1),
    maxValue: helpers.withMessage('Max 65535', val => val <= 65535)
  },
  hmlgwKeepAlivePort: {
    required: requiredIf(hmlgwEnabled),
    numeric,
    minValue: helpers.withMessage('Min 1', val => val >= 1),
    maxValue: helpers.withMessage('Max 65535', val => val <= 65535)
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
  dcfOffset,
  hmlgwPort,
  hmlgwKeepAlivePort
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
  checkUpdates.value = settingsStore.checkUpdates
  allowPrerelease.value = settingsStore.allowPrerelease

  if (settingsStore.hmlgwEnabled !== undefined) {
      hmlgwEnabled.value = settingsStore.hmlgwEnabled
      hmlgwPort.value = settingsStore.hmlgwPort || 2000
      hmlgwKeepAlivePort.value = settingsStore.hmlgwKeepAlivePort || 2001
  }

  if (settingsStore.analyzerEnabled !== undefined) {
      analyzerEnabled.value = settingsStore.analyzerEnabled
  }

  // Load DTLS settings
  if (settingsStore.dtlsMode !== undefined) {
    dtlsMode.value = settingsStore.dtlsMode
    dtlsCipherSuite.value = settingsStore.dtlsCipherSuite
    dtlsRequireClientCert.value = settingsStore.dtlsRequireClientCert
    dtlsSessionResumption.value = settingsStore.dtlsSessionResumption
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
}

// Watch store changes
watch(() => settingsStore.$state, () => {
  loadSettings()
}, { deep: true })

onMounted(async () => {
  await settingsStore.load()
  loadSettings()
})

const saveSettingsClick = async () => {
  v$.value.$touch()
  if (v$.value.$error) return

  loading.value = true
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
      // HMLGW
      hmlgwEnabled: hmlgwEnabled.value,
      hmlgwPort: hmlgwPort.value,
      hmlgwKeepAlivePort: hmlgwKeepAlivePort.value,
      analyzerEnabled: analyzerEnabled.value,
      // DTLS
      dtlsMode: dtlsMode.value,
      dtlsCipherSuite: dtlsCipherSuite.value,
      dtlsRequireClientCert: dtlsRequireClientCert.value,
      dtlsSessionResumption: dtlsSessionResumption.value,
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
  } finally {
    loading.value = false
  }
}

const downloadBackup = async () => {
  backupLoading.value = true
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
  } finally {
    backupLoading.value = false
  }
}

const restoreSettingsClick = () => {
  if (!restoreFile.value) return
  showRestoreModal.value = true
}

const restoreConfirmed = async () => {
  restoreLoading.value = true
  try {
    const reader = new FileReader()
    reader.onload = async (e) => {
      try {
        const json = JSON.parse(e.target.result)
        await axios.post('/api/restore', json)
        systemBusyMessage.value = t('settings.restoreSuccess')
        isSystemBusy.value = true
        setTimeout(() => { window.location.reload() }, 10000)
      } catch (err) {
        restoreLoading.value = false
        alert(t('settings.restoreError') + ': ' + err.message)
      }
    }
    reader.onerror = () => {
      restoreLoading.value = false
      alert(t('settings.restoreError'))
    }
    reader.readAsText(restoreFile.value)
  } catch (e) {
    restoreLoading.value = false
    alert(t('settings.restoreError'))
  }
}

const rebootConfirmed = async () => {
  try {
    await axios.post('/api/restart')
    systemBusyMessage.value = t('common.rebootingWait')
    isSystemBusy.value = true
    setTimeout(() => {
      window.location.reload()
    }, 10000)
  } catch (e) {
    console.error(e)
    showError.value = true
  }
}

const factoryResetConfirmed = async () => {
  try {
    await axios.post('/api/factory_reset')
    systemBusyMessage.value = t('common.factoryResettingWait')
    isSystemBusy.value = true
    setTimeout(() => {
      window.location.reload()
    }, 10000)
  } catch (e) {
    console.error(e)
    alert('Factory Reset failed')
  }
}

const handlePasswordChange = async (event) => {
  event.preventDefault()
  passwordValidation.value.$touch()

  if (passwordValidation.value.$error) {
    passwordError.value = passwordValidation.value.$errors[0].$message
    return
  }

  try {
    const settings = {
      adminPassword: adminPassword.value
    }

    await settingsStore.save(settings)
    showPasswordModal.value = false
    resetPasswordModal()
    showSuccess.value = true
  } catch (error) {
    passwordError.value = t('settings.saveError')
  }
}

const resetPasswordModal = () => {
  adminPassword.value = ''
  adminPasswordRepeat.value = ''
  passwordError.value = null
  passwordValidation.value.$reset()
}
</script>

<style scoped>
</style>
