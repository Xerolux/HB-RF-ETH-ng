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
import { onBeforeUnmount, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { useFirmwareUpdateStore, useRestartUiStore, useSysInfoStore, useUiStore } from './stores.js'
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
  triggerMessage,
  resultMessage,
  checkForUpdates,
  loadUpdateStatus,
  stopPolling,
  formatTimestamp
} = useUpdateSearch(t)

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
