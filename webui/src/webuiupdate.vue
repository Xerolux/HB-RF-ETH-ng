<template>
  <div class="www-page page-shell">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow"><AppIcon name="globe" /> {{ t('webuiUpdate.webuiChip') }}</span>
        <h1 class="hero-title">{{ t('webuiUpdate.pageTitle') }}</h1>
        <p class="hero-subtitle">{{ t('webuiUpdate.pageSubtitle') }}</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="globe" /> {{ t('webuiUpdate.webuiChip') }}: {{ installedVersion }}</span>
        <span class="meta-chip"><AppIcon name="firmware" /> {{ t('webuiUpdate.firmwareChip') }}: {{ firmwareVersion || '—' }}</span>
        <span class="meta-chip" :class="status.valid ? 'success-chip' : 'warning-chip'">
          <AppIcon :name="status.valid ? 'check' : 'shield'" />
          {{ status.valid ? t('webuiUpdate.separateActive') : t('webuiUpdate.embeddedFallback') }}
        </span>
      </div>
    </section>

    <BAlert v-if="statusError" variant="danger" :model-value="true" class="status-error">
      {{ statusError }}
    </BAlert>

    <section class="status-grid">
      <article class="panel status-card">
        <span class="label">{{ t('webuiUpdate.installedLabel') }}</span>
        <strong>{{ installedVersion }}</strong>
        <small>{{ status.valid ? t('webuiUpdate.separateSource') : t('webuiUpdate.embeddedSource') }}</small>
      </article>
      <article class="panel status-card">
        <span class="label">{{ t('webuiUpdate.storageLabel') }}</span>
        <strong>{{ formatBytes(status.usedBytes) }} / {{ formatBytes(status.partitionSize) }}</strong>
        <div class="track"><span :style="{ width: storagePercent + '%' }"></span></div>
      </article>
    </section>

    <div class="content-grid">
      <section class="update-card">
        <div class="card-header">
          <div class="header-icon bg-success-light text-success"><AppIcon name="externalLink" /></div>
          <div class="header-text">
            <span class="kicker">{{ t('webuiUpdate.availableKicker') }}</span>
            <h2>{{ t('firmware.onlineHeading') }}</h2>
            <p>{{ t('webuiUpdate.onlineHelp') }}</p>
          </div>
        </div>

        <div class="card-body">
          <p class="muted-text">{{ t('firmware.onlineManualNote') }}</p>

          <div class="channel-row">
            <label class="channel-label" for="webui-update-channel">{{ t('firmware.channelLabel') }}</label>
            <select id="webui-update-channel" v-model="channel" class="form-select channel-select"
                    :disabled="checking">
              <option value="stable">{{ t('firmware.channelStable') }}</option>
              <option value="beta">{{ t('firmware.channelBeta') }}</option>
            </select>
          </div>

          <!-- Same rule as on the firmware page: the outcome is durable page
               content, and a skip is never rendered as "up to date". -->
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
          <BAlert v-if="triggerMessage.text" :variant="triggerMessage.variant" :model-value="true">
            {{ triggerMessage.text }}
          </BAlert>
          <BAlert v-if="searchMessage.text" :variant="searchMessage.variant" :model-value="true">
            {{ searchMessage.text }}
          </BAlert>
          <p v-if="!searchMessage.text" class="muted-text">{{ t('firmware.neverChecked') }}</p>

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
              {{ updateStatus.notesUrl ? t('firmware.releaseNotes') : t('webuiUpdate.releaseLink') }}
            </a>
          </div>

          <!-- Online install: the device downloads the WebUI image named by
               its own manifest, verifies SHA-256 and remounts it - no reboot,
               but the browser needs a reload to pick up the new assets. -->
          <div v-if="updateStatus.webuiUpdateAvailable || webuiInstallVisible || webuiInstallDone" class="install-panel">
            <BButton v-if="!installBusy && !webuiInstallDone" variant="primary" class="action-btn"
                     :disabled="checking || installStarting" @click="openInstallConfirm">
              <AppIcon name="download" /> {{ t('firmware.installNow') }}
            </BButton>
            <div v-if="installBusy" class="progress-panel">
              <div class="progress-copy"><span>{{ installStateText }}</span><strong>{{ installProgressPct }}%</strong></div>
              <div class="track"><span :style="{ width: installProgressPct + '%' }"></span></div>
            </div>
            <BAlert v-if="installError" variant="danger" :model-value="true">
              {{ t('firmware.installFailed') }} {{ installError }}
            </BAlert>
            <BAlert v-if="webuiInstallDone" variant="success" :model-value="true" class="install-done">
              <span>{{ t('webuiUpdate.installDone') }}</span>
              <BButton variant="outline-success" size="sm" @click="reloadPage">
                {{ t('webuiUpdate.reloadAfterInstall') }}
              </BButton>
            </BAlert>
          </div>
        </div>
      </section>

      <section class="update-card">
        <div class="card-header">
          <div class="header-icon bg-primary-light text-primary"><AppIcon name="upload" /></div>
          <div class="header-text">
            <span class="kicker">{{ t('webuiUpdate.manualKicker') }}</span>
            <h2>{{ t('webuiUpdate.uploadHeading') }}</h2>
            <p>{{ t('webuiUpdate.uploadHelp', { size: formatBytes(status.partitionSize) }) }}</p>
          </div>
        </div>

        <div class="card-body">
          <label class="file-drop" :class="{ disabled: busy, invalid: !!fileError }">
            <input type="file" accept=".bin,application/octet-stream" :disabled="busy" @change="selectFile">
            <AppIcon name="upload" />
            <span>{{ selectedFile ? selectedFile.name : t('webuiUpdate.chooseFile') }}</span>
            <small v-if="selectedFile">{{ formatBytes(selectedFile.size) }}</small>
          </label>

          <BAlert v-if="fileError" variant="danger" :model-value="true">{{ fileError }}</BAlert>

          <div v-if="busy" class="progress-panel">
            <div class="progress-copy"><span>{{ t('webuiUpdate.uploadInProgress') }}</span><strong>{{ progress }}%</strong></div>
            <div class="track"><span :style="{ width: progress + '%' }"></span></div>
          </div>

          <BAlert variant="info" :model-value="true">
            {{ t('webuiUpdate.uploadExplainer') }}
          </BAlert>

          <div class="actions">
            <BButton variant="primary" :disabled="busy || !selectedFile || !!fileError" @click="installManual">
              <span v-if="busy" class="spinner-border spinner-border-sm me-2"></span>
              <AppIcon v-else name="upload" /> {{ busy ? t('webuiUpdate.installingButton') : t('webuiUpdate.installButton') }}
            </BButton>
            <BButton v-if="selectedFile" variant="outline-secondary" :disabled="busy" @click="clearFile">
              <AppIcon name="close" /> {{ t('webuiUpdate.clearSelectionButton') }}
            </BButton>
          </div>
        </div>
      </section>
    </div>

    <!-- Changelog-before-install confirmation; notes are plain text. -->
    <Transition name="fade">
      <div v-if="showInstallConfirm" class="modal-backdrop" @click.self="closeInstallConfirm">
        <div class="modal-card" role="dialog" aria-modal="true">
          <h3>{{ t('webuiUpdate.installConfirmTitle', { version: updateStatus.latestWebui }) }}</h3>
          <p class="muted-text">{{ t('webuiUpdate.installConfirmBody') }}</p>
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
import axios from 'axios'
import { useI18n } from 'vue-i18n'
import { useUiStore } from './stores.js'
import { useUpdateSearch } from './composables/useUpdateSearch'

const { t } = useI18n()

// Shared with the firmware update page - one device-side search answers both.
const {
  channel,
  checking,
  updateStatus,
  triggerMessage,
  resultMessage,
  checkForUpdates,
  loadUpdateStatus,
  scheduleInstall,
  cancelScheduledInstall,
  stopPolling,
  formatTimestamp,
  startInstall,
  installBusy,
  fetchReleaseNotes
} = useUpdateSearch(t)

// --- Scheduled one-shot install (pending banner + window button) -------------
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

const confirmScheduleInstall = async () => {
  installStarting.value = true
  const outcome = await scheduleInstall('webui')
  installStarting.value = false
  showInstallConfirm.value = false
  if (outcome === 'scheduled') {
    uiStore.pushToast({
      type: 'success',
      title: t('firmware.pendingScheduledTitle'),
      message: t('firmware.pendingToast', { version: updateStatus.value.latestWebui }),
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

const searchMessage = resultMessage(status => {
  if (status.webuiUpdateAvailable) {
    return {
      variant: 'success',
      text: `${t('firmware.webuiAvailable', { version: status.latestWebui })} ${t('webuiUpdate.installHint')}`
    }
  }
  return { variant: 'success', text: t('webuiUpdate.upToDate', { version: status.latestWebui }) }
})

// --- Online install --------------------------------------------------------
// Same flow as the firmware page, but without a reboot: on success the device
// already serves the new image and the browser just needs a reload.
const showInstallConfirm = ref(false)
const installStarting = ref(false)
const notesLoading = ref(false)
const releaseNotes = ref('')

const webuiInstall = computed(() => (
  updateStatus.value.install.target === 'webui'
    ? updateStatus.value.install
    : { state: 'idle', progress: 0, error: '' }
))
const installProgressPct = computed(() => webuiInstall.value.progress || 0)
const installError = computed(() => webuiInstall.value.error || '')
const webuiInstallVisible = computed(() =>
  ['preparing', 'downloading', 'verifying', 'error'].includes(webuiInstall.value.state))
const webuiInstallDone = computed(() => webuiInstall.value.state === 'success')

const installStateText = computed(() => {
  switch (webuiInstall.value.state) {
    case 'preparing': return t('firmware.installPreparing')
    case 'downloading': return t('firmware.installDownloading')
    case 'verifying': return t('firmware.installVerifying')
    default: return ''
  }
})

const openInstallConfirm = async () => {
  showInstallConfirm.value = true
  releaseNotes.value = ''
  notesLoading.value = true
  releaseNotes.value = await fetchReleaseNotes(updateStatus.value.latestWebui) || ''
  notesLoading.value = false
}

const closeInstallConfirm = () => {
  if (!installStarting.value) showInstallConfirm.value = false
}

const confirmInstall = async () => {
  installStarting.value = true
  const outcome = await startInstall('webui')
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

const reloadPage = () => window.location.reload()

// Freshly mounted image: refresh the storage status so the installed version
// chip reflects what just happened.
watch(() => updateStatus.value.install.state, state => {
  if (state === 'success' && updateStatus.value.install.target === 'webui') {
    refreshCachedStatus().catch(() => {})
  }
})

onBeforeUnmount(stopPolling)

const WEBUI_API_VERSION = typeof __WEBUI_API_VERSION__ !== 'undefined' ? __WEBUI_API_VERSION__ : 1
const EMBEDDED_WEBUI_VERSION = typeof __WEBUI_VERSION__ !== 'undefined' ? __WEBUI_VERSION__ : ''

const uiStore = useUiStore()
const loading = ref(true)
const busy = ref(false)
const progress = ref(0)
const statusError = ref('')
const fileError = ref('')
const selectedFile = ref(null)
const firmwareVersion = ref('')
const status = ref({
  partitionFound: false,
  mounted: false,
  manifestValid: false,
  compatible: false,
  valid: false,
  updateActive: false,
  partitionSize: 0,
  totalBytes: 0,
  usedBytes: 0,
  bytesWritten: 0,
  apiVersion: 0,
  supportedApiVersion: WEBUI_API_VERSION,
  version: '',
  effectiveVersion: '',
  minFirmwareVersion: '',
  compatibilityStatus: 'unknown',
  design: 'newdesign',
  lastError: ''
})

const installedVersion = computed(() => status.value.effectiveVersion || status.value.version || EMBEDDED_WEBUI_VERSION || t('webuiUpdate.unknownVersion'))
const storagePercent = computed(() => {
  const total = status.value.totalBytes || status.value.partitionSize || 0
  return total ? Math.min(100, Math.round((status.value.usedBytes || 0) * 100 / total)) : 0
})

const formatBytes = bytes => {
  const value = Number(bytes) || 0
  if (value < 1024) return `${value} B`
  if (value < 1024 * 1024) return `${(value / 1024).toFixed(1)} KB`
  return `${(value / 1024 / 1024).toFixed(2)} MB`
}

const loadStatus = async () => {
  try {
    const [storageResponse, systemResponse] = await Promise.all([
      axios.get('/api/webui/status', { timeout: 8000, silent: true }),
      axios.get('/sysinfo.json', { timeout: 8000, silent: true })
    ])
    status.value = { ...status.value, ...storageResponse.data }
    window.dispatchEvent(new CustomEvent('webui-status-changed', {
      detail: status.value
    }))
    firmwareVersion.value = systemResponse.data?.sysInfo?.currentVersion || ''
    statusError.value = ''
  } catch (err) {
    statusError.value = err.response?.data?.message || err.message || t('webuiUpdate.statusLoadError')
  }
}

const refreshCachedStatus = async () => {
  loading.value = true
  try {
    await loadStatus()
  } finally {
    loading.value = false
  }
}

const clearFile = () => {
  selectedFile.value = null
  fileError.value = ''
}

const selectFile = async event => {
  fileError.value = ''
  selectedFile.value = event.target.files?.[0] || null
  if (!selectedFile.value) return

  const name = String(selectedFile.value.name || '').toLowerCase()
  if (!name.endsWith('.bin')) {
    fileError.value = t('webuiUpdate.fileInvalidExtension')
    return
  }
  if (name.startsWith('firmware_')) {
    fileError.value = t('webuiUpdate.fileIsFirmware')
    return
  }
  // Die Größenprüfung braucht die Partitionsgröße vom Gerät. Stand der
  // Seitenaufruf fehlgeschlagen, wird er hier still einmal nachgeladen,
  // statt die Dateiauswahl ohne Grund zu verweigern.
  if (!status.value.partitionSize) await refreshCachedStatus()
  if (!status.value.partitionSize) {
    fileError.value = t('webuiUpdate.fileStatusMissing')
  } else if (Number(selectedFile.value.size) !== Number(status.value.partitionSize)) {
    fileError.value = t('webuiUpdate.fileWrongSize', { expected: formatBytes(status.value.partitionSize), bytes: status.value.partitionSize })
  }
}

const installManual = async () => {
  if (!selectedFile.value || fileError.value || busy.value) return
  busy.value = true
  progress.value = 0
  try {
    const headers = { 'Content-Type': 'application/octet-stream' }

    await axios.post('/api/webui/update', selectedFile.value, {
      timeout: 180000,
      headers,
      onUploadProgress: event => {
        if (event.total) progress.value = Math.round(event.loaded * 100 / event.total)
      }
    })
    progress.value = 100
    await loadStatus()
    uiStore.pushToast({ type: 'success', title: t('webuiUpdate.installSuccessTitle'), message: t('webuiUpdate.installSuccessMessage', { version: installedVersion.value }), duration: 3500 })
    window.setTimeout(() => window.location.reload(), 1200)
  } catch (error) {
    const message = typeof error.response?.data === 'string'
      ? error.response.data
      : (error.response?.data?.message || error.message || t('webuiUpdate.uploadFailedMessage'))
    await loadStatus()
    uiStore.pushToast({ type: 'error', title: t('webuiUpdate.uploadFailedTitle'), message, duration: 7000 })
  } finally {
    busy.value = false
    progress.value = 0
  }
}

onMounted(async () => {
  await refreshCachedStatus()
  // Show whatever the device already knows, without starting a search.
  try { await loadUpdateStatus() } catch { /* Karte zeigt dann "noch keine Suche". */ }
})
</script>

<style scoped>
.status-grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:var(--space-3); margin-bottom:var(--card-padding); }
.content-grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:var(--card-padding); }
.channel-row { display:flex; align-items:center; gap:var(--space-3); margin-bottom:var(--space-3); }
.channel-label { color:var(--color-text-secondary); font-size:var(--fs-sm); font-weight:var(--font-weight-medium); margin:0; }
.channel-select { max-width:200px; }
.panel { background:var(--color-surface); border:1px solid var(--color-border); border-radius:var(--radius-lg); }
.status-card { padding:var(--space-4); display:flex; flex-direction:column; gap:5px; }
.status-card .label,.status-card small { color:var(--color-text-secondary); font-size: var(--fs-xs); }
.status-card strong { font-size: var(--fs-md); overflow-wrap:anywhere; }
.update-card { background:var(--color-surface); border:1px solid var(--color-border); border-radius:var(--radius-lg); overflow:hidden; }
.card-header { display:flex; gap:14px; align-items:flex-start; padding:var(--card-padding); border-bottom:1px solid var(--color-border); }
.header-icon { width:44px; height:44px; flex:0 0 auto; border-radius:var(--radius-md); display:flex; align-items:center; justify-content:center; }
.header-text { min-width:0; flex:1; }
.header-text h2 { margin:2px 0 0; font-size:var(--fs-lg); font-weight:var(--font-weight-semibold); }
.header-text p { margin:.35rem 0 0; color:var(--color-text-secondary); }
.card-body { padding:var(--card-padding); display:flex; flex-direction:column; gap:var(--space-4); }
.kicker { color:var(--color-primary-strong); font-size: var(--fs-2xs); font-weight:var(--font-weight-heavy); text-transform:uppercase; letter-spacing:.04em; }
.muted-text { margin:0; color:var(--color-text-secondary); }
.actions { display:flex; flex-wrap:wrap; gap:var(--space-3); }
.action-btn { display:inline-flex; gap:var(--space-2); align-items:center; text-decoration:none; }
.file-drop { min-height:150px; border:2px dashed var(--color-border-strong); border-radius:var(--radius-lg); display:flex; flex-direction:column; gap:7px; align-items:center; justify-content:center; text-align:center; cursor:pointer; padding:var(--card-padding); }
.file-drop input { display:none; }
.file-drop.invalid { border-color:var(--color-danger); }
.file-drop.disabled { opacity:.6; cursor:not-allowed; }
.file-drop small { color:var(--color-text-secondary); }
.track { height:10px; border-radius:999px; background:var(--color-bg-alt); overflow:hidden; }
.track span { display:block; height:100%; background:var(--color-primary); }
.progress-panel { display:flex; flex-direction:column; gap:var(--space-2); }
.progress-copy { display:flex; justify-content:space-between; gap:var(--space-3); }
.install-panel { display:flex; flex-direction:column; gap:var(--space-3); }
.install-done { display:flex; align-items:center; justify-content:space-between; gap:var(--space-3); flex-wrap:wrap; }
.modal-backdrop { position:fixed; inset:0; background:rgba(0,0,0,.55); display:flex; align-items:center; justify-content:center; z-index:1050; padding:var(--card-padding); }
.modal-card { background:var(--color-surface); border-radius:var(--radius-lg); max-width:640px; width:100%; max-height:85vh; overflow-y:auto; padding:calc(var(--card-padding) * 1.25); display:flex; flex-direction:column; gap:var(--space-3); }
.modal-card h3 { margin:0; font-size:var(--fs-lg); }
.modal-card h4 { margin:0 0 var(--space-2); font-size:var(--fs-md); }
.modal-actions { display:flex; justify-content:flex-end; gap:10px; }
.changelog-box { border:1px solid var(--color-border); border-radius:var(--radius-md); padding:var(--space-3); background:var(--color-bg-alt); }
.changelog-text { margin:0; white-space:pre-wrap; overflow-wrap:anywhere; font-family:var(--font-mono); font-size:var(--fs-xs); max-height:40vh; overflow-y:auto; }
@media(max-width:900px){ .status-grid,.content-grid { grid-template-columns:1fr; } }
</style>
