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
          <p class="muted-text">{{ t('firmware.onlineManualNote') }}</p>

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
            <BButton variant="success" class="action-btn" :disabled="checking" @click="checkForUpdates">
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
</div>
</template>

<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import axios from 'axios'
import { useI18n } from 'vue-i18n'
import { useFirmwareUpdateStore, useRestartUiStore, useSysInfoStore, useUiStore } from './stores.js'
import { safeLocal } from './composables/useSafeStorage'

const WEBUI_IMAGE_SIZE = 0x50000
const ESP_IMAGE_MAGIC = 0xe9

const { t } = useI18n()
const firmwareUpdateStore = useFirmwareUpdateStore()
const restartUiStore = useRestartUiStore()
const sysInfoStore = useSysInfoStore()
const uiStore = useUiStore()

// --- Online update search --------------------------------------------------
// The device only searches when asked. The channel is a request parameter
// rather than a device setting: with no automatic schedule there is nothing
// on the device that would need to remember it, so it stays a per-browser
// preference and the firmware writes no NVS for it.
const CHANNEL_STORAGE_KEY = 'hb-rf-eth-ng-update-channel'
const POLL_INTERVAL_MS = 1500
const POLL_TIMEOUT_MS = 45000

const channel = ref(safeLocal.get(CHANNEL_STORAGE_KEY) === 'beta' ? 'beta' : 'stable')
const checking = ref(false)
const updateStatus = ref({
  everChecked: false,
  lastCheck: 0,
  latestFirmware: '',
  latestWebui: '',
  firmwareUpdateAvailable: false,
  webuiUpdateAvailable: false,
  notesUrl: '',
  lastError: '',
  lastSkipReason: ''
})
const triggerOutcome = ref('')
let pollTimer = null

const formatTimestamp = seconds => {
  const value = Number(seconds) || 0
  if (value <= 0) return '—'
  return new Date(value * 1000).toLocaleString()
}

// Why the press did not start a search. Kept separate from the result below so
// a cooldown never hides the finding the user is looking at - "I clicked and
// the page told me nothing useful" was a real complaint about the old page.
const triggerMessage = computed(() => {
  switch (triggerOutcome.value) {
    case 'cooldown': return { variant: 'info', text: t('firmware.checkCooldown') }
    case 'busy': return { variant: 'info', text: t('firmware.checkBusy') }
    case 'unavailable': return { variant: 'warning', text: t('firmware.checkUnavailable') }
    default: return { variant: '', text: '' }
  }
})

// One message, one meaning. A skip and a failure are shown as themselves and
// never collapse into the reassuring "everything is current" case.
const checkMessage = computed(() => {
  const status = updateStatus.value
  if (status.lastSkipReason) {
    return { variant: 'warning', text: t('firmware.checkSkipped', { reason: status.lastSkipReason }) }
  }
  if (status.lastError) {
    return { variant: 'danger', text: t('firmware.checkFailed', { reason: status.lastError }) }
  }
  if (!status.everChecked) return { variant: '', text: '' }
  if (status.firmwareUpdateAvailable || status.webuiUpdateAvailable) {
    const parts = []
    if (status.firmwareUpdateAvailable) {
      parts.push(t('firmware.firmwareAvailable', { version: status.latestFirmware }))
    }
    if (status.webuiUpdateAvailable) {
      parts.push(t('firmware.webuiAvailable', { version: status.latestWebui }))
    }
    parts.push(t('firmware.installHint'))
    return { variant: 'success', text: parts.join(' ') }
  }
  return {
    variant: 'success',
    text: t('firmware.upToDate', { fw: status.latestFirmware, ui: status.latestWebui })
  }
})

const loadUpdateStatus = async () => {
  const response = await axios.get(`/api/update/status?t=${Date.now()}`, { timeout: 8000, silent: true })
  updateStatus.value = response.data || updateStatus.value
  return response.data
}

const stopPolling = () => {
  if (pollTimer) {
    clearTimeout(pollTimer)
    pollTimer = null
  }
}

const checkForUpdates = async () => {
  if (checking.value) return
  checking.value = true
  triggerOutcome.value = ''
  safeLocal.set(CHANNEL_STORAGE_KEY, channel.value)

  try {
    const response = await axios.post('/api/update/check', { channel: channel.value },
                                      { timeout: 8000, silent: true })
    const outcome = response.data?.outcome || 'accepted'
    if (outcome !== 'accepted') {
      // Rejected before any network activity - report it and stop; there is
      // no result coming that polling could wait for.
      triggerOutcome.value = outcome
      await loadUpdateStatus().catch(() => {})
      checking.value = false
      return
    }
  } catch {
    triggerOutcome.value = 'unavailable'
    checking.value = false
    return
  }

  // The device answers immediately and does the work on its own task, so the
  // outcome is polled rather than awaited on the request.
  const deadline = Date.now() + POLL_TIMEOUT_MS
  const poll = async () => {
    let status = null
    try {
      status = await loadUpdateStatus()
    } catch {
      // A transient read failure is not a failed search; keep polling until
      // the deadline and let the device's own state have the last word.
    }
    if (status && status.state !== 'running') {
      checking.value = false
      return
    }
    if (Date.now() >= deadline) {
      checking.value = false
      return
    }
    pollTimer = setTimeout(poll, POLL_INTERVAL_MS)
  }
  pollTimer = setTimeout(poll, POLL_INTERVAL_MS)
}

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
    restartUiStore.start({ includeFlashPause: true, syncSeconds: 120, restartSeconds: 30 })
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
@media(max-width:900px){ .content-grid { grid-template-columns:1fr; } }
</style>
