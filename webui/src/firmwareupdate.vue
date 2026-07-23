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
        <span class="meta-chip"><AppIcon name="clock" /> {{ lastCheckText }}</span>
      </div>
    </section>

    <div class="content-grid">
      <section class="update-card">
        <div class="card-header">
          <div class="header-icon bg-success-light text-success"><AppIcon name="download" /></div>
          <div class="header-text">
            <span class="kicker">{{ t('firmware.availableKicker') }}</span>
            <h2>{{ t('firmware.availableHeading') }}</h2>
            <p>{{ t('firmware.availableHelp') }}</p>
          </div>
        </div>
        <div class="card-body">
          <BAlert v-if="updateStore.checkError" variant="warning" :model-value="true">
            {{ t('firmware.checkFailed') }}: {{ updateStore.checkError }}
          </BAlert>

          <!-- Manual "search for updates now" (Korrekturauftrag §6.2/§6.3).
               The on-device fetch runs off the httpd thread; the store polls
               fetchInProgress until it clears, so the spinner reflects the real
               check. The automatic 24 h cycle is untouched (§6.4). -->
          <div class="check-now-row">
            <BButton variant="outline-primary" class="action-btn check-now-btn"
                     :disabled="updateStore.isChecking" @click="onCheckNow">
              <span v-if="updateStore.isChecking" class="spinner-border spinner-border-sm me-2"></span>
              <AppIcon v-else name="refresh" />
              {{ updateStore.isChecking ? t('updates.checkingNow') : t('updates.checkNow') }}
            </BButton>
          </div>

          <BAlert
            v-if="manualCheckFeedback"
            :variant="manualCheckFeedback.variant"
            :model-value="true"
            class="manual-check-feedback"
            data-testid="manual-check-feedback"
          >
            <strong>{{ manualCheckFeedback.title }}</strong>
            <span>{{ manualCheckFeedback.message }}</span>
          </BAlert>

          <div v-if="updateStore.updateAvailable" class="release-box">
            <div>
              <span class="release-label">{{ t('firmware.newVersionLabel') }}</span>
              <strong>v{{ updateStore.latestVersion }}</strong>
              <small v-if="updateStore.publishedAt">{{ t('firmware.publishedAt', { time: formatDate(updateStore.publishedAt) }) }}</small>
            </div>
            <span v-if="updateStore.isPrerelease" class="beta-badge">{{ t('firmware.beta') }}</span>
          </div>
          <div v-else class="release-box is-current">
            <div>
              <span class="release-label">{{ t('firmware.statusLabel') }}</span>
              <strong>{{ updateStore.hasCompletedManualCheck || (updateStore.latestVersion && updateStore.latestVersion !== 'n/a') ? t('firmware.upToDate') : t('firmware.noCheckResult') }}</strong>
            </div>
          </div>

          <div class="actions">
            <a
              v-if="updateStore.updateAvailable && updateStore.downloadUrl"
              class="btn btn-success action-btn"
              :href="updateStore.downloadUrl"
              target="_blank"
              rel="noopener noreferrer"
            >
              <AppIcon name="download" /> {{ t('firmware.downloadBin') }}
            </a>
            <a
              v-if="updateStore.releaseUrl"
              class="btn btn-outline-secondary action-btn"
              :href="updateStore.releaseUrl"
              target="_blank"
              rel="noopener noreferrer"
            >
              <AppIcon name="externalLink" /> {{ t('firmware.viewOnGithub') }}
            </a>
          </div>

          <div class="beta-toggle-row">
            <div class="form-check form-switch">
              <input
                id="betaChannelSwitch"
                class="form-check-input"
                type="checkbox"
                role="switch"
                :checked="updateStore.betaChannel"
                :disabled="betaToggleSaving"
                @change="onBetaToggle"
              >
            </div>
            <label for="betaChannelSwitch" class="beta-toggle-label">
              {{ t('firmware.betaChannel') }}
              <span class="beta-toggle-hint">{{ t('firmware.betaCycleHint') }}</span>
            </label>
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

    <section class="system-actions">
      <button type="button" class="action-tile warning" @click="restartClick">
        <div class="tile-icon"><AppIcon name="refresh" /></div>
        <div class="tile-text"><h4>{{ t('firmware.restart') }}</h4><p>{{ t('firmware.restartSyncHint') }}</p></div>
      </button>
      <button type="button" class="action-tile danger" @click="openFactoryResetModal">
        <div class="tile-icon"><AppIcon name="restore" /></div>
        <div class="tile-text"><h4>{{ t('firmware.factoryResetTitle') }}</h4><p>{{ t('firmware.factoryResetActionHint') }}</p></div>
      </button>
    </section>

    <!-- Werkseinstellungen ist eine Gefahrenaktion (Korrekturauftrag §10):
         eigener Bestätigungsdialog mit Danger-Button statt window.confirm. The
         @ok handler calls event.preventDefault() so the modal stays open while
         confirmFactoryReset runs; the modal is closed explicitly on success. -->
    <BModal
      v-model="showFactoryResetModal"
      :title="t('firmware.factoryResetTitle')"
      centered
      no-close-on-backdrop
      :ok-title="t('firmware.factoryResetConfirm')"
      ok-variant="danger"
      :cancel-title="t('common.cancel')"
      @ok="onFactoryResetOk"
      :ok-disabled="isResetting"
      :cancel-disabled="isResetting"
    >
      <p>{{ t('firmware.factoryResetMessage') }}</p>
      <BAlert variant="danger" :model-value="true" fade class="mt-2 mb-0">
        {{ t('firmware.factoryResetWarning') }}
      </BAlert>
    </BModal>
  </div>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import axios from 'axios'
import { useFirmwareUpdateStore, useRestartUiStore, useSysInfoStore, useUiStore, useUpdateStore } from './stores.js'

const WEBUI_IMAGE_SIZE = 0x50000
const ESP_IMAGE_MAGIC = 0xe9

const { t, locale } = useI18n()
const firmwareUpdateStore = useFirmwareUpdateStore()
const restartUiStore = useRestartUiStore()
const sysInfoStore = useSysInfoStore()
const uiStore = useUiStore()
const updateStore = useUpdateStore()

const file = ref(null)
const fileInput = ref(null)
const fileError = ref('')
const isDragging = ref(false)
const uploading = ref(false)
const uploadProgress = ref(0)
const betaToggleSaving = ref(false)
const showFactoryResetModal = ref(false)
const isResetting = ref(false)

const lastCheckText = computed(() => {
  const checkedAt = updateStore.lastSuccessfulManualCheckAt || updateStore.lastCheck
  if (!checkedAt) return t('firmware.noRecentCheck')
  return t('firmware.lastCheckAt', { time: new Date(checkedAt).toLocaleString(locale.value) })
})

const manualCheckFeedback = computed(() => {
  switch (updateStore.lastManualCheckOutcome) {
    case 'updated':
      return {
        variant: 'success',
        title: t('updates.checkResultUpdatedTitle'),
        message: t('updates.checkResultUpdated', { version: updateStore.latestVersion })
      }
    case 'no-update':
      return {
        variant: 'success',
        title: t('updates.checkResultNoUpdateTitle'),
        message: t('updates.checkResultNoUpdate')
      }
    case 'cooldown':
      return {
        variant: 'info',
        title: t('updates.checkResultCooldownTitle'),
        message: t('updates.checkResultCooldown')
      }
    case 'skipped':
      return {
        variant: 'warning',
        title: t('updates.checkResultSkippedTitle'),
        message: updateStore.lastSkipReason || t('updates.checkResultSkipped')
      }
    case 'error':
      return {
        variant: 'danger',
        title: t('updates.checkResultErrorTitle'),
        message: updateStore.checkError || t('updates.checkResultError')
      }
    default:
      return null
  }
})

const formatDate = value => value ? new Date(value).toLocaleString(locale.value) : '—'
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
    restartUiStore.start({ includeFlashPause: true, syncSeconds: 40, restartSeconds: 30 })
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

const onBetaToggle = async event => {
  const enabled = event.target.checked
  betaToggleSaving.value = true
  try {
    await axios.post('/settings.json', { betaChannel: enabled }, { silent: true })
    updateStore.betaChannel = enabled
    uiStore.pushToast({ type: 'success', title: t('firmware.betaSavedTitle'), message: t('firmware.betaSavedMessage') })
  } catch (error) {
    event.target.checked = !enabled
    uiStore.pushToast({ type: 'error', title: t('firmware.saveFailedTitle'), message: error.response?.data?.error || error.message })
  } finally {
    betaToggleSaving.value = false
  }
}

// Manual update search (Korrekturauftrag §6.2/§6.3). The store handles the
// POST + polling and returns one of: 'updated' | 'no-update' | 'cooldown' |
// 'skipped' | 'error'. Each outcome surfaces a clear toast so the user
// always knows the result of clicking "search now".
const onCheckNow = async () => {
  if (updateStore.isChecking) return
  const outcome = await updateStore.checkNow(sysInfoStore.currentVersion)
  if (outcome === 'updated') {
    uiStore.pushToast({
      type: 'success',
      title: t('updates.checkResultUpdatedTitle'),
      message: t('updates.checkResultUpdated', { version: updateStore.latestVersion }),
      duration: 5000
    })
  } else if (outcome === 'no-update') {
    uiStore.pushToast({
      type: 'info',
      title: t('updates.checkResultNoUpdateTitle'),
      message: t('updates.checkResultNoUpdate'),
      duration: 4000
    })
  } else if (outcome === 'skipped') {
    // Device accepted the request but skipped the fetch (typically low heap
    // while the radio module is actively serving a CCU session). Show the
    // device's own reason rather than a misleading "no update available".
    uiStore.pushToast({
      type: 'warning',
      title: t('updates.checkResultSkippedTitle'),
      message: updateStore.lastSkipReason || t('updates.checkResultSkipped'),
      duration: 8000
    })
  } else if (outcome === 'cooldown') {
    uiStore.pushToast({
      type: 'info',
      title: t('updates.checkResultCooldownTitle'),
      message: t('updates.checkResultCooldown'),
      duration: 4000
    })
  } else {
    uiStore.pushToast({
      type: 'error',
      title: t('updates.checkResultErrorTitle'),
      message: updateStore.checkError || t('updates.checkResultError'),
      duration: 6000
    })
  }
}

const restartClick = async () => {
  if (!window.confirm(t('firmware.restartConfirm'))) return
  try { await axios.post('/api/restart') } catch { /* Verbindung bricht beim Neustart erwartbar ab. */ }
  restartUiStore.start({ includeFlashPause: true, syncSeconds: 40, restartSeconds: 30 })
}

const openFactoryResetModal = () => {
  showFactoryResetModal.value = true
}

// The BModal @ok handler receives the synthetic event from BModal.triggerOk;
// calling event.preventDefault() keeps the modal open while the request runs.
// On success we close it explicitly and start the restart-sync countdown.
const onFactoryResetOk = (event) => {
  event.preventDefault()
  confirmFactoryReset()
}

const confirmFactoryReset = async () => {
  if (isResetting.value) return
  isResetting.value = true
  try {
    await axios.post('/api/factory-reset')
    showFactoryResetModal.value = false
    restartUiStore.start({ includeFlashPause: true, syncSeconds: 40, restartSeconds: 30 })
  } catch (error) {
    uiStore.pushToast({
      type: 'error',
      title: t('common.error'),
      message: error.response?.data?.error || error.message || t('firmware.factoryResetError')
    })
    isResetting.value = false
  }
}

onMounted(async () => {
  try { await sysInfoStore.update() } catch { /* Anzeige bleibt mit Platzhalter nutzbar. */ }
  await updateStore.checkForUpdate(sysInfoStore.currentVersion)
})
</script>

<style scoped>
.content-grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:var(--card-padding); }
.update-card { background:var(--color-surface); border:1px solid var(--color-border); border-radius:var(--radius-lg); overflow:hidden; }
.card-header { display:flex; gap:14px; align-items:flex-start; padding:var(--card-padding); border-bottom:1px solid var(--color-border); }
.header-icon { width:44px; height:44px; flex:0 0 auto; border-radius:var(--radius-md); display:flex; align-items:center; justify-content:center; }
.header-text { min-width:0; flex:1; }
.header-text h2 { margin:2px 0 0; font-size: var(--fs-lg); font-weight: var(--font-weight-semibold); }
.header-text p { margin:.35rem 0 0; color:var(--color-text-secondary); }
.kicker { color:var(--color-primary-strong); font-size:var(--fs-2xs); font-weight:var(--font-weight-heavy); text-transform:uppercase; letter-spacing:.04em; }
.card-body { padding:var(--card-padding); display:flex; flex-direction:column; gap:var(--space-4); }
.release-box { display:flex; justify-content:space-between; gap:14px; align-items:center; padding:var(--space-4); border-radius:var(--radius-md); background:var(--color-success-soft); }
.release-box.is-current { background:var(--color-bg-alt); }
.release-box div { display:flex; flex-direction:column; gap:3px; }
.release-label, .release-box small { color:var(--color-text-secondary); font-size: var(--fs-xs); }
.release-box strong { font-size: var(--fs-lg); }
.beta-badge { padding:var(--space-1) var(--space-2); border-radius:var(--radius-pill); background:var(--color-warning-soft); font-size: var(--fs-2xs); font-weight: var(--font-weight-bold); }
.actions { display:flex; flex-wrap:wrap; gap:10px; }
.action-btn { display:inline-flex; align-items:center; justify-content:center; gap:var(--space-2); text-decoration:none; }
/* Manual "search now" sits above the release status so the primary action is
   reachable before the user reads the cached result. Reuses .action-btn so it
   shares size/alignment with the download + GitHub buttons below. */
.check-now-row { display:flex; }
.check-now-btn { width:auto; }
.manual-check-feedback { margin:0; display:flex; flex-direction:column; gap:var(--space-1); }
.beta-toggle-row { display:flex; gap:12px; align-items:flex-start; padding-top:14px; border-top:1px solid var(--color-border); }
.beta-toggle-label { display:flex; flex-direction:column; gap:3px; font-weight: var(--font-weight-bold); }
.beta-toggle-hint { font-size: var(--fs-xs); font-weight: var(--font-weight-normal); color:var(--color-text-secondary); }
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
.system-actions { margin-top:18px; display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:14px; }
.action-tile { width:100%; display:flex; gap:var(--space-3); align-items:center; text-align:left; padding:var(--space-4); border:1px solid var(--color-border); border-radius:var(--radius-lg); background:var(--color-surface); color:var(--color-text); cursor: pointer; }
.action-tile h4 { font-weight: var(--font-weight-semibold); }
.action-tile h4,.action-tile p { margin:0; }
.action-tile p { color:var(--color-text-secondary); font-size: var(--fs-xs); }
.action-tile.warning { border-color: var(--color-warning); background: var(--color-warning-soft); }
.action-tile.danger { border-color:var(--color-danger); background: var(--color-danger-soft); }
.action-tile.danger h4 { color: var(--color-danger); }
@media(max-width:900px){ .content-grid,.system-actions { grid-template-columns:1fr; } }
</style>
