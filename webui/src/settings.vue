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
      <!-- Korrekturauftrag §9: the hero previously repeated every tab name as
           a status chip, creating a second, ambiguous navigation layer. Now it
           shows only the device hostname + a NON-interactive saved/unsaved
           indicator that just reflects form state (the segmented control below
           is the single navigation surface). -->
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="board" /> {{ hostname || 'HB-RF-ETH-ng' }}</span>
        <span class="save-state" :class="hasUnsavedChanges ? 'is-unsaved' : 'is-saved'" aria-live="polite">
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
                    autocorrect="off"
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

          <!-- Korrekturauftrag §7: opt-in to experimental features. Hidden by
               default; enabling reveals the experimental tab + any experimental
               nav entries. Stored experimental values are NOT deleted on
               disable (we only hide the UI). -->
          <div class="settings-card">
            <div class="card-header">
              <h3>{{ t('settings.advancedTitle') }}</h3>
            </div>
            <div class="card-body">
              <BAlert variant="warning" :model-value="true" fade class="mb-3">
                {{ t('settings.showExperimentalWarning') }}
              </BAlert>
              <div class="switch-row">
                <label class="switch-label">{{ t('settings.showExperimental') }}</label>
                <div class="form-check form-switch">
                  <input
                    class="form-check-input"
                    type="checkbox"
                    role="switch"
                    :checked="experimentalStore.showExperimental"
                    @change="onToggleExperimental($event.target.checked)"
                  >
                </div>
              </div>
              <div class="form-text mt-2">
                <small>{{ t('settings.showExperimentalHint') }}</small>
              </div>
            </div>
          </div>
        </div>
      </Transition>

      <!-- License Tab -->
      <Transition name="fade" mode="out-in">
        <div v-if="activeTab === 'license'" class="tab-panel">
          <!-- Supporter key (cosmetic badge) -->
          <div class="settings-card supporter-card" :class="{ 'is-active': sysInfoStore.supporterActive }">
            <div class="card-header">
              <div class="header-content">
                <div class="header-icon supporter-icon"><AppIcon :name="sysInfoStore.supporterActive ? 'heart' : 'coffee'" /></div>
                <h3>{{ t('supporter.title') }}</h3>
              </div>
              <span v-if="sysInfoStore.supporterActive" class="supporter-badge-active">
                <AppIcon name="check" /> {{ t('supporter.active') }}
              </span>
            </div>
            <div class="card-body">
              <p v-if="!sysInfoStore.supporterActive && !sysInfoStore.supporterRevoked" class="supporter-intro">{{ t('supporter.intro') }}</p>

              <div v-if="sysInfoStore.supporterRevoked" class="supporter-revoked-panel">
                <div class="revoked-icon-wrap"><AppIcon name="alert" /></div>
                <div class="revoked-text">
                  <strong>{{ t('supporter.revokedTitle') }}</strong>
                  <p>{{ t('supporter.revokedBody') }}</p>
                  <button type="button" class="btn-link-danger" @click="removeSupporterKey">
                    {{ t('supporter.remove') }}
                  </button>
                </div>
              </div>

              <div v-else-if="sysInfoStore.supporterActive" class="supporter-thanks-panel">
                <div class="medallion">
                  <span class="medallion-shine"></span>
                  <span class="medallion-core"><AppIcon name="heart" /></span>
                  <span class="medallion-spark spark-a"><AppIcon name="star" /></span>
                  <span class="medallion-spark spark-b"><AppIcon name="star" /></span>
                  <span class="medallion-spark spark-c"><AppIcon name="star" /></span>
                </div>
                <div class="medallion-text">
                  <span class="medallion-label">{{ t('supporter.badgeLabel') }}</span>
                  <strong class="medallion-title">{{ t('supporter.thanksTitle') }}</strong>
                  <p class="medallion-body">{{ t('supporter.thanksBody', { date: sysInfoStore.supporterExpiresAt || '—' }) }}</p>
                  <button type="button" class="btn-link-danger supporter-remove" @click="removeSupporterKey">
                    {{ t('supporter.remove') }}
                  </button>
                </div>
              </div>

              <div v-else class="supporter-input-row">
                <BFormInput
                  v-model="supporterKeyInput"
                  class="supporter-input"
                  :placeholder="t('supporter.placeholder')"
                  :state="supporterFeedback === 'invalid' ? false : (supporterFeedback === 'valid' ? true : null)"
                  spellcheck="false"
                  autocapitalize="characters"
                  autocomplete="off"
                  @input="onSupporterKeyInput"
                  @keyup.enter="activateSupporterKey"
                />
                <BButton
                  variant="primary"
                  class="supporter-activate-btn"
                  :disabled="!canActivateSupporterKey || activatingSupporter"
                  @click="activateSupporterKey"
                >
                  <span v-if="activatingSupporter" class="spinner-border spinner-border-sm me-1"></span>
                  {{ t('supporter.activate') }}
                </BButton>
              </div>

              <div v-if="supporterFeedbackText" class="supporter-feedback" :class="supporterFeedback">
                {{ supporterFeedbackText }}
              </div>

              <div class="form-text supporter-footnote">
                {{ t('supporter.footnote') }}
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
                  maxlength="63"
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

          <div class="settings-card mt-3">
            <div class="card-header">
              <h3>{{ t('settings.ping.title') }}</h3>
            </div>
            <div class="card-body">
              <div class="form-group mb-3">
                <label class="form-label">{{ t('settings.ping.target') }}</label>
                <div class="d-flex">
                  <BFormInput
                    type="text"
                    v-model="pingTarget"
                    trim
                    :placeholder="t('settings.ping.placeholder')"
                    class="me-2"
                    @keyup.enter="runPing"
                  />
                  <BButton variant="primary" @click="runPing" :disabled="pingLoading || !pingTarget">
                    <span v-if="pingLoading" class="spinner-border spinner-border-sm" role="status" aria-hidden="true"></span>
                    <span v-else>{{ t('settings.ping.button') }}</span>
                  </BButton>
                </div>
              </div>
              <div v-if="pingResult !== null" :class="['alert', pingResult >= 0 ? 'alert-success' : 'alert-danger']">
                <span v-if="pingResult >= 0">{{ t('settings.ping.success', { latency: pingResult }) }}</span>
                <span v-else>{{ t('settings.ping.failure') }}</span>
              </div>
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

      <!-- Design Tab (Korrekturauftrag §5): embeds the same ThemeSettings the
           standalone /theme page renders, so the header theme toggle and the
           settings page stay in sync via useThemeStore. -->
      <Transition name="fade" mode="out-in">
        <div v-if="activeTab === 'design'" class="tab-panel">
          <ThemeSettings embedded />
        </div>
      </Transition>

      <!-- Experimental Tab (Korrekturauftrag §7): only rendered when the user
           opted in via Settings → Allgemein. The tab is included in the
           segmented control conditionally, but guard the panel too so a stale
           activeTab=experimental can never render an orphan panel. -->
      <Transition name="fade" mode="out-in">
        <div v-if="activeTab === 'experimental' && experimentalStore.showExperimental" class="tab-panel">
          <div class="settings-card">
            <div class="card-header">
              <h3>{{ t('settings.experimentalTitle') }}</h3>
            </div>
            <div class="card-body">
              <div class="experimental-empty-state">
                <AppIcon name="check" />
                <div>
                  <strong>{{ t('settings.experimentalEmptyTitle') }}</strong>
                  <p>{{ t('settings.experimentalEmptyText') }}</p>
                </div>
              </div>
            </div>
          </div>
        </div>
      </Transition>
    </div>

    <!-- Floating Action Bar for Save -->
    <Transition name="slide-up">
      <div v-if="loadedSnapshot || showError" class="floating-footer">
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
        v-if="true"
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
import { ref, computed, nextTick, onMounted, onBeforeUnmount, watch } from 'vue'
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
import { useSettingsStore, useLoginStore, useUiStore, useSysInfoStore, useRestartUiStore, useExperimentalStore } from './stores.js'
import PasswordChangeModal from './components/PasswordChangeModal.vue'
import ThemeSettings from './theme.vue'
import { validateSupporterKey, normalizeSupporterKey } from './composables/supporterKey.js'

import { useRoute, useRouter, onBeforeRouteLeave } from 'vue-router'

const { t } = useI18n()
const settingsStore = useSettingsStore()
const loginStore = useLoginStore()
const uiStore = useUiStore()
const sysInfoStore = useSysInfoStore()
const restartUiStore = useRestartUiStore()
const experimentalStore = useExperimentalStore()
const route = useRoute()
const router = useRouter()

// Password modal
const showPasswordModal = ref(false)

// Tab management
const activeTab = ref(route.query.tab || 'general')

const tabs = computed(() => {
  const base = [
    { id: 'general', label: t('settings.tabGeneral'), icon: 'shield' },
    { id: 'network', label: t('settings.tabNetwork'), icon: 'network' },
    { id: 'time', label: t('settings.tabTime'), icon: 'clock' },
    { id: 'backup', label: t('settings.tabBackup'), icon: 'backup' },
    { id: 'design', label: t('settings.tabDesign'), icon: 'sun' },
    { id: 'license', label: t('settings.tabLicense'), icon: 'heart' }
  ]
  // Experimental tab is gated behind the opt-in flag (Korrekturauftrag §7).
  // Stored experimental values are preserved when the flag is off — we just
  // hide the entry. The tab itself renders an empty-state; concrete
  // experimental toggles would live here when one ships.
  if (experimentalStore.showExperimental) {
    base.push({ id: 'experimental', label: t('settings.tabExperimental'), icon: 'alert' })
  }
  return base
})

// Sync URL with tab change
watch(activeTab, (newTab) => {
  router.replace({ query: { ...route.query, tab: newTab } })
})

// Sync tab with URL change. Bounce unknown / hidden tabs (e.g. ?tab=experimental
// when the opt-in flag is off) back to General so a stale link can't strand
// the user on an orphan panel. `immediate: true` covers the very first mount
// where `route.query.tab` may already point at a hidden tab — `tabs` is
// declared above so the immediate callback can read it safely.
watch(() => route.query.tab, (newTab) => {
  if (newTab && tabs.value.some(tab => tab.id === newTab)) {
    activeTab.value = newTab
  } else if (newTab && !tabs.value.some(tab => tab.id === newTab)) {
    activeTab.value = 'general'
  }
}, { immediate: true })

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

const pingTarget = ref('')
const pingLoading = ref(false)
const pingResult = ref(null)

const timesource = ref(0)
const dcfOffset = ref(0)
const gpsBaudrate = ref(9600)
const ntpServer = ref('')
const ledBrightness = ref(100)

// ---- Supporter key (cosmetic badge) ----
// The key is stored on the device and validated by the firmware. We do a
// client-side pre-check for instant feedback, then POST to /settings.json
// (the backend re-validates and only persists checksum-valid keys).
const supporterKeyInput = ref('')
const supporterFeedback = ref('')            // '' | 'valid' | 'invalid' | 'expired'
const activatingSupporter = ref(false)

const canActivateSupporterKey = computed(() => {
  const r = validateSupporterKey(supporterKeyInput.value)
  return r.valid && !r.expired
})

const supporterFeedbackText = computed(() => {
  if (!supporterKeyInput.value) return ''
  const r = validateSupporterKey(supporterKeyInput.value)
  if (!r.valid) return t('supporter.invalid')
  if (r.expired) return t('supporter.expired', { date: r.expiresAt })
  return t('supporter.validHint', { date: r.expiresAt })
})

function onSupporterKeyInput() {
  // Re-format as the user types: strip junk, group into XXXX-XXXX-XXXX-XXXX.
  const normalized = normalizeSupporterKey(supporterKeyInput.value)
  if (normalized !== supporterKeyInput.value) {
    supporterKeyInput.value = normalized
  }
  const r = validateSupporterKey(supporterKeyInput.value)
  supporterFeedback.value = !supporterKeyInput.value ? '' : (!r.valid ? 'invalid' : (r.expired ? 'expired' : 'valid'))
}

async function activateSupporterKey() {
  const r = validateSupporterKey(supporterKeyInput.value)
  if (!r.valid || r.expired) {
    supporterFeedback.value = r.expired ? 'expired' : 'invalid'
    return
  }
  activatingSupporter.value = true
  try {
    await axios.post('/settings.json', { supporterKey: supporterKeyInput.value }, { timeout: 10000 })
    // Refresh sysInfo so the badge in the header updates immediately.
    await sysInfoStore.update()
    uiStore.pushToast({ type: 'success', title: t('common.success'), message: t('supporter.activated') })
  } catch (e) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: e.response?.data?.error || t('supporter.activateFailed') })
  } finally {
    activatingSupporter.value = false
  }
}

async function removeSupporterKey() {
  try {
    await axios.post('/settings.json', { supporterKey: '' }, { timeout: 10000 })
    await sysInfoStore.update()
    supporterKeyInput.value = ''
    supporterFeedback.value = ''
    uiStore.pushToast({ type: 'info', title: t('common.success'), message: t('supporter.removed') })
  } catch (e) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('supporter.activateFailed') })
  }
}

// Korrekturauftrag §7: persist + immediately apply the experimental-visibility
// flag. Stored experimental values are untouched — we only flip the client
// gate, which the header nav + this tab list read reactively.
function onToggleExperimental(value) {
  experimentalStore.setShowExperimental(value)
  // If the user just disabled the flag while sitting on the experimental tab,
  // bounce them back to General so they don't see an empty panel.
  if (!value && activeTab.value === 'experimental') {
    activeTab.value = 'general'
  }
  uiStore.pushToast({
    type: 'info',
    title: t('common.success'),
    message: value ? t('settings.showExperimentalOn') : t('settings.showExperimentalOff'),
    duration: 2200
  })
}

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

// Keep these validators aligned with main/validation.cpp. Settings already
// accepted by the firmware must never make an unrelated bulk save silently
// fail in the WebUI.
const hostname_validator = (value) => {
  if (!value) return true
  if (value.length > 63 || !/^[a-zA-Z0-9]/.test(value) || !/^[a-zA-Z0-9.-]+$/.test(value)) return false
  for (let i = 0; i < value.length; i++) {
    if (value[i] === '-' && (i === 0 || i === value.length - 1 || value[i - 1] === '.' || value[i + 1] === '.')) {
      return false
    }
  }
  return true
}
const username_validator = helpers.regex(/^[a-zA-Z0-9._-]{1,32}$/)
const domainname_validator = helpers.regex(/^([a-zA-Z0-9_-]{1,63}\.)*[a-zA-Z0-9_-]{1,63}$/)
const ipv6_validator = (value) => {
  if (!value) return true
  if (typeof value !== 'string' || value.length > 45 || value.includes(':::')) return false

  let normalized = value
  if (value.includes('.')) {
    const separator = value.lastIndexOf(':')
    if (separator < 0) return false
    const ipv4 = value.slice(separator + 1).split('.')
    if (ipv4.length !== 4 || ipv4.some(part => !/^\d{1,3}$/.test(part) || Number(part) > 255)) return false
    // An embedded IPv4 suffix occupies two IPv6 hextets.
    normalized = `${value.slice(0, separator)}:0:0`
  }

  if ((normalized.match(/::/g) || []).length > 1) return false
  const hasCompression = normalized.includes('::')
  const [left = '', right = ''] = normalized.split('::')
  const leftParts = left ? left.split(':') : []
  const rightParts = right ? right.split(':') : []
  const validHextet = part => /^[0-9a-fA-F]{1,4}$/.test(part)
  if (!leftParts.every(validHextet) || !rightParts.every(validHextet)) return false

  const count = leftParts.length + rightParts.length
  return hasCompression ? count < 8 : count === 8
}

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
    maxLength: maxLength(63)
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
    ipv6_validator: helpers.withMessage(t('settings.validation.invalidIpv6'), ipv6_validator)
  },
  ipv6Dns1: {
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
  // NOTE: testDesignEnabled is deliberately omitted. It is persisted device-
  // wide the moment the user flips the toggle (useExperimentalStore posts it
  // immediately), so it must never travel in the bulk "save settings" payload
  // — otherwise a save could overwrite a newer toggle choice with a stale
  // snapshot value, and the dirty-tracker would falsely flag "unsaved
  // changes" right after a flip.
})

const serializeSettings = () => JSON.stringify(buildSettingsPayload())

const serializedCurrent = computed(serializeSettings)

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

  // NOTE: testDesignEnabled is intentionally NOT synced here. It is owned by
  // useExperimentalStore (single source of truth) and persisted device-wide on
  // every flip via that store. Re-syncing it on every settingsStore watcher
  // fire was the "toggle springs back" bug. The initial device value reaches
  // the experimental store via syncFromServer() inside settingsStore.load().
  // flashPause is also intentionally not synced into a local ref — it is read
  // straight from the store by the restart-confirmation modals, which is the
  // only place it surfaces.

  // Pre-fill the supporter key input from the stored value (formatted), and
  // make sure the header badge reflects the current device state.
  supporterKeyInput.value = settingsStore.supporterKey ? normalizeSupporterKey(settingsStore.supporterKey) : ''
  supporterFeedback.value = ''
  sysInfoStore.update().catch(() => {})

  loadedSnapshot.value = serializeSettings()
  showError.value = null
  v$.value.$reset()
}

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

const runPing = async () => {
  if (!pingTarget.value || pingLoading.value) return
  
  pingLoading.value = true
  pingResult.value = null
  
  try {
    const response = await fetch('/api/ping', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({ target: pingTarget.value })
    })
    
    if (response.ok) {
      const data = await response.json()
      if (data.success) {
        pingResult.value = data.latency_ms
      } else {
        pingResult.value = -1
      }
    } else {
      pingResult.value = -1
    }
  } catch (error) {
    console.error('Ping failed:', error)
    pingResult.value = -1
  } finally {
    pingLoading.value = false
  }
}

const saveSettingsClick = async () => {
  v$.value.$touch()
  if (v$.value.$error) {
    const validationTabs = [
      ['general', ['adminUsername', 'ccuIP']],
      ['network', ['hostname', 'localIP', 'netmask', 'gateway', 'dns1', 'dns2', 'ipv6Address', 'ipv6PrefixLength', 'ipv6Gateway', 'ipv6Dns1', 'ipv6Dns2']],
      ['time', ['ntpServer', 'dcfOffset']]
    ]
    const invalidGroup = validationTabs.find(([, fields]) => fields.some(field => v$.value[field]?.$error))
    if (invalidGroup) activeTab.value = invalidGroup[0]
    showError.value = t('settings.validation.fixErrors')
    await nextTick()
    document.querySelector('.tab-panel .is-invalid')?.scrollIntoView({ behavior: 'smooth', block: 'center' })
    return
  }

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
  loadSettings()
}

const performRestart = async () => {
  isRestarting.value = true
  try {
    await axios.post('/api/restart')
    showRestartModal.value = false
    uiStore.pushToast({ type: 'info', title: t('common.success'), message: t('firmware.restartingText'), duration: 1200 })
    restartUiStore.start({ includeFlashPause: true })
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
        // Sanity-check that this looks like a settings backup before POSTing
        // arbitrary keys to the device. A real backup always carries top-level
        // device-config keys (hostname/useDHCP). A wrong/truncated/malicious
        // file is rejected client-side without hitting the network.
        if (!json || typeof json !== 'object' || Array.isArray(json) ||
            !('hostname' in json) || !('useDHCP' in json)) {
          uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('settings.restoreError') })
          return
        }
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
  min-width: 0;
}

/* iOS Segmented Control */
.tabs-container {
  display: flex;
  justify-content: center;
  margin-bottom: var(--spacing-xl);
  overflow: visible;
}

.segmented-control {
  display: inline-flex;
  flex-wrap: wrap;
  background-color: var(--color-border-light);
  padding: var(--space-1);
  border-radius: var(--radius-lg);
  position: relative;
  width: 100%;
  max-width: 700px;
  gap: var(--space-1);
  min-width: 0;
}

.segment-btn {
  flex: 1 1 min(150px, 100%);
  min-width: 0;
  border: none;
  background: transparent;
  padding: var(--space-2) var(--space-4);
  border-radius: var(--radius-md);
  font-weight: var(--font-weight-semibold);
  font-size: var(--fs-sm);
  color: var(--color-text-secondary);
  cursor: pointer;
  transition: all 0.2s ease;
  position: relative;
  z-index: 1;
  text-align: center;
  gap: var(--space-2);
  display: inline-flex;
  justify-content: center;
  align-items: center;
  white-space: normal;
  overflow-wrap: anywhere;
}

.segment-label {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
}

.segment-btn.active {
  background-color: var(--color-surface);
  color: var(--color-text);
  box-shadow: var(--shadow-sm);
}

/* Settings Cards (Apple Style Grouped) */
.settings-card {
  background: var(--color-surface);
  border-radius: var(--radius-xl);
  box-shadow: var(--shadow-md);
  margin-bottom: var(--spacing-lg);
  overflow: hidden;
  min-width: 0;
}

.card-header {
  padding: var(--spacing-lg) var(--spacing-lg) 0;
  background: transparent;
  border: none;
}

.card-header h3 {
  font-size: var(--fs-lg);
  margin: 0;
  color: var(--color-text);
  font-weight: var(--font-weight-bold);
  letter-spacing: -0.01em;
  overflow-wrap: anywhere;
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
  gap: var(--spacing-md);
  min-width: 0;
}

.security-info {
  min-width: 0;
}

.security-info h4 {
  margin: 0;
  font-size: var(--fs-md);
  overflow-wrap: anywhere;
}

.security-info p {
  margin: 0;
  font-size: var(--fs-xs);
  color: var(--color-text-secondary);
  overflow-wrap: anywhere;
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
  gap: var(--spacing-md);
  min-width: 0;
}

.switch-label {
  font-size: var(--fs-md);
  font-weight: var(--font-weight-medium);
}

.switch-copy {
  display: flex;
  flex-direction: column;
  gap: 4px;
  padding-right: var(--spacing-md);
  min-width: 0;
}

.switch-copy h4 {
  margin: 0;
  font-size: var(--fs-md);
}

.switch-copy p {
  margin: 0;
  color: var(--color-text-secondary);
  font-size: var(--fs-xs);
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
  font-size: var(--fs-sm);
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
  font-size: var(--fs-xl);
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
  background: var(--color-border-strong);
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
  padding: var(--space-1);
  border-radius: var(--radius-lg);
  margin-top: var(--spacing-xs);
  min-width: 0;
}

.mode-btn {
  flex: 1;
  border: none;
  background: transparent;
  padding: var(--space-2);
  border-radius: var(--radius-md);
  font-weight: var(--font-weight-medium);
  transition: all 0.2s;
  min-width: 0;
  overflow-wrap: anywhere;
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
  min-width: 0;
  overflow-wrap: anywhere;
  text-align: center;
}

.source-card.active {
  background: var(--color-surface);
  border-color: var(--color-primary);
  box-shadow: var(--shadow-md);
}

.source-icon {
  font-size: var(--fs-3xl);
  color: var(--color-primary);
  margin-bottom: var(--spacing-xs);
  display: inline-flex;
}

.check-icon {
  position: absolute;
  top: 8px;
  right: 8px;
  color: var(--color-primary);
  font-size: var(--fs-lg);
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
  min-width: 0;
}

.action-tile:hover {
  background: var(--color-border-light);
}

.tile-icon {
  font-size: var(--fs-2xl);
  color: var(--color-primary);
  margin-right: var(--spacing-md);
  display: inline-flex;
}

.tile-content {
  flex: 1;
  min-width: 0;
}

.tile-content h4 {
  font-size: var(--fs-md);
  margin: 0;
  overflow-wrap: anywhere;
}

.tile-content p {
  margin: 0;
  font-size: var(--fs-xs);
  color: var(--color-text-secondary);
  overflow-wrap: anywhere;
}

.tile-arrow {
  color: var(--color-text-secondary);
  font-size: var(--fs-lg);
  display: inline-flex;
}

.file-input {
  display: none;
}

/* LED Programs grid - base definition (was only in mobile MQ, missing on desktop) */
.led-programs-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(min(160px, 100%), 1fr));
  gap: var(--spacing-md);
  margin-top: var(--spacing-md);
}

.led-program-item {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-xs);
}

.led-program-item .form-label.small {
  font-size: var(--fs-xs);
  font-weight: var(--font-weight-medium);
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
  min-width: 0;
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
  min-width: 0;
}

.warning-chip {
  color: var(--color-warning);
}

/* Korrekturauftrag §9: the "saved / unsaved" indicator in the hero is a
   NON-interactive status badge (not a tab). It mirrors the form dirty-state
   without looking clickable. */
.save-state {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: var(--space-1) var(--space-3);
  border-radius: var(--radius-sm);
  font-size: var(--fs-xs);
  font-weight: var(--font-weight-bold);
  border: 1px solid var(--color-border);
  background: var(--color-bg-alt);
  color: var(--color-text-secondary);
}

.save-state .app-icon {
  width: 14px;
  height: 14px;
}

.save-state.is-saved {
  color: var(--color-success);
  border-color: var(--color-success);
  background: var(--color-success-soft);
}

.save-state.is-unsaved {
  color: var(--color-warning);
  border-color: var(--color-warning);
  background: var(--color-warning-soft);
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
    justify-content: flex-start;
    margin-left: 0;
    margin-right: 0;
    padding: 0;
  }

  .segmented-control {
    width: 100%;
    flex-wrap: wrap;
  }

  .segment-btn {
    padding: 10px 16px;
    font-size: var(--fs-xs);
    flex: 1 1 min(140px, 100%);
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

  .switch-row {
    align-items: flex-start;
    flex-direction: column;
    gap: var(--spacing-sm);
  }

  .switch-copy {
    padding-right: 0;
  }

  .mode-selector {
    flex-direction: column;
  }

  .mode-btn {
    text-align: center;
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
    font-size: var(--fs-2xl);
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

  .action-tile {
    align-items: flex-start;
  }

  .tile-arrow {
    display: none;
  }

  .floating-footer {
    padding: var(--spacing-sm) var(--spacing-md);
  }

  .footer-container {
    flex-direction: column;
  }

  .footer-alert,
  .save-btn,
  .discard-btn {
    width: 100%;
  }
}

@media (max-width: 576px) {
  .segment-btn {
    padding: 8px 14px;
    font-size: var(--fs-xs);
  }
}

/* ---- Supporter key card ---- */
.supporter-card {
  border: 1px solid var(--color-border-light);
  transition: border-color 0.25s, box-shadow 0.25s;
}

.supporter-card.is-active {
  border-color: var(--color-primary-soft);
  box-shadow: 0 0 0 1px var(--color-primary-soft), 0 8px 24px var(--color-primary-soft);
}

.supporter-icon {
  background: linear-gradient(135deg, var(--color-primary), var(--color-primary-strong));
  color: #fff;
}

.supporter-badge-active {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 6px 12px;
  border-radius: var(--radius-full);
  background: linear-gradient(135deg, var(--color-primary-soft), var(--color-primary-soft));
  color: var(--color-primary-strong);
  font-size: var(--fs-xs);
  font-weight: var(--font-weight-bold);
}

.supporter-intro {
  margin: 0 0 14px;
  color: var(--color-text-secondary);
  font-size: var(--fs-sm);
  line-height: 1.5;
}

/* ---- "Thank you" medallion shown when a key is active ---- */
.supporter-thanks-panel {
  display: flex;
  align-items: center;
  gap: 22px;
  padding: 6px 2px 2px;
  flex-wrap: wrap;
}

.medallion {
  position: relative;
  width: 96px;
  height: 96px;
  flex: 0 0 auto;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  background:
    radial-gradient(circle at 30% 25%, rgba(255, 255, 255, 0.45), transparent 55%),
    linear-gradient(135deg, var(--color-primary) 0%, var(--color-primary-strong) 100%);
  box-shadow:
    0 10px 26px var(--color-primary-soft),
    inset 0 0 0 4px rgba(255, 255, 255, 0.2),
    inset 0 -8px 14px rgba(0, 0, 0, 0.12);
  animation: medallion-float 3.2s ease-in-out infinite;
}

/* Rotating conic shine sweeping around the rim */
.medallion-shine {
  position: absolute;
  inset: -3px;
  border-radius: 50%;
  background: conic-gradient(from 0deg, transparent 65%, rgba(255, 255, 255, 0.6) 82%, transparent 95%);
  animation: medallion-spin 4.5s linear infinite;
  pointer-events: none;
}

.medallion-core {
  position: relative;
  z-index: 1;
  color: #fff;
}

.medallion-core .app-icon {
  width: 46px;
  height: 46px;
  filter: drop-shadow(0 2px 3px rgba(0, 0, 0, 0.25));
}

/* Twinkling stars orbiting the badge */
.medallion-spark {
  position: absolute;
  color: var(--color-primary-strong);
  z-index: 2;
  animation: spark-twinkle 2.4s ease-in-out infinite;
}

.medallion-spark .app-icon { width: 14px; height: 14px; }
.spark-a { top: -4px; right: 8px; animation-delay: 0s; }
.spark-b { bottom: 2px; left: -2px; width: 18px; animation-delay: 0.8s; }
.spark-b .app-icon { width: 18px; height: 18px; }
.spark-c { top: 26px; right: -6px; animation-delay: 1.6s; }

.medallion-text {
  flex: 1 1 220px;
  min-width: 0;
}

.medallion-label {
  display: inline-block;
  font-size: var(--fs-2xs);
  font-weight: var(--font-weight-heavy);
  letter-spacing: 0.18em;
  text-transform: uppercase;
  color: var(--color-primary-strong);
  background: var(--color-primary-soft);
  padding: 3px 10px;
  border-radius: var(--radius-full);
  margin-bottom: 8px;
}

.medallion-title {
  display: block;
  font-size: var(--fs-lg);
  line-height: 1.25;
}

.medallion-body {
  margin: 6px 0 10px;
  color: var(--color-text-secondary);
  font-size: var(--fs-xs);
  line-height: 1.45;
}

.supporter-remove {
  font-size: var(--fs-xs);
}

@keyframes medallion-float {
  0%, 100% { transform: translateY(0); }
  50% { transform: translateY(-5px); }
}

@keyframes medallion-spin {
  to { transform: rotate(360deg); }
}

@keyframes spark-twinkle {
  0%, 100% { opacity: 0.25; transform: scale(0.8); }
  50% { opacity: 1; transform: scale(1.15); }
}

.supporter-input-row {
  display: flex;
  gap: 10px;
  align-items: stretch;
}

.supporter-input {
  flex: 1 1 auto;
  font-family: var(--font-mono);
  letter-spacing: 0.04em;
  text-transform: uppercase;
}

.supporter-activate-btn {
  flex: 0 0 auto;
  white-space: nowrap;
}

.supporter-feedback {
  margin-top: 8px;
  font-size: var(--fs-xs);
  font-weight: var(--font-weight-semibold);
}

.supporter-feedback.valid { color: var(--color-success); }
.supporter-feedback.invalid { color: var(--color-danger); }
.supporter-feedback.expired { color: var(--color-warning); }

.supporter-remove {
  font-size: var(--fs-xs);
}

/* ---- revoked state ---- */
.supporter-revoked-panel {
  display: flex;
  align-items: center;
  gap: var(--space-4);
  padding: 14px;
  border-radius: var(--radius-lg);
  background: var(--color-danger-soft);
  border: 1px solid color-mix(in srgb, var(--color-danger) 30%, transparent);
}

.revoked-icon-wrap {
  width: 44px;
  height: 44px;
  flex: 0 0 auto;
  border-radius: var(--radius-lg);
  display: inline-flex;
  align-items: center;
  justify-content: center;
  background: var(--color-danger);
  color: #fff;
}

.revoked-text strong {
  display: block;
  font-size: var(--fs-md);
  color: var(--color-danger);
}

.revoked-text p {
  margin: 4px 0 8px;
  color: var(--color-text-secondary);
  font-size: var(--fs-xs);
  line-height: 1.4;
}

.supporter-footnote {
  margin-top: 12px;
  font-size: var(--fs-xs);
}

@media (max-width: 576px) {
  .supporter-input-row {
    flex-direction: column;
  }

  .supporter-activate-btn {
    width: 100%;
  }
}
</style>
