<template>
  <div class="www-page page-shell">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow"><AppIcon name="firmware" /> WebUI</span>
        <h1 class="hero-title">{{ text.title }}</h1>
        <p class="hero-subtitle">{{ text.subtitle }}</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="check" /> New Design</span>
        <span class="meta-chip"><AppIcon name="firmware" /> {{ currentVersion }}</span>
        <span class="meta-chip" :class="status.valid ? 'success-chip' : 'warning-chip'">
          <AppIcon :name="status.valid ? 'check' : 'alert'" />
          {{ status.valid ? text.externalActive : text.fallbackActive }}
        </span>
      </div>
    </section>

    <div v-if="loading" class="panel loading-panel">
      <span class="spinner-border spinner-border-sm"></span>
      {{ text.loading }}
    </div>

    <template v-else>
      <section class="status-grid">
        <article class="panel status-card">
          <span class="label">{{ text.source }}</span>
          <strong>{{ status.valid ? text.external : text.embedded }}</strong>
          <small>{{ status.valid ? text.externalHint : text.fallbackHint }}</small>
        </article>
        <article class="panel status-card">
          <span class="label">{{ text.version }}</span>
          <strong>{{ currentVersion }}</strong>
          <small>design: newdesign</small>
        </article>
        <article class="panel status-card">
          <span class="label">{{ text.storage }}</span>
          <strong>{{ formatBytes(status.usedBytes) }} / {{ formatBytes(status.partitionSize) }}</strong>
          <div class="track"><span :style="{ width: storagePercent + '%' }"></span></div>
        </article>
      </section>

      <BAlert v-if="status.lastError && !status.valid" variant="info" :model-value="true">
        {{ status.lastError }}
      </BAlert>

      <section class="panel update-card primary-card">
        <div class="card-heading">
          <div>
            <span class="kicker">{{ text.recommended }}</span>
            <h2>{{ text.onlineTitle }}</h2>
            <p>{{ text.onlineHint }}</p>
          </div>
          <span v-if="release.version" class="version-badge">v{{ release.version }}</span>
        </div>

        <BAlert v-if="manifestError" variant="warning" :model-value="true">
          {{ manifestError }}
        </BAlert>

        <div v-if="release.version" class="release-grid">
          <div><span>{{ text.available }}</span><strong>{{ release.version }}</strong></div>
          <div><span>SHA-256</span><code>{{ shortHash(release.sha256) }}</code></div>
          <div><span>{{ text.imageSize }}</span><strong>{{ formatBytes(release.size) }}</strong></div>
        </div>
        <p v-if="release.version" class="compatibility-note">
          API {{ release.apiVersion ?? '—' }} · Firmware ≥ {{ release.minFirmwareVersion || '—' }}
        </p>

        <div v-if="busy" class="progress-panel">
          <div class="progress-copy"><span>{{ progressLabel }}</span><strong>{{ progress }}%</strong></div>
          <div class="track"><span :style="{ width: progress + '%' }"></span></div>
        </div>

        <div class="actions">
          <BButton variant="primary" :disabled="busy || !onlineReady" @click="installOnline">
            <span v-if="busy" class="spinner-border spinner-border-sm me-2"></span>
            <AppIcon v-else name="download" /> {{ text.installOnline }}
          </BButton>
          <BButton variant="outline-secondary" :disabled="busy" @click="refreshAll">
            <AppIcon name="refresh" /> {{ text.refresh }}
          </BButton>
        </div>
      </section>

      <section class="panel update-card">
        <div class="card-heading">
          <div>
            <span class="kicker">{{ text.expert }}</span>
            <h2>{{ text.manualTitle }}</h2>
            <p>{{ text.manualHint }}</p>
          </div>
        </div>

        <label class="file-drop" :class="{ disabled: busy }">
          <input type="file" accept=".bin,application/octet-stream" :disabled="busy" @change="selectFile" />
          <AppIcon name="firmware" />
          <span>{{ selectedFile ? selectedFile.name : text.choose }}</span>
          <small v-if="selectedFile">{{ formatBytes(selectedFile.size) }}</small>
        </label>

        <BAlert v-if="fileError" variant="danger" :model-value="true">{{ fileError }}</BAlert>

        <div class="actions">
          <BButton variant="outline-primary" :disabled="busy || !selectedFile || !!fileError" @click="installManual">
            <AppIcon name="upload" /> {{ text.upload }}
          </BButton>
        </div>
      </section>

      <section class="panel safety-card">
        <AppIcon name="shield" />
        <div><strong>{{ text.safetyTitle }}</strong><p>{{ text.safety }}</p></div>
      </section>
    </template>
  </div>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import axios from 'axios'
import { useUiStore } from './stores.js'

const WEBUI_API_VERSION = 1
const { locale } = useI18n()
const uiStore = useUiStore()

const translations = {
  de: {
    title: 'Weboberfläche aktualisieren', subtitle: 'Das New Design wird unabhängig von der Firmware aktualisiert.',
    loading: 'WebUI-Status wird geladen …', externalActive: 'Extern aktiv', fallbackActive: 'Fallback aktiv',
    source: 'Aktive Oberfläche', external: 'Separate WWW-Partition', embedded: 'Eingebettetes New Design',
    externalHint: 'Firmware und WebUI können getrennt aktualisiert werden.', fallbackHint: 'Sicherer Übergangs- und Wiederherstellungsmodus.',
    version: 'Installierte WebUI-Version', storage: 'WWW-Speicher', recommended: 'Empfohlen', onlineTitle: 'Online-Update',
    onlineHint: 'Release laden und direkt auf das Gerät übertragen. Der ESP prüft SHA-256 vor der Aktivierung.',
    available: 'Verfügbare Version', imageSize: 'Image-Größe', installOnline: 'WebUI online aktualisieren', refresh: 'Status aktualisieren',
    expert: 'Expertenmodus', manualTitle: 'WWW-Image manuell hochladen', manualHint: 'Nur das für HB-RF-ETH-ng erzeugte spiffs.bin verwenden.',
    choose: 'spiffs.bin auswählen', upload: 'Image hochladen', safetyTitle: 'Sicherer Fallback',
    safety: 'Bei einem fehlerhaften oder unvollständigen WWW-Update bleibt das eingebettete New Design erreichbar. NVS-Einstellungen werden nicht verändert.',
    downloading: 'WWW-Image wird geladen', verifying: 'SHA-256 wird geprüft', uploading: 'WWW-Image wird übertragen', completed: 'New Design wurde aktualisiert',
    noManifest: 'Dieses Release enthält noch kein separates New-Design-Image.', wrongDesign: 'Das Release-Manifest ist nicht für das New Design bestimmt.',
    sizeMismatch: 'Die Image-Größe passt nicht zur WWW-Partition.', hashMismatch: 'SHA-256-Prüfung fehlgeschlagen.',
    incompatibleApi: 'Diese WebUI benötigt eine andere Firmware-API.', tooOldFirmware: 'Die installierte Firmware ist für diese WebUI zu alt.',
    invalidFile: 'Die Datei muss exakt zur WWW-Partition passen.', wrongFirmwareFile: 'Falsche Datei: Das ist eine Firmware-Datei. Bitte unter System → Firmware installieren.', failed: 'WebUI-Update fehlgeschlagen.'
  },
  en: {
    title: 'Update Web Interface', subtitle: 'The New Design is updated independently from the firmware.',
    loading: 'Loading WebUI status …', externalActive: 'External active', fallbackActive: 'Fallback active',
    source: 'Active interface', external: 'Separate WWW partition', embedded: 'Embedded New Design',
    externalHint: 'Firmware and WebUI can be updated independently.', fallbackHint: 'Safe transition and recovery mode.',
    version: 'Installed WebUI version', storage: 'WWW storage', recommended: 'Recommended', onlineTitle: 'Online update',
    onlineHint: 'Download the release and transfer it to the device. The ESP verifies SHA-256 before activation.',
    available: 'Available version', imageSize: 'Image size', installOnline: 'Update WebUI online', refresh: 'Refresh status',
    expert: 'Expert mode', manualTitle: 'Upload WWW image manually', manualHint: 'Only use the spiffs.bin generated for HB-RF-ETH-ng.',
    choose: 'Select spiffs.bin', upload: 'Upload image', safetyTitle: 'Safe fallback',
    safety: 'If a WWW update is corrupt or incomplete, the embedded New Design remains available. NVS settings are not modified.',
    downloading: 'Downloading WWW image', verifying: 'Verifying SHA-256', uploading: 'Uploading WWW image', completed: 'New Design updated',
    noManifest: 'This release does not include a separate New Design image yet.', wrongDesign: 'The release manifest is not intended for the New Design.',
    sizeMismatch: 'The image size does not match the WWW partition.', hashMismatch: 'SHA-256 verification failed.',
    incompatibleApi: 'This WebUI requires a different firmware API.', tooOldFirmware: 'The installed firmware is too old for this WebUI.',
    invalidFile: 'The file must exactly match the WWW partition.', wrongFirmwareFile: 'Wrong file: this is a firmware image. Install it under System → Firmware.', failed: 'WebUI update failed.'
  }
}

const text = computed(() => translations[locale.value] || translations.de)
const loading = ref(true)
const busy = ref(false)
const progress = ref(0)
const progressPhase = ref('')
const manifestError = ref('')
const fileError = ref('')
const selectedFile = ref(null)
const release = ref({})
const status = ref({ partitionFound: false, mounted: false, valid: false, updateActive: false, partitionSize: 0, totalBytes: 0, usedBytes: 0, bytesWritten: 0, version: '', firmwareVersion: '', design: 'newdesign', lastError: '' })

const currentVersion = computed(() => status.value.version || 'embedded')
const storagePercent = computed(() => {
  const total = status.value.totalBytes || status.value.partitionSize || 0
  return total ? Math.min(100, Math.round((status.value.usedBytes || 0) * 100 / total)) : 0
})

const compareVersions = (left = '', right = '') => {
  const parse = value => {
    const [core, pre = ''] = String(value).split('-', 2)
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

const onlineReady = computed(() => release.value?.design === 'newdesign' && release.value.downloadUrl && /^[0-9a-f]{64}$/i.test(release.value.sha256 || '') && Number(release.value.size) === Number(status.value.partitionSize) && Number(release.value.apiVersion) === WEBUI_API_VERSION && compareVersions(status.value.firmwareVersion, release.value.minFirmwareVersion) >= 0 && status.value.partitionSize > 0)
const progressLabel = computed(() => ({ download: text.value.downloading, verify: text.value.verifying, upload: text.value.uploading, done: text.value.completed })[progressPhase.value] || text.value.completed)

const formatBytes = (bytes) => {
  const value = Number(bytes) || 0
  if (value < 1024) return `${value} B`
  if (value < 1024 * 1024) return `${(value / 1024).toFixed(1)} KB`
  return `${(value / 1024 / 1024).toFixed(2)} MB`
}
const shortHash = (hash) => hash ? `${hash.slice(0, 12)}…${hash.slice(-8)}` : '—'

// SubtleCrypto is unavailable on many HTTP-only private-LAN origins. It is an
// optional browser-side extra; the ESP always calculates the image hash and
// compares it with the release hash before mounting the New Design.
const optionalBrowserSha256 = async (buffer) => {
  if (!globalThis.crypto?.subtle) return null
  const digest = await globalThis.crypto.subtle.digest('SHA-256', buffer)
  return Array.from(new Uint8Array(digest)).map(value => value.toString(16).padStart(2, '0')).join('')
}

const loadStatus = async () => {
  const [storageResponse, systemResponse] = await Promise.all([
    axios.get('/api/webui/status', { timeout: 8000, silent: true }),
    axios.get('/sysinfo.json', { timeout: 8000, silent: true })
  ])
  status.value = {
    ...status.value,
    ...storageResponse.data,
    firmwareVersion: systemResponse.data?.sysInfo?.currentVersion || ''
  }
}

const loadReleaseManifest = async () => {
  manifestError.value = ''
  release.value = {}
  try {
    const info = await axios.get('/api/check_update', { timeout: 15000, silent: true })
    const item = info.data?.webui
    if (!item) return void (manifestError.value = text.value.noManifest)
    if (item.design !== 'newdesign') return void (manifestError.value = text.value.wrongDesign)
    release.value = item
    if (Number(item.size) !== Number(status.value.partitionSize)) manifestError.value = text.value.sizeMismatch
    else if (Number(item.apiVersion) !== WEBUI_API_VERSION) manifestError.value = text.value.incompatibleApi
    else if (compareVersions(status.value.firmwareVersion, item.minFirmwareVersion) < 0) manifestError.value = `${text.value.tooOldFirmware} (≥ ${item.minFirmwareVersion})`
  } catch (error) {
    manifestError.value = error.response?.data?.message || error.message || text.value.noManifest
  }
}

const refreshAll = async () => {
  loading.value = true
  try { await loadStatus(); await loadReleaseManifest() } finally { loading.value = false }
}

const uploadBuffer = async (buffer, expectedHash = '') => {
  progressPhase.value = 'upload'
  progress.value = 50
  const headers = { 'Content-Type': 'application/octet-stream' }
  if (expectedHash) headers['X-WebUI-SHA256'] = expectedHash
  await axios.post('/api/webui/update', buffer, {
    timeout: 120000,
    headers,
    onUploadProgress: event => {
      if (event.total) progress.value = 50 + Math.round(event.loaded * 50 / event.total)
    }
  })
  progress.value = 100
  progressPhase.value = 'done'
  await loadStatus()
  uiStore.pushToast({ type: 'success', title: text.value.completed, message: `v${status.value.version || ''}` })
  window.setTimeout(() => window.location.reload(), 1000)
}

const errorMessage = error => typeof error.response?.data === 'string' ? error.response.data : (error.response?.data?.message || error.message || text.value.failed)

const installOnline = async () => {
  if (!onlineReady.value || busy.value) return
  busy.value = true
  progress.value = 0
  try {
    const item = release.value
    progressPhase.value = 'download'
    const response = await axios.get(item.downloadUrl, {
      responseType: 'arraybuffer', timeout: 120000,
      onDownloadProgress: event => { if (event.total) progress.value = Math.min(45, Math.round(event.loaded * 45 / event.total)) }
    })
    const buffer = response.data
    if (buffer.byteLength !== Number(status.value.partitionSize)) throw new Error(text.value.sizeMismatch)
    progressPhase.value = 'verify'
    progress.value = 47
    const browserHash = await optionalBrowserSha256(buffer)
    if (browserHash && browserHash.toLowerCase() !== item.sha256.toLowerCase()) throw new Error(text.value.hashMismatch)
    progress.value = 50
    await uploadBuffer(buffer, item.sha256)
  } catch (error) {
    uiStore.pushToast({ type: 'error', title: text.value.failed, message: errorMessage(error), duration: 7000 })
  } finally { busy.value = false }
}

const selectFile = event => {
  fileError.value = ''
  selectedFile.value = event.target.files?.[0] || null
  const name = String(selectedFile.value?.name || '').toLowerCase()
  if (name.startsWith('firmware_')) {
    fileError.value = text.value.wrongFirmwareFile
  } else if (selectedFile.value && Number(selectedFile.value.size) !== Number(status.value.partitionSize)) {
    fileError.value = `${text.value.invalidFile} (${formatBytes(status.value.partitionSize)})`
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
    const optionalHash = await optionalBrowserSha256(buffer)
    progress.value = 50
    await uploadBuffer(buffer, optionalHash || '')
  } catch (error) {
    uiStore.pushToast({ type: 'error', title: text.value.failed, message: errorMessage(error), duration: 7000 })
  } finally { busy.value = false }
}

onMounted(refreshAll)
</script>

<style scoped>
.www-page { display: flex; flex-direction: column; gap: 20px; }
.panel { border: 1px solid var(--color-border-light); background: var(--color-surface); box-shadow: var(--shadow-sm); }
.success-chip { color: var(--color-success); }
.warning-chip { color: var(--color-warning); }
.loading-panel { padding: 24px; border-radius: var(--radius-lg); display: flex; gap: 12px; align-items: center; }
.status-grid, .release-grid { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 16px; }
.status-card { padding: 20px; border-radius: var(--radius-lg); display: flex; flex-direction: column; gap: 8px; }
.status-card small, .label, .release-grid span, .compatibility-note { color: var(--color-text-secondary); }
.label, .kicker { font-size: .76rem; text-transform: uppercase; letter-spacing: .08em; font-weight: 800; }
.kicker { color: var(--color-primary); }
.update-card { border-radius: var(--radius-xl); padding: 24px; }
.primary-card { border-color: color-mix(in srgb, var(--color-primary) 35%, var(--color-border-light)); }
.card-heading { display: flex; justify-content: space-between; gap: 20px; margin-bottom: 20px; }
.card-heading h2 { margin: 4px 0 6px; font-size: 1.35rem; }
.card-heading p, .safety-card p { margin: 0; color: var(--color-text-secondary); }
.version-badge { border-radius: var(--radius-full); background: var(--color-primary-soft); color: var(--color-primary); padding: 8px 12px; font-weight: 700; height: fit-content; }
.release-grid { margin-bottom: 10px; }
.release-grid > div { border-radius: var(--radius-md); background: var(--color-background); padding: 14px; display: flex; flex-direction: column; gap: 5px; min-width: 0; }
.release-grid code { overflow: hidden; text-overflow: ellipsis; }
.compatibility-note { margin: 0; font-size: .86rem; }
.actions { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 18px; }
.progress-copy { display: flex; justify-content: space-between; margin: 16px 0 8px; }
.track { height: 7px; border-radius: var(--radius-full); background: var(--color-border-light); overflow: hidden; }
.track span { display: block; height: 100%; background: var(--color-primary); transition: width .2s ease; }
.file-drop { min-height: 120px; border: 1.5px dashed var(--color-border); border-radius: var(--radius-lg); display: flex; flex-direction: column; justify-content: center; align-items: center; gap: 7px; cursor: pointer; padding: 20px; text-align: center; }
.file-drop:hover { border-color: var(--color-primary); background: var(--color-primary-soft); }
.file-drop.disabled { opacity: .6; cursor: not-allowed; }
.file-drop input { display: none; }
.safety-card { border-radius: var(--radius-lg); padding: 18px 20px; display: flex; gap: 14px; align-items: flex-start; }
@media (max-width: 800px) { .status-grid, .release-grid { grid-template-columns: 1fr; } .card-heading { flex-direction: column; } }
</style>
