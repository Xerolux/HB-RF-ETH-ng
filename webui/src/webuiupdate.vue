<template>
  <div class="www-page page-shell">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow"><AppIcon name="firmware" /> WebUI</span>
        <h1 class="hero-title">{{ copy.title }}</h1>
        <p class="hero-subtitle">{{ copy.subtitle }}</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="check" /> New Design</span>
        <span class="meta-chip"><AppIcon name="firmware" /> {{ currentVersion }}</span>
        <span class="meta-chip" :class="status.valid ? 'success-chip' : 'warning-chip'">
          <AppIcon :name="status.valid ? 'check' : 'alert'" />
          {{ status.valid ? copy.externalActive : copy.embeddedFallback }}
        </span>
      </div>
    </section>

    <div v-if="loading" class="loading-card">
      <span class="spinner-border spinner-border-sm"></span>
      {{ copy.loading }}
    </div>

    <template v-else>
      <section class="status-grid">
        <article class="status-card">
          <span class="status-label">{{ copy.activeSource }}</span>
          <strong>{{ status.valid ? copy.externalPartition : copy.embeddedFirmware }}</strong>
          <small>{{ status.valid ? copy.externalHint : copy.fallbackHint }}</small>
        </article>
        <article class="status-card">
          <span class="status-label">{{ copy.installedVersion }}</span>
          <strong>{{ currentVersion }}</strong>
          <small>design: newdesign</small>
        </article>
        <article class="status-card">
          <span class="status-label">{{ copy.storage }}</span>
          <strong>{{ formatBytes(status.usedBytes) }} / {{ formatBytes(status.partitionSize) }}</strong>
          <div class="usage-track">
            <span :style="{ width: storagePercent + '%' }"></span>
          </div>
        </article>
      </section>

      <BAlert v-if="status.lastError && !status.valid" variant="info" :model-value="true">
        {{ status.lastError }}
      </BAlert>

      <section class="update-card primary-card">
        <div class="card-heading">
          <div>
            <span class="section-kicker">{{ copy.recommended }}</span>
            <h2>{{ copy.onlineTitle }}</h2>
            <p>{{ copy.onlineHint }}</p>
          </div>
          <span v-if="releaseWebui.version" class="version-badge">v{{ releaseWebui.version }}</span>
        </div>

        <BAlert v-if="manifestError" variant="warning" :model-value="true">
          {{ manifestError }}
        </BAlert>

        <div class="release-row" v-if="releaseWebui.version">
          <div>
            <span>{{ copy.availableVersion }}</span>
            <strong>{{ releaseWebui.version }}</strong>
          </div>
          <div>
            <span>SHA-256</span>
            <code>{{ shortHash(releaseWebui.sha256) }}</code>
          </div>
          <div>
            <span>{{ copy.imageSize }}</span>
            <strong>{{ formatBytes(releaseWebui.size) }}</strong>
          </div>
        </div>

        <div v-if="busy" class="progress-panel">
          <div class="progress-copy">
            <span>{{ progressLabel }}</span>
            <strong>{{ progress }}%</strong>
          </div>
          <div class="progress-track"><span :style="{ width: progress + '%' }"></span></div>
        </div>

        <div class="action-row">
          <BButton variant="primary" :disabled="busy || !onlineReady" @click="installOnline">
            <span v-if="busy" class="spinner-border spinner-border-sm me-2"></span>
            <AppIcon v-else name="download" />
            {{ copy.installOnline }}
          </BButton>
          <BButton variant="outline-secondary" :disabled="busy" @click="refreshAll">
            <AppIcon name="refresh" /> {{ copy.refresh }}
          </BButton>
        </div>
      </section>

      <section class="update-card">
        <div class="card-heading">
          <div>
            <span class="section-kicker">{{ copy.expert }}</span>
            <h2>{{ copy.manualTitle }}</h2>
            <p>{{ copy.manualHint }}</p>
          </div>
        </div>

        <label class="file-drop" :class="{ disabled: busy }">
          <input type="file" accept=".bin,application/octet-stream" :disabled="busy" @change="selectFile" />
          <AppIcon name="firmware" />
          <span>{{ selectedFile ? selectedFile.name : copy.chooseImage }}</span>
          <small v-if="selectedFile">{{ formatBytes(selectedFile.size) }}</small>
        </label>

        <BAlert v-if="fileError" variant="danger" :model-value="true">
          {{ fileError }}
        </BAlert>

        <div class="action-row">
          <BButton variant="outline-primary" :disabled="busy || !selectedFile || !!fileError" @click="installManual">
            <AppIcon name="upload" /> {{ copy.uploadImage }}
          </BButton>
        </div>
      </section>

      <section class="safety-card">
        <AppIcon name="shield" />
        <div>
          <strong>{{ copy.safetyTitle }}</strong>
          <p>{{ copy.safetyText }}</p>
        </div>
      </section>
    </template>
  </div>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import axios from 'axios'
import { useUiStore } from './stores.js'

const { locale } = useI18n()
const uiStore = useUiStore()

const translations = {
  de: {
    title: 'Weboberfläche aktualisieren',
    subtitle: 'Das New Design wird unabhängig von der Firmware aktualisiert.',
    loading: 'WebUI-Status wird geladen …',
    externalActive: 'Extern aktiv',
    embeddedFallback: 'Fallback aktiv',
    activeSource: 'Aktive Oberfläche',
    externalPartition: 'Separate WWW-Partition',
    embeddedFirmware: 'Eingebettetes New Design',
    externalHint: 'Firmware und WebUI können getrennt aktualisiert werden.',
    fallbackHint: 'Sicherer Übergangs- und Wiederherstellungsmodus.',
    installedVersion: 'Installierte WebUI-Version',
    storage: 'WWW-Speicher',
    recommended: 'Empfohlen',
    onlineTitle: 'Online-Update',
    onlineHint: 'Release laden, SHA-256 prüfen und direkt auf das Gerät übertragen.',
    availableVersion: 'Verfügbare Version',
    imageSize: 'Image-Größe',
    installOnline: 'WebUI online aktualisieren',
    refresh: 'Neu prüfen',
    expert: 'Expertenmodus',
    manualTitle: 'WWW-Image manuell hochladen',
    manualHint: 'Nur ein für HB-RF-ETH-ng gebautes spiffs.bin verwenden.',
    chooseImage: 'spiffs.bin auswählen',
    uploadImage: 'Image hochladen',
    safetyTitle: 'Sicherer Fallback',
    safetyText: 'Bei einem fehlerhaften oder unvollständigen WWW-Update bleibt das eingebettete New Design erreichbar. Einstellungen in NVS werden nicht verändert.',
    downloading: 'WWW-Image wird geladen',
    verifying: 'SHA-256 wird geprüft',
    uploading: 'WWW-Image wird übertragen',
    completed: 'New Design wurde aktualisiert',
    noManifest: 'Dieses Release enthält noch kein separates New-Design-Image.',
    wrongDesign: 'Das Release-Manifest ist nicht für das New Design bestimmt.',
    sizeMismatch: 'Die Image-Größe passt nicht zur WWW-Partition.',
    hashMismatch: 'SHA-256-Prüfung fehlgeschlagen.',
    invalidFile: 'Die Datei muss exakt zur WWW-Partition passen.',
    updateFailed: 'WebUI-Update fehlgeschlagen.'
  },
  en: {
    title: 'Update Web Interface',
    subtitle: 'The New Design is updated independently from the firmware.',
    loading: 'Loading WebUI status …',
    externalActive: 'External active',
    embeddedFallback: 'Fallback active',
    activeSource: 'Active interface',
    externalPartition: 'Separate WWW partition',
    embeddedFirmware: 'Embedded New Design',
    externalHint: 'Firmware and WebUI can be updated independently.',
    fallbackHint: 'Safe transition and recovery mode.',
    installedVersion: 'Installed WebUI version',
    storage: 'WWW storage',
    recommended: 'Recommended',
    onlineTitle: 'Online update',
    onlineHint: 'Download the release, verify SHA-256 and transfer it to the device.',
    availableVersion: 'Available version',
    imageSize: 'Image size',
    installOnline: 'Update WebUI online',
    refresh: 'Check again',
    expert: 'Expert mode',
    manualTitle: 'Upload WWW image manually',
    manualHint: 'Only use a spiffs.bin built for HB-RF-ETH-ng.',
    chooseImage: 'Select spiffs.bin',
    uploadImage: 'Upload image',
    safetyTitle: 'Safe fallback',
    safetyText: 'If a WWW update is corrupt or incomplete, the embedded New Design remains available. NVS settings are not modified.',
    downloading: 'Downloading WWW image',
    verifying: 'Verifying SHA-256',
    uploading: 'Uploading WWW image',
    completed: 'New Design updated',
    noManifest: 'This release does not include a separate New Design image yet.',
    wrongDesign: 'The release manifest is not intended for the New Design.',
    sizeMismatch: 'The image size does not match the WWW partition.',
    hashMismatch: 'SHA-256 verification failed.',
    invalidFile: 'The file must exactly match the WWW partition.',
    updateFailed: 'WebUI update failed.'
  }
}

const copy = computed(() => translations[locale.value] || translations.en)
const loading = ref(true)
const busy = ref(false)
const progress = ref(0)
const progressPhase = ref('')
const manifestError = ref('')
const fileError = ref('')
const selectedFile = ref(null)
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
  design: 'newdesign',
  lastError: ''
})
const releaseWebui = ref({})

const currentVersion = computed(() => status.value.version || 'embedded')
const storagePercent = computed(() => {
  const total = status.value.totalBytes || status.value.partitionSize || 0
  if (!total) return 0
  return Math.min(100, Math.round((status.value.usedBytes || 0) * 100 / total))
})
const onlineReady = computed(() => {
  const item = releaseWebui.value
  return item && item.design === 'newdesign' && item.downloadUrl &&
    /^[0-9a-f]{64}$/i.test(item.sha256 || '') &&
    Number(item.size) === Number(status.value.partitionSize) &&
    status.value.partitionSize > 0
})
const progressLabel = computed(() => {
  if (progressPhase.value === 'download') return copy.value.downloading
  if (progressPhase.value === 'verify') return copy.value.verifying
  if (progressPhase.value === 'upload') return copy.value.uploading
  return copy.value.completed
})

const formatBytes = (bytes) => {
  const value = Number(bytes) || 0
  if (value < 1024) return `${value} B`
  if (value < 1024 * 1024) return `${(value / 1024).toFixed(1)} KB`
  return `${(value / 1024 / 1024).toFixed(2)} MB`
}
const shortHash = (hash) => hash ? `${hash.slice(0, 12)}…${hash.slice(-8)}` : '—'

const sha256Hex = async (buffer) => {
  const digest = await crypto.subtle.digest('SHA-256', buffer)
  return Array.from(new Uint8Array(digest))
    .map(value => value.toString(16).padStart(2, '0'))
    .join('')
}

const loadStatus = async () => {
  const response = await axios.get('/api/webui/status', { timeout: 8000, silent: true })
  status.value = { ...status.value, ...response.data }
}

const loadReleaseManifest = async () => {
  manifestError.value = ''
  releaseWebui.value = {}
  try {
    const releaseInfo = await axios.get('/api/check_update', { timeout: 15000, silent: true })
    const channel = releaseInfo.data?.betaChannel ? 'beta' : 'latest'
    const url = `https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/main/${channel}.json?t=${Date.now()}`
    const manifest = await axios.get(url, { timeout: 15000, headers: { 'Cache-Control': 'no-cache' } })
    const item = manifest.data?.webui
    if (!item) {
      manifestError.value = copy.value.noManifest
      return
    }
    if (item.design !== 'newdesign') {
      manifestError.value = copy.value.wrongDesign
      return
    }
    releaseWebui.value = item
    if (Number(item.size) !== Number(status.value.partitionSize)) {
      manifestError.value = copy.value.sizeMismatch
    }
  } catch (error) {
    manifestError.value = error.response?.data?.message || error.message || copy.value.noManifest
  }
}

const refreshAll = async () => {
  loading.value = true
  try {
    await loadStatus()
    await loadReleaseManifest()
  } finally {
    loading.value = false
  }
}

const uploadBuffer = async (buffer, expectedHash) => {
  progressPhase.value = 'upload'
  progress.value = 50
  await axios.post('/api/webui/update', buffer, {
    timeout: 120000,
    headers: {
      'Content-Type': 'application/octet-stream',
      'X-WebUI-SHA256': expectedHash
    },
    onUploadProgress: (event) => {
      if (!event.total) return
      progress.value = 50 + Math.round((event.loaded * 50) / event.total)
    }
  })
  progress.value = 100
  progressPhase.value = 'done'
  await loadStatus()
  uiStore.pushToast({ type: 'success', title: copy.value.completed, message: `v${status.value.version || ''}` })
  window.setTimeout(() => window.location.reload(), 1000)
}

const installOnline = async () => {
  if (!onlineReady.value || busy.value) return
  busy.value = true
  progress.value = 0
  try {
    const item = releaseWebui.value
    progressPhase.value = 'download'
    const response = await axios.get(item.downloadUrl, {
      responseType: 'arraybuffer',
      timeout: 120000,
      onDownloadProgress: (event) => {
        if (!event.total) return
        progress.value = Math.min(45, Math.round((event.loaded * 45) / event.total))
      }
    })
    const buffer = response.data
    if (buffer.byteLength !== Number(status.value.partitionSize)) {
      throw new Error(copy.value.sizeMismatch)
    }
    progressPhase.value = 'verify'
    progress.value = 47
    const actualHash = await sha256Hex(buffer)
    if (actualHash.toLowerCase() !== item.sha256.toLowerCase()) {
      throw new Error(copy.value.hashMismatch)
    }
    progress.value = 50
    await uploadBuffer(buffer, actualHash)
  } catch (error) {
    uiStore.pushToast({
      type: 'error',
      title: copy.value.updateFailed,
      message: error.response?.data || error.message || copy.value.updateFailed,
      duration: 7000
    })
  } finally {
    busy.value = false
  }
}

const selectFile = (event) => {
  fileError.value = ''
  selectedFile.value = event.target.files?.[0] || null
  if (selectedFile.value && Number(selectedFile.value.size) !== Number(status.value.partitionSize)) {
    fileError.value = `${copy.value.invalidFile} (${formatBytes(status.value.partitionSize)})`
  }
}

const installManual = async () => {
  if (!selectedFile.value || fileError.value || busy.value) return
  busy.value = true
  progress.value = 0
  try {
    const buffer = await selectedFile.value.arrayBuffer()
    progressPhase.value = 'verify'
    progress.value = 25
    const hash = await sha256Hex(buffer)
    progress.value = 50
    await uploadBuffer(buffer, hash)
  } catch (error) {
    uiStore.pushToast({
      type: 'error',
      title: copy.value.updateFailed,
      message: error.response?.data || error.message || copy.value.updateFailed,
      duration: 7000
    })
  } finally {
    busy.value = false
  }
}

onMounted(refreshAll)
</script>

<style scoped>
.www-page { display: flex; flex-direction: column; gap: 20px; }
.success-chip { color: var(--color-success); }
.warning-chip { color: var(--color-warning); }
.loading-card, .update-card, .status-card, .safety-card {
  border: 1px solid var(--color-border-light);
  background: var(--color-surface);
  box-shadow: var(--shadow-sm);
}
.loading-card { padding: 24px; border-radius: var(--radius-lg); display: flex; gap: 12px; align-items: center; }
.status-grid { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 16px; }
.status-card { padding: 20px; border-radius: var(--radius-lg); display: flex; flex-direction: column; gap: 8px; }
.status-card strong { font-size: 1.05rem; }
.status-card small, .status-label { color: var(--color-text-secondary); }
.status-label { font-size: .78rem; text-transform: uppercase; letter-spacing: .08em; font-weight: 700; }
.update-card { border-radius: var(--radius-xl); padding: 24px; }
.primary-card { border-color: color-mix(in srgb, var(--color-primary) 35%, var(--color-border-light)); }
.card-heading { display: flex; justify-content: space-between; align-items: flex-start; gap: 20px; margin-bottom: 20px; }
.card-heading h2 { margin: 4px 0 6px; font-size: 1.35rem; }
.card-heading p { margin: 0; color: var(--color-text-secondary); }
.section-kicker { color: var(--color-primary); text-transform: uppercase; letter-spacing: .08em; font-size: .75rem; font-weight: 800; }
.version-badge { border-radius: var(--radius-full); background: var(--color-primary-soft); color: var(--color-primary); padding: 8px 12px; font-weight: 700; white-space: nowrap; }
.release-row { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 12px; margin-bottom: 20px; }
.release-row > div { border-radius: var(--radius-md); background: var(--color-background); padding: 14px; display: flex; flex-direction: column; gap: 5px; min-width: 0; }
.release-row span { font-size: .78rem; color: var(--color-text-secondary); }
.release-row code { overflow: hidden; text-overflow: ellipsis; }
.action-row { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 18px; }
.progress-panel { margin-top: 18px; }
.progress-copy { display: flex; justify-content: space-between; margin-bottom: 8px; }
.progress-track, .usage-track { height: 8px; border-radius: var(--radius-full); background: var(--color-border-light); overflow: hidden; }
.progress-track span, .usage-track span { display: block; height: 100%; background: var(--color-primary); transition: width .2s ease; }
.usage-track { height: 5px; margin-top: 4px; }
.file-drop { min-height: 120px; border: 1.5px dashed var(--color-border); border-radius: var(--radius-lg); display: flex; flex-direction: column; justify-content: center; align-items: center; gap: 7px; cursor: pointer; text-align: center; padding: 20px; }
.file-drop:hover { border-color: var(--color-primary); background: var(--color-primary-soft); }
.file-drop.disabled { opacity: .6; cursor: not-allowed; }
.file-drop input { display: none; }
.file-drop small { color: var(--color-text-secondary); }
.safety-card { border-radius: var(--radius-lg); padding: 18px 20px; display: flex; gap: 14px; align-items: flex-start; }
.safety-card p { margin: 4px 0 0; color: var(--color-text-secondary); }
@media (max-width: 800px) {
  .status-grid, .release-row { grid-template-columns: 1fr; }
  .card-heading { flex-direction: column; }
}
</style>
