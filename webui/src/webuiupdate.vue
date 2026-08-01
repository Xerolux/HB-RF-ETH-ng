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
            <h2>{{ t('firmware.updatesOnGithubHeading') }}</h2>
            <p>{{ t('firmware.updatesOnGithubHelp') }}</p>
          </div>
        </div>

        <div class="card-body">
          <p class="muted-text">{{ t('firmware.noAutoCheckNote') }}</p>
          <div class="actions">
            <a
              class="btn btn-success action-btn"
              href="https://github.com/Xerolux/HB-RF-ETH-ng/releases"
              target="_blank"
              rel="noopener noreferrer"
            >
              <AppIcon name="externalLink" /> {{ t('webuiUpdate.releaseLink') }}
            </a>
            <BButton variant="outline-secondary" :disabled="loading" @click="refreshCachedStatus">
              <AppIcon name="refresh" /> {{ t('webuiUpdate.reloadStatusButton') }}
            </BButton>
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
  </div>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import axios from 'axios'
import { useI18n } from 'vue-i18n'
import { useUiStore } from './stores.js'

const { t } = useI18n()

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

onMounted(refreshCachedStatus)
</script>

<style scoped>
.status-grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:var(--space-3); margin-bottom:var(--card-padding); }
.content-grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:var(--card-padding); }
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
@media(max-width:900px){ .status-grid,.content-grid { grid-template-columns:1fr; } }
</style>
