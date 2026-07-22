<template>
  <div class="www-page page-shell">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow"><AppIcon name="firmware" /> {{ t('webuiUpdate.webuiChip') }}</span>
        <h1 class="hero-title">{{ t('webuiUpdate.pageTitle') }}</h1>
        <p class="hero-subtitle">{{ t('webuiUpdate.pageSubtitle') }}</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="firmware" /> {{ t('webuiUpdate.webuiChip') }}: {{ installedVersion }}</span>
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
      <article class="panel status-card">
        <span class="label">{{ t('webuiUpdate.checkLabel') }}</span>
        <strong>{{ lastCheckText }}</strong>
        <small>{{ t('webuiUpdate.checkHint') }}</small>
      </article>
    </section>

    <div class="content-grid">
      <section class="panel update-card">
        <div class="card-heading">
          <div>
            <span class="kicker">{{ t('webuiUpdate.availableKicker') }}</span>
            <h2>{{ t('webuiUpdate.downloadHeading') }}</h2>
            <p>{{ t('webuiUpdate.downloadHelp') }}</p>
          </div>
          <span v-if="release.version" class="version-badge">v{{ release.version }}</span>
        </div>

        <BAlert v-if="manifestError" variant="warning" :model-value="true">{{ manifestError }}</BAlert>

        <!-- Manual "search for updates now" (Korrekturauftrag §6.2/§6.3). Shares
             the on-device fetch with the Firmware tab; after it settles we reload
             the WebUI-specific release block. -->
        <div class="check-now-row">
          <BButton variant="outline-primary" class="action-btn check-now-btn"
                   :disabled="checkNowBusy" @click="onCheckNow">
            <span v-if="checkNowBusy" class="spinner-border spinner-border-sm me-2"></span>
            <AppIcon v-else name="refresh" />
            {{ checkNowBusy ? checkNowLabel : checkNowIdleLabel }}
          </BButton>
        </div>

        <div v-if="release.version" class="release-grid">
          <div><span>{{ t('webuiUpdate.installedGridLabel') }}</span><strong>{{ installedVersion }}</strong></div>
          <div><span>{{ t('webuiUpdate.availableGridLabel') }}</span><strong>{{ release.version }}</strong></div>
          <div><span>{{ t('webuiUpdate.imageSizeLabel') }}</span><strong>{{ formatBytes(release.size) }}</strong></div>
          <div><span>{{ t('webuiUpdate.minFirmwareLabel') }}</span><strong>{{ release.minFirmwareVersion || '—' }}</strong></div>
        </div>

        <BAlert v-if="release.version && !updateAvailable" variant="success" :model-value="true">
          {{ t('webuiUpdate.upToDate') }}
        </BAlert>

        <div class="actions">
          <a
            v-if="updateAvailable && release.downloadUrl"
            class="btn btn-success action-btn"
            :href="release.downloadUrl"
            target="_blank"
            rel="noopener noreferrer"
          >
            <AppIcon name="download" /> {{ t('webuiUpdate.downloadButton') }}
          </a>
          <a
            v-if="release.releaseUrl"
            class="btn btn-outline-secondary action-btn"
            :href="release.releaseUrl"
            target="_blank"
            rel="noopener noreferrer"
          >
            <AppIcon name="externalLink" /> {{ t('webuiUpdate.releaseLink') }}
          </a>
          <BButton variant="outline-secondary" :disabled="loading" @click="refreshCachedStatus">
            <AppIcon name="refresh" /> {{ t('webuiUpdate.reloadStatusButton') }}
          </BButton>
        </div>
      </section>

      <section class="panel update-card">
        <div class="card-heading">
          <div>
            <span class="kicker">{{ t('webuiUpdate.manualKicker') }}</span>
            <h2>{{ t('webuiUpdate.uploadHeading') }}</h2>
            <p>{{ t('webuiUpdate.uploadHelp', { size: formatBytes(status.partitionSize) }) }}</p>
          </div>
        </div>

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
      </section>
    </div>
  </div>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import axios from 'axios'
import { useI18n } from 'vue-i18n'
import { useUiStore, useUpdateStore } from './stores.js'

const { t } = useI18n()

const WEBUI_API_VERSION = 1
const EMBEDDED_WEBUI_VERSION = typeof __WEBUI_VERSION__ !== 'undefined' ? __WEBUI_VERSION__ : ''

const uiStore = useUiStore()
const updateStore = useUpdateStore()
const loading = ref(true)
const busy = ref(false)
const progress = ref(0)
const manifestError = ref('')
const statusError = ref('')
const fileError = ref('')
const selectedFile = ref(null)
const release = ref({})
const firmwareVersion = ref('')
const updateFetchedAt = ref(null)
// Manual update search (Korrekturauftrag §6.2/§6.3). Labels reuse the shared
// updates.* keys so this tab stays in sync with the Firmware tab.
const checkNowBusy = ref(false)
const checkNowIdleLabel = computed(() => t('updates.checkNow'))
const checkNowLabel = computed(() => t('updates.checkingNow'))
const status = ref({
  partitionFound: false,
  mounted: false,
  valid: false,
  updateActive: false,
  partitionSize: 0,
  totalBytes: 0,
  usedBytes: 0,
  bytesWritten: 0,
  version: '',
  effectiveVersion: '',
  design: 'newdesign',
  lastError: ''
})

const installedVersion = computed(() => status.value.effectiveVersion || status.value.version || EMBEDDED_WEBUI_VERSION || t('webuiUpdate.unknownVersion'))
const storagePercent = computed(() => {
  const total = status.value.totalBytes || status.value.partitionSize || 0
  return total ? Math.min(100, Math.round((status.value.usedBytes || 0) * 100 / total)) : 0
})
const updateAvailable = computed(() => !!release.value.version && compareVersions(installedVersion.value, release.value.version) < 0)
const lastCheckText = computed(() => updateFetchedAt.value ? new Date(Number(updateFetchedAt.value)).toLocaleString() : t('webuiUpdate.neverChecked'))

const formatBytes = bytes => {
  const value = Number(bytes) || 0
  if (value < 1024) return `${value} B`
  if (value < 1024 * 1024) return `${(value / 1024).toFixed(1)} KB`
  return `${(value / 1024 / 1024).toFixed(2)} MB`
}

const compareVersions = (left = '', right = '') => {
  const parse = value => {
    const [core, pre = ''] = String(value).replace(/^v/i, '').split('-', 2)
    const numbers = core.split('.').map(part => Number(part) || 0)
    return { numbers: [numbers[0] || 0, numbers[1] || 0, numbers[2] || 0], pre }
  }
  const a = parse(left)
  const b = parse(right)
  for (let index = 0; index < 3; index += 1) {
    if (a.numbers[index] !== b.numbers[index]) return a.numbers[index] > b.numbers[index] ? 1 : -1
  }
  if (!a.pre && !b.pre) return 0
  if (!a.pre) return 1
  if (!b.pre) return -1
  return a.pre.localeCompare(b.pre, undefined, { numeric: true, sensitivity: 'base' })
}

const loadStatus = async () => {
  try {
    const [storageResponse, systemResponse] = await Promise.all([
      axios.get('/api/webui/status', { timeout: 8000, silent: true }),
      axios.get('/sysinfo.json', { timeout: 8000, silent: true })
    ])
    status.value = { ...status.value, ...storageResponse.data }
    firmwareVersion.value = systemResponse.data?.sysInfo?.currentVersion || ''
    statusError.value = ''
  } catch (err) {
    // Network drop, device mid-reboot, 500 — degrade gracefully instead of
    // rejecting through onMounted. Show a retry banner; partitionSize stays
    // at its previous value so install validation can't trip on a bogus 0.
    statusError.value = err.response?.data?.message || err.message || t('webuiUpdate.statusLoadError')
  }
}

const loadCachedRelease = async () => {
  manifestError.value = ''
  release.value = {}
  try {
    const response = await axios.get('/api/check_update', { timeout: 8000, silent: true })
    updateFetchedAt.value = response.data?.fetchedAt || null
    const item = response.data?.webui
    if (!item) {
      manifestError.value = t('webuiUpdate.manifestNoWebui')
      return
    }
    if (item.design !== 'newdesign') {
      manifestError.value = t('webuiUpdate.manifestWrongDesign')
      return
    }
    release.value = item
    if (Number(item.size) !== Number(status.value.partitionSize)) {
      manifestError.value = t('webuiUpdate.manifestWrongSize', { expected: formatBytes(status.value.partitionSize) })
    } else if (Number(item.apiVersion) !== WEBUI_API_VERSION) {
      manifestError.value = t('webuiUpdate.manifestWrongApi')
    } else if (compareVersions(firmwareVersion.value, item.minFirmwareVersion) < 0) {
      manifestError.value = t('webuiUpdate.manifestFirmwareTooOld', { min: item.minFirmwareVersion })
    }
  } catch (error) {
    manifestError.value = error.response?.data?.message || error.message || t('webuiUpdate.manifestLoadError')
  }
}

// Manual update search (Korrekturauftrag §6.2/§6.3). Delegates the POST +
// polling to the shared update store, then reloads the WebUI release block so
// the result (new version / current / error) is reflected in this tab. The
// outcome drives a definitive toast per §6.3.
const onCheckNow = async () => {
  if (checkNowBusy.value) return
  checkNowBusy.value = true
  try {
    const outcome = await updateStore.checkNow(firmwareVersion.value)
    await loadCachedRelease()
    if (outcome === 'updated' && updateAvailable.value) {
      uiStore.pushToast({ type: 'success', title: t('updates.checkResultUpdatedTitle'), message: t('updates.checkResultUpdated', { version: release.value.version }), duration: 5000 })
    } else if (outcome === 'skipped') {
      uiStore.pushToast({ type: 'warning', title: t('updates.checkResultSkippedTitle'), message: updateStore.lastSkipReason || t('updates.checkResultSkipped'), duration: 8000 })
    } else if (outcome === 'no-update' || outcome === 'updated') {
      uiStore.pushToast({ type: 'info', title: t('updates.checkResultNoUpdateTitle'), message: t('updates.checkResultNoUpdate'), duration: 4000 })
    } else if (outcome === 'cooldown') {
      uiStore.pushToast({ type: 'info', title: t('updates.checkResultCooldownTitle'), message: t('updates.checkResultCooldown'), duration: 4000 })
    } else {
      uiStore.pushToast({ type: 'error', title: t('updates.checkResultErrorTitle'), message: updateStore.checkError || t('updates.checkResultError'), duration: 6000 })
    }
  } finally {
    checkNowBusy.value = false
  }
}

const refreshCachedStatus = async () => {
  loading.value = true
  try {
    await loadStatus()
    await loadCachedRelease()
  } finally {
    loading.value = false
  }
}

const clearFile = () => {
  selectedFile.value = null
  fileError.value = ''
}

const selectFile = event => {
  fileError.value = ''
  selectedFile.value = event.target.files?.[0] || null
  if (!selectedFile.value) return

  const name = String(selectedFile.value.name || '').toLowerCase()
  if (!name.endsWith('.bin')) {
    fileError.value = t('webuiUpdate.fileInvalidExtension')
  } else if (name.startsWith('firmware_')) {
    fileError.value = t('webuiUpdate.fileIsFirmware')
  } else if (!status.value.partitionSize) {
    // Status load failed earlier — can't validate size. Tell the user instead
    // of every file failing with "expected 0 bytes".
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
    await axios.post('/api/webui/update', selectedFile.value, {
      timeout: 180000,
      headers: { 'Content-Type': 'application/octet-stream' },
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
    uiStore.pushToast({ type: 'error', title: t('webuiUpdate.uploadFailedTitle'), message, duration: 7000 })
  } finally {
    busy.value = false
    progress.value = 0
  }
}

onMounted(refreshCachedStatus)
</script>

<style scoped>
.status-grid { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:var(--space-3); margin-bottom:var(--card-padding); }
.content-grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:var(--card-padding); }
.panel { background:var(--color-surface); border:1px solid var(--color-border); border-radius:var(--radius-lg); }
.status-card { padding:var(--space-4); display:flex; flex-direction:column; gap:5px; }
.status-card .label,.status-card small { color:var(--color-text-secondary); font-size: var(--fs-xs); }
.status-card strong { font-size: var(--fs-md); overflow-wrap:anywhere; }
.update-card { padding:var(--card-padding); display:flex; flex-direction:column; gap:var(--space-4); }
.card-heading { display:flex; justify-content:space-between; gap:var(--space-3); align-items:flex-start; }
.card-heading h2 { margin:.25rem 0; font-size: var(--fs-lg); }
.card-heading p { margin:0; color:var(--color-text-secondary); }
.kicker { color:var(--color-primary-strong); font-size: var(--fs-2xs); font-weight:var(--font-weight-heavy); text-transform:uppercase; letter-spacing:.04em; }
.version-badge { padding:5px 9px; border-radius:999px; background:var(--color-primary-soft); font-weight:var(--font-weight-heavy); white-space:nowrap; }
.release-grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:var(--space-3); }
.release-grid div { padding:var(--space-3); border-radius:var(--radius-sm); background:var(--color-bg-alt); display:flex; flex-direction:column; gap:var(--space-1); }
.release-grid span { color:var(--color-text-secondary); font-size: var(--fs-xs); }
.release-grid strong { overflow-wrap:anywhere; }
.actions { display:flex; flex-wrap:wrap; gap:var(--space-3); }
.action-btn { display:inline-flex; gap:var(--space-2); align-items:center; text-decoration:none; }
/* Manual "search now" row — mirrors the Firmware tab so both sub-tabs share
   the same primary action placement (Korrekturauftrag §6.5). */
.check-now-row { display:flex; margin-bottom:var(--space-1); }
.check-now-btn { width:auto; }
.file-drop { min-height:145px; border:2px dashed var(--color-border-strong); border-radius:var(--radius-lg); display:flex; flex-direction:column; gap:7px; align-items:center; justify-content:center; text-align:center; cursor:pointer; padding:var(--card-padding); }
.file-drop input { display:none; }
.file-drop.invalid { border-color:var(--color-danger); }
.file-drop.disabled { opacity:.6; cursor:not-allowed; }
.file-drop small { color:var(--color-text-secondary); }
.track { height:10px; border-radius:999px; background:var(--color-bg-alt); overflow:hidden; }
.track span { display:block; height:100%; background:var(--color-primary); }
.progress-panel { display:flex; flex-direction:column; gap:var(--space-2); }
.progress-copy { display:flex; justify-content:space-between; gap:var(--space-3); }
@media(max-width:900px){ .status-grid,.content-grid { grid-template-columns:1fr; } }
</style>
