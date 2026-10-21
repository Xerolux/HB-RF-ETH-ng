<template>
  <div class="firmware-page page-shell">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow"><AppIcon name="firmware" /> {{ t('updates.firmware') }}</span>
        <h1 class="hero-title">{{ t('firmware.pageTitle') }}</h1>
        <p class="hero-subtitle">{{ t('firmware.pageDescription') }}</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="firmware" /> {{ t('firmware.installedLabel') }}: {{ sysInfoStore.currentVersion || '—' }}</span>
      </div>
    </section>

    <div class="content-grid">
      <section class="update-card">
        <div class="card-header">
          <div class="header-icon bg-success-light text-success"><AppIcon name="externalLink" /></div>
          <div class="header-text">
            <span class="kicker">{{ t('firmware.onlineKicker') }}</span>
            <h2>{{ t('firmware.onlineHeading') }}</h2>
            <p>{{ t('firmware.onlineHelp') }}</p>
          </div>
        </div>
        <div class="card-body">
          <p class="muted-text">{{ manualNoteText }}</p>

          <!-- One-shot install scheduled from the confirm dialog: shown as long
               as the entry exists, cancellable at any time. -->
          <BAlert v-if="updateStatus.schedule.pending" variant="info" :model-value="true">
            {{ t('firmware.pendingScheduled', {
              target: updateStatus.schedule.pending.target === 'firmware' ? t('updates.firmware') : t('updates.webui'),
              version: updateStatus.schedule.pending.version
            }) }}
            <BButton variant="outline-secondary" size="sm" class="ms-2" :disabled="cancellingPending" @click="cancelPending">
              {{ t('firmware.pendingCancel') }}
            </BButton>
          </BAlert>
          <p v-if="updateStatus.schedule.stage" class="muted-text">
            {{ t('firmware.stagePending', {
              firmware: updateStatus.schedule.stage.firmware,
              webui: updateStatus.schedule.stage.webui
            }) }}
          </p>

          <div class="channel-row">
            <label class="channel-label" for="update-channel">{{ t('firmware.channelLabel') }}</label>
            <select id="update-channel" v-model="channel" class="form-select channel-select" :disabled="checking">
              <option value="stable">{{ t('firmware.channelStable') }}</option>
              <option value="beta">{{ t('firmware.channelBeta') }}</option>
            </select>
          </div>

          <!-- Outcome is rendered as durable page content, never only as a
               toast: a result that vanishes after a few seconds is how a
               skipped search once looked identical to "no update found". -->
          <BAlert v-if="triggerMessage.text" :variant="triggerMessage.variant" :model-value="true">
            {{ triggerMessage.text }}
          </BAlert>
          <BAlert v-if="checkMessage.text" :variant="checkMessage.variant" :model-value="true">
            {{ checkMessage.text }}
          </BAlert>
          <p v-if="!checkMessage.text" class="muted-text">{{ t('firmware.neverChecked') }}</p>

          <p v-if="updateStatus.lastCheck" class="muted-text">
            {{ t('firmware.lastCheck') }}: {{ formatTimestamp(updateStatus.lastCheck) }}
          </p>

          <div class="actions">
            <BButton variant="success" class="action-btn" :disabled="checking || installBusy" @click="checkForUpdates">
              <span v-if="checking" class="spinner-border spinner-border-sm me-2"></span>
              <AppIcon v-else name="refresh" /> {{ checking ? t('firmware.checking') : t('firmware.checkNow') }}
            </BButton>
            <a
              class="btn btn-outline-secondary action-btn"
              :href="updateStatus.notesUrl || 'https://github.com/Xerolux/HB-RF-ETH-ng/releases'"
              target="_blank"
              rel="noopener noreferrer"
            >
              <AppIcon name="externalLink" />
              {{ updateStatus.notesUrl ? t('firmware.releaseNotes') : t('firmware.viewOnGithub') }}
            </a>
          </div>

          <!-- Online install: the device itself downloads the release named by
               its own manifest, verifies SHA-256 and reboots to activate. -->
          <div v-if="updateStatus.firmwareUpdateAvailable || installPanelVisible" class="install-panel">
            <BButton v-if="!installBusy" variant="primary" class="action-btn"
                     :disabled="checking || installStarting" @click="openInstallConfirm">
              <AppIcon name="download" /> {{ t('firmware.installNow') }}
            </BButton>
            <template v-else>
              <div class="progress-container">
                <div class="progress-bar"><div class="progress-value" :style="{ width: installProgressPct + '%' }"></div></div>
                <span class="progress-label">{{ installProgressPct }}%</span>
              </div>
              <p class="muted-text">{{ installStateText }}</p>
            </template>
            <BAlert v-if="installError" variant="danger" :model-value="true">
              {{ t('firmware.installFailed') }} {{ installError }}
            </BAlert>
          </div>
        </div>
      </section>

      <section class="update-card" data-testid="auto-update-card">
        <div class="card-header">
          <div class="header-icon bg-info-light text-info"><AppIcon name="refresh" /></div>
          <div class="header-text">
            <span class="kicker">{{ t('firmware.autoKicker') }}</span>
            <h2>{{ t('firmware.autoHeading') }}</h2>
            <p>{{ t('firmware.autoHelp') }}</p>
          </div>
        </div>
        <div class="card-body">
          <template v-if="configLoaded">
            <div class="auto-grid">
              <div class="form-group">
                <label class="form-label" for="auto-mode">{{ t('firmware.autoMode') }}</label>
                <select id="auto-mode" v-model="autoMode" class="form-select" :disabled="configSaving">
                  <option value="off">{{ t('firmware.autoModeOff') }}</option>
                  <option value="notify">{{ t('firmware.autoModeNotify') }}</option>
                  <option value="auto_webui">{{ t('firmware.autoModeWebui') }}</option>
                  <option value="auto_all">{{ t('firmware.autoModeAll') }}</option>
                </select>
                <div class="form-text">{{ autoModeHelpText }}</div>
              </div>
              <div v-if="autoMode !== 'off'" class="form-group">
                <label class="form-label" for="auto-interval">{{ t('firmware.autoInterval') }}</label>
                <BFormInput
                  id="auto-interval" type="number" v-model.number="autoInterval"
                  min="6" max="168" :state="autoIntervalValid ? null : false" :disabled="configSaving"
                />
                <BFormInvalidFeedback v-if="!autoIntervalValid">{{ t('firmware.autoIntervalInvalid') }}</BFormInvalidFeedback>
              </div>
            </div>
            <template v-if="autoMode !== 'off'">
              <div class="auto-grid">
                <div class="form-group">
                  <label class="form-label" for="auto-win-start">{{ t('firmware.autoWindowStart') }}</label>
                  <select id="auto-win-start" v-model.number="autoWinStart" class="form-select" :disabled="configSaving">
                    <option v-for="hour in 24" :key="hour - 1" :value="hour - 1">{{ String(hour - 1).padStart(2, '0') }}:00</option>
                  </select>
                </div>
                <div class="form-group">
                  <label class="form-label" for="auto-win-len">{{ t('firmware.autoWindowLength') }}</label>
                  <select id="auto-win-len" v-model.number="autoWinLen" class="form-select" :disabled="configSaving">
                    <option v-for="hours in 24" :key="hours" :value="hours">{{ t('firmware.autoWindowHours', { hours }) }}</option>
                  </select>
                  <div class="form-text">{{ windowText }}</div>
                </div>
              </div>
              <div class="form-check form-switch">
                <input class="form-check-input" type="checkbox" role="switch" id="auto-idle"
                       v-model="autoIdle" :disabled="configSaving" />
                <label class="form-check-label" for="auto-idle">{{ t('firmware.autoIdleLabel') }}</label>
                <div class="form-text">{{ t('firmware.autoIdleHelp') }}</div>
              </div>
            </template>
            <BAlert v-if="blockedHint" variant="info" :model-value="true">{{ blockedHint }}</BAlert>
            <p v-if="nextCheckText" class="muted-text">{{ nextCheckText }}</p>
            <div class="actions">
              <BButton variant="primary" class="action-btn"
                       :disabled="!autoDirty || !autoValid || configSaving" @click="saveAutoConfig">
                <span v-if="configSaving" class="spinner-border spinner-border-sm me-2"></span>
                {{ t('firmware.autoSave') }}
              </BButton>
            </div>
          </template>
          <p v-else class="muted-text">{{ t('firmware.autoUnavailable') }}</p>
        </div>
      </section>

      <section class="update-card">
        <div class="card-header">
          <div class="header-icon bg-primary-light text-primary"><AppIcon name="upload" /></div>
          <div class="header-text">
            <span class="kicker">{{ t('firmware.manualKicker') }}</span>
            <h2>{{ t('firmware.manualHeading') }}</h2>
            <p>{{ t('firmware.manualHelp') }}</p>
          </div>
        </div>
        <div class="card-body">
          <label class="upload-zone" :class="{ 'has-file': file, dragging: isDragging, invalid: !!fileError }"
                 @dragover.prevent="isDragging = true"
                 @dragleave.prevent="isDragging = false"
                 @drop.prevent="handleDrop">
            <input ref="fileInput" type="file" accept=".bin,application/octet-stream" class="hidden-input" @change="handleFileSelect">
            <template v-if="!file">
              <div class="upload-icon"><AppIcon name="upload" /></div>
              <span class="upload-text">{{ t('firmware.selectFirmwareBin') }}</span>
            </template>
            <template v-else>
              <div class="file-preview">
                <div class="file-icon"><AppIcon name="file" /></div>
                <div class="file-details">
                  <span class="file-name">{{ file.name }}</span>
                  <span class="file-size">{{ formatSize(file.size) }}</span>
                </div>
                <button type="button" class="remove-file-btn" @click.stop.prevent="clearFile"><AppIcon name="close" /></button>
              </div>
            </template>
          </label>

          <BAlert v-if="fileError" variant="danger" :model-value="true">{{ fileError }}</BAlert>

          <div v-if="uploadProgress > 0" class="progress-container">
            <div class="progress-bar"><div class="progress-value" :style="{ width: uploadProgress + '%' }"></div></div>
            <span class="progress-label">{{ uploadProgress }}%</span>
          </div>

          <BAlert variant="warning" :model-value="true">
            {{ t('firmware.writeWarning') }}
          </BAlert>

          <BButton variant="primary" size="lg" block class="action-btn"
                   :disabled="!file || !!fileError || uploading" @click="uploadFirmware">
            <span v-if="uploading" class="spinner-border spinner-border-sm me-2"></span>
            <AppIcon v-else name="upload" /> {{ uploading ? t('firmware.uploading') : t('firmware.upload') }}
          </BButton>
        </div>
      </section>
    </div>

    <!-- Changelog-before-install confirmation. The notes are rendered as
         plain text (no HTML renderer in this project by design). -->
    <Transition name="fade">
      <div v-if="showInstallConfirm" class="modal-backdrop" @click.self="closeInstallConfirm">
        <div class="modal-card" role="dialog" aria-modal="true">
          <h3>{{ t('firmware.installConfirmTitle', { version: updateStatus.latestFirmware }) }}</h3>
          <p class="muted-text">{{ t('firmware.installConfirmBody') }}</p>
          <div class="changelog-box">
            <h4>{{ t('firmware.installChangelogTitle') }}</h4>
            <p v-if="notesLoading" class="muted-text">{{ t('firmware.installChangelogLoading') }}</p>
            <pre v-else-if="releaseNotes" class="changelog-text">{{ releaseNotes }}</pre>
            <p v-else class="muted-text">
              {{ t('firmware.installChangelogMissing') }}
              <a :href="updateStatus.notesUrl" target="_blank" rel="noopener noreferrer">{{ t('firmware.releaseNotes') }}</a>
            </p>
          </div>
          <div class="modal-actions">
            <BButton variant="outline-secondary" :disabled="installStarting" @click="closeInstallConfirm">
              {{ t('common.cancel') }}
            </BButton>
            <BButton variant="outline-primary" :disabled="installStarting" @click="confirmScheduleInstall">
              {{ t('firmware.installScheduleButton') }}
            </BButton>
            <BButton variant="primary" :disabled="installStarting" @click="confirmInstall">
              <span v-if="installStarting" class="spinner-border spinner-border-sm me-2"></span>
              {{ t('firmware.installConfirmButton') }}
            </BButton>
          </div>
        </div>
      </div>
    </Transition>
  </div>
</template>

<script setup>
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useFirmwareUpdateStore, useRestartUiStore, useSysInfoStore, useUiStore, FLASH_PAUSE_SECONDS } from './stores.js'
import { useUpdateSearch } from './composables/useUpdateSearch'

const WEBUI_IMAGE_SIZE = 0x50000
const ESP_IMAGE_MAGIC = 0xe9

const { t } = useI18n()
const firmwareUpdateStore = useFirmwareUpdateStore()
const restartUiStore = useRestartUiStore()
const sysInfoStore = useSysInfoStore()
const uiStore = useUiStore()

// --- Online update search --------------------------------------------------
// Shared with the WebUI update page: one device-side search answers both, and
// the logic lives in one place so a defect is fixed once. See
// composables/useUpdateSearch.js.
const {
  channel,
  checking,
  updateStatus,
  updateConfig,
  triggerMessage,
  resultMessage,
  checkForUpdates,
  loadUpdateStatus,
  loadUpdateConfig,
  saveUpdateConfig,
  scheduleInstall,
  cancelScheduledInstall,
  stopPolling,
  formatTimestamp,
  startInstall,
  installBusy,
  fetchReleaseNotes
} = useUpdateSearch(t)

// --- Automatic update configuration (stages B2/C/D) -------------------------
// The card mirrors GET/POST /api/update/config. Older firmware (no endpoint)
// leaves configLoaded false and shows the unavailable note instead of
// guessing defaults.
const configSaving = ref(false)
const autoMode = ref('off')
const autoInterval = ref(24)
const autoWinStart = ref(3)
const autoWinLen = ref(2)
const autoIdle = ref(true)
let autoSnapshot = ''

const configLoaded = computed(() => updateConfig.value !== null)
const autoIntervalValid = computed(() =>
  Number.isInteger(autoInterval.value) && autoInterval.value >= 6 && autoInterval.value <= 168)
const autoValid = computed(() => autoIntervalValid.value)
const autoSerialized = computed(() => JSON.stringify({
  mode: autoMode.value,
  intervalHours: autoInterval.value,
  windowStart: autoWinStart.value,
  windowLength: autoWinLen.value,
  idleOnly: autoIdle.value
}))
const autoDirty = computed(() => configLoaded.value && autoSerialized.value !== autoSnapshot)

const applyConfigToForm = config => {
  if (!config) return
  autoMode.value = config.mode || 'off'
  autoInterval.value = Number(config.intervalHours) || 24
  autoWinStart.value = Number(config.windowStart) || 0
  autoWinLen.value = Number(config.windowLength) || 2
  autoIdle.value = config.idleOnly !== false
  autoSnapshot = autoSerialized.value
}

const saveAutoConfig = async () => {
  if (!autoValid.value || configSaving.value) return
  configSaving.value = true
  try {
    const saved = await saveUpdateConfig({
      mode: autoMode.value,
      intervalHours: autoInterval.value,
      windowStart: autoWinStart.value,
      windowLength: autoWinLen.value,
      idleOnly: autoIdle.value
    })
    applyConfigToForm(saved)
    uiStore.pushToast({ type: 'success', title: t('common.success'), message: t('firmware.autoSaveSuccess'), duration: 2500 })
  } catch {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('firmware.autoSaveError'), duration: 5000 })
  } finally {
    configSaving.value = false
  }
}

// Dynamic mode descriptions: the keys live in every locale; the map keeps the
// template free of string building.
const AUTO_MODE_HELP_KEYS = {
  off: 'firmware.autoModeOffHelp',
  notify: 'firmware.autoModeNotifyHelp',
  auto_webui: 'firmware.autoModeWebuiHelp',
  auto_all: 'firmware.autoModeAllHelp'
}
const autoModeHelpText = computed(() => t(AUTO_MODE_HELP_KEYS[autoMode.value] || AUTO_MODE_HELP_KEYS.off))

const pad2 = value => String(value).padStart(2, '0')
const windowText = computed(() => t('firmware.autoWindowRange', {
  start: `${pad2(autoWinStart.value)}:00`,
  end: `${pad2((autoWinStart.value + autoWinLen.value) % 24)}:00`
}))

const nextCheckText = computed(() => {
  const schedule = updateStatus.value.schedule
  if (schedule.mode === 'off' || !schedule.nextCheckIn) return ''
  const minutes = Math.max(1, Math.round(schedule.nextCheckIn / 60))
  const humanized = minutes >= 90 ? `~${Math.round(minutes / 60)} h` : `~${minutes} min`
  return t('firmware.autoNextCheck', { time: humanized })
})

const BLOCKED_KEYS = {
  no_valid_time: 'firmware.blockedNoValidTime',
  outside_window: 'firmware.blockedOutsideWindow',
  session_active: 'firmware.blockedSessionActive',
  busy: 'firmware.blockedBusy'
}
const blockedHint = computed(() => {
  const blocked = updateStatus.value.schedule.installBlocked
  return blocked && BLOCKED_KEYS[blocked] ? t(BLOCKED_KEYS[blocked]) : ''
})

const manualNoteText = computed(() => updateStatus.value.schedule.mode && updateStatus.value.schedule.mode !== 'off'
  ? t('firmware.onlineScheduledNote', { interval: updateStatus.value.schedule.intervalHours })
  : t('firmware.onlineManualNote'))

const cancellingPending = ref(false)
const cancelPending = async () => {
  if (cancellingPending.value) return
  cancellingPending.value = true
  const outcome = await cancelScheduledInstall()
  cancellingPending.value = false
  if (outcome === 'cancelled' || outcome === 'nothing_scheduled') {
    uiStore.pushToast({ type: 'success', title: t('common.success'), message: t('firmware.pendingCancelled'), duration: 2500 })
  } else {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('firmware.scheduleFailed'), duration: 5000 })
  }
}

const checkMessage = resultMessage(status => {
  if (status.firmwareUpdateAvailable) {
    return {
      variant: 'success',
      text: `${t('firmware.firmwareAvailable', { version: status.latestFirmware })} ${t('firmware.installHint')}`
    }
  }
  return {
    variant: 'success',
    text: t('firmware.upToDate', { fw: status.latestFirmware, ui: status.latestWebui })
  }
})

// --- Online install --------------------------------------------------------
// The changelog is fetched from the Pages notes mirror before the admin
// confirms; plain markdown text, never rendered as HTML.
const showInstallConfirm = ref(false)
const installStarting = ref(false)
const notesLoading = ref(false)
const releaseNotes = ref('')

const firmwareInstall = computed(() => (
  updateStatus.value.install.target === 'firmware' ? updateStatus.value.install : { state: 'idle', progress: 0, error: '' }
))
const installProgressPct = computed(() => firmwareInstall.value.progress || 0)
const installError = computed(() => firmwareInstall.value.error || '')
const installPanelVisible = computed(() =>
  ['downloading', 'verifying', 'preparing', 'finalizing', 'error'].includes(firmwareInstall.value.state))

const installStateText = computed(() => {
  switch (firmwareInstall.value.state) {
    case 'preparing': return t('firmware.installPreparing')
    case 'downloading': return t('firmware.installDownloading')
    case 'verifying': return t('firmware.installVerifying')
    case 'finalizing': return t('firmware.installFinalizing')
    default: return ''
  }
})

const openInstallConfirm = async () => {
  showInstallConfirm.value = true
  releaseNotes.value = ''
  notesLoading.value = true
  releaseNotes.value = await fetchReleaseNotes(updateStatus.value.latestFirmware) || ''
  notesLoading.value = false
}

const closeInstallConfirm = () => {
  if (!installStarting.value) showInstallConfirm.value = false
}

const confirmInstall = async () => {
  installStarting.value = true
  const outcome = await startInstall('firmware')
  installStarting.value = false
  if (outcome === 'accepted') {
    showInstallConfirm.value = false
    return
  }
  uiStore.pushToast({
    type: 'error',
    title: t('firmware.installFailedTitle'),
    message: t('firmware.installStartFailed'),
    duration: 6000
  })
}

// Schedule exactly this update for the next maintenance window instead of
// installing now. The version always comes from the search result the
// operator is looking at; the device re-verifies the manifest at window time.
const confirmScheduleInstall = async () => {
  installStarting.value = true
  const outcome = await scheduleInstall('firmware')
  installStarting.value = false
  showInstallConfirm.value = false
  if (outcome === 'scheduled') {
    uiStore.pushToast({
      type: 'success',
      title: t('firmware.pendingScheduledTitle'),
      message: t('firmware.pendingToast', { version: updateStatus.value.latestFirmware }),
      duration: 5000
    })
  } else if (outcome === 'no_search_result') {
    uiStore.pushToast({
      type: 'info',
      title: t('firmware.pendingScheduledTitle'),
      message: t('firmware.scheduleNoSearch'),
      duration: 5000
    })
  } else {
    uiStore.pushToast({
      type: 'error',
      title: t('firmware.installFailedTitle'),
      message: t('firmware.scheduleFailed'),
      duration: 6000
    })
  }
}

// "finalizing" means the image is verified and selected: the restart countdown
// overlay takes over from here, exactly like the manual upload flow (40 s
// covers the firmware's fixed 35 s Ethernet link-down window).
watch(() => updateStatus.value.install.state, state => {
  if (state === 'finalizing' && updateStatus.value.install.target === 'firmware') {
    restartUiStore.start({ includeFlashPause: true, syncSeconds: FLASH_PAUSE_SECONDS, restartSeconds: 30 })
  }
})

onBeforeUnmount(stopPolling)

const file = ref(null)
const fileInput = ref(null)
const fileError = ref('')
const isDragging = ref(false)
const uploading = ref(false)
const uploadProgress = ref(0)

const formatSize = bytes => {
  const value = Number(bytes) || 0
  if (value < 1024) return `${value} B`
  if (value < 1024 * 1024) return `${(value / 1024).toFixed(1)} KB`
  return `${(value / 1024 / 1024).toFixed(2)} MB`
}

const clearFile = () => {
  file.value = null
  fileError.value = ''
  if (fileInput.value) fileInput.value.value = ''
}

const validateFirmwareFile = async selectedFile => {
  fileError.value = ''
  file.value = null
  if (!selectedFile) return

  const name = String(selectedFile.name || '').toLowerCase()
  if (!name.endsWith('.bin')) {
    fileError.value = t('firmware.fileInvalidExtension')
    return
  }
  if (name.startsWith('webui_') || name === 'spiffs.bin' || Number(selectedFile.size) === WEBUI_IMAGE_SIZE) {
    fileError.value = t('firmware.fileIsWebui')
    return
  }
  if (selectedFile.size < 1024) {
    fileError.value = t('firmware.fileTooSmall')
    return
  }

  try {
    const firstByte = new Uint8Array(await selectedFile.slice(0, 1).arrayBuffer())[0]
    if (firstByte !== ESP_IMAGE_MAGIC) {
      fileError.value = t('firmware.fileInvalidMagic')
      return
    }
  } catch {
    fileError.value = t('firmware.fileReadError')
    return
  }

  file.value = selectedFile
}

const handleFileSelect = event => validateFirmwareFile(event.target.files?.[0])
const handleDrop = event => {
  isDragging.value = false
  validateFirmwareFile(event.dataTransfer.files?.[0])
}

const uploadFirmware = async () => {
  if (!file.value || fileError.value || uploading.value) return
  uploading.value = true
  uploadProgress.value = 0
  try {
    await firmwareUpdateStore.update(file.value, {
      onUploadProgress: event => {
        if (event.total) uploadProgress.value = Math.round(event.loaded * 100 / event.total)
      }
    })
    uiStore.pushToast({ type: 'success', title: t('firmware.uploadCompleteTitle'), message: t('firmware.uploadCompleteMessage'), duration: 2500 })
    // 40 s covers the firmware's fixed 35 s Ethernet link-down window (see
    // main/system_reset.cpp); the following restart phase covers the boot.
    restartUiStore.start({ includeFlashPause: true, syncSeconds: FLASH_PAUSE_SECONDS, restartSeconds: 30 })
  } catch (error) {
    const message = typeof error.response?.data === 'string'
      ? error.response.data
      : (error.response?.data?.error || error.message || t('firmware.uploadFailedMessage'))
    uiStore.pushToast({ type: 'error', title: t('firmware.uploadFailedTitle'), message, duration: 7000 })
  } finally {
    uploading.value = false
    uploadProgress.value = 0
  }
}

onMounted(async () => {
  try { await sysInfoStore.update() } catch { /* Anzeige bleibt mit Platzhalter nutzbar. */ }
  // Show whatever the device already knows, without starting a search.
  try { await loadUpdateStatus() } catch { /* Karte zeigt dann "noch keine Suche". */ }
  // Load the automatic-update configuration (no search request is triggered;
  // firmware without the endpoint leaves the card in its unavailable state).
  await loadUpdateConfig()
  applyConfigToForm(updateConfig.value)
})
</script>

<style scoped>
.content-grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:var(--card-padding); }
.channel-row { display:flex; align-items:center; gap:var(--space-3); margin-bottom:var(--space-3); }
.channel-label { color:var(--color-text-secondary); font-size:var(--fs-sm); font-weight:var(--font-weight-medium); margin:0; }
.channel-select { max-width:200px; }
.update-card { background:var(--color-surface); border:1px solid var(--color-border); border-radius:var(--radius-lg); overflow:hidden; }
.card-header { display:flex; gap:14px; align-items:flex-start; padding:var(--card-padding); border-bottom:1px solid var(--color-border); }
.header-icon { width:44px; height:44px; flex:0 0 auto; border-radius:var(--radius-md); display:flex; align-items:center; justify-content:center; }
.header-text { min-width:0; flex:1; }
.header-text h2 { margin:2px 0 0; font-size: var(--fs-lg); font-weight: var(--font-weight-semibold); }
.header-text p { margin:.35rem 0 0; color:var(--color-text-secondary); }
.kicker { color:var(--color-primary-strong); font-size:var(--fs-2xs); font-weight:var(--font-weight-heavy); text-transform:uppercase; letter-spacing:.04em; }
.card-body { padding:var(--card-padding); display:flex; flex-direction:column; gap:var(--space-4); }
.muted-text { margin:0; color:var(--color-text-secondary); }
.actions { display:flex; flex-wrap:wrap; gap:10px; }
.action-btn { display:inline-flex; align-items:center; justify-content:center; gap:var(--space-2); text-decoration:none; }
.upload-zone { min-height:150px; border:2px dashed var(--color-border-strong); border-radius:var(--radius-lg); display:flex; align-items:center; justify-content:center; cursor:pointer; padding:18px; text-align:center; }
.upload-zone.dragging { border-color:var(--color-primary); background:var(--color-primary-soft); }
.upload-zone.invalid { border-color:var(--color-danger); }
.hidden-input { display:none; }
.upload-icon { font-size: var(--fs-3xl); margin-bottom:8px; }
.upload-text { display:block; font-weight: var(--font-weight-bold); }
.file-preview { width:100%; display:flex; gap:var(--space-3); align-items:center; text-align:left; }
.file-details { min-width:0; flex:1; display:flex; flex-direction:column; }
.file-name { overflow-wrap:anywhere; font-weight: var(--font-weight-bold); }
.file-size { color:var(--color-text-secondary); font-size: var(--fs-xs); }
.remove-file-btn { border:0; background:transparent; color:var(--color-danger); padding:var(--space-2); }
.progress-container { display:flex; align-items:center; gap:10px; }
.progress-bar { flex:1; height:10px; border-radius:var(--radius-pill); background:var(--color-bg-alt); overflow:hidden; }
.progress-value { height:100%; background:var(--color-primary); }
.install-panel { display:flex; flex-direction:column; gap:var(--space-3); }
.auto-grid { display:grid; grid-template-columns:1fr 1fr; gap:var(--space-3); }
.form-text { color:var(--color-text-secondary); font-size:var(--fs-xs); }
@media(max-width:600px){ .auto-grid { grid-template-columns:1fr; } }
.modal-backdrop { position:fixed; inset:0; background:rgba(0,0,0,.55); display:flex; align-items:center; justify-content:center; z-index:1050; padding:var(--card-padding); }
.modal-card { background:var(--color-surface); border-radius:var(--radius-lg); max-width:640px; width:100%; max-height:85vh; overflow-y:auto; padding:calc(var(--card-padding) * 1.25); display:flex; flex-direction:column; gap:var(--space-3); }
.modal-card h3 { margin:0; font-size:var(--fs-lg); }
.modal-card h4 { margin:0 0 var(--space-2); font-size:var(--fs-md); }
.modal-actions { display:flex; justify-content:flex-end; gap:10px; }
.changelog-box { border:1px solid var(--color-border); border-radius:var(--radius-md); padding:var(--space-3); background:var(--color-bg-alt); }
.changelog-text { margin:0; white-space:pre-wrap; overflow-wrap:anywhere; font-family:var(--font-mono); font-size:var(--fs-xs); max-height:40vh; overflow-y:auto; }
@media(max-width:900px){ .content-grid { grid-template-columns:1fr; } }
</style>
