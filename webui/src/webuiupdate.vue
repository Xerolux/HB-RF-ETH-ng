<template>
  <div class="www-page page-shell">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow"><AppIcon name="firmware" /> WebUI</span>
        <h1 class="hero-title">Weboberfläche aktualisieren</h1>
        <p class="hero-subtitle">Firmware und WebUI besitzen getrennte Versionen. Die Installation erfolgt ausschließlich über ein manuell ausgewähltes 320-KB-WWW-Image.</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="firmware" /> WebUI: {{ installedVersion }}</span>
        <span class="meta-chip"><AppIcon name="firmware" /> Firmware: {{ firmwareVersion || '—' }}</span>
        <span class="meta-chip" :class="status.valid ? 'success-chip' : 'warning-chip'">
          <AppIcon :name="status.valid ? 'check' : 'shield'" />
          {{ status.valid ? 'Separate WebUI aktiv' : 'Eingebetteter Fallback aktiv' }}
        </span>
      </div>
    </section>

    <section class="status-grid">
      <article class="panel status-card">
        <span class="label">Installierte WebUI-Version</span>
        <strong>{{ installedVersion }}</strong>
        <small>{{ status.valid ? 'Aus der separaten WWW-Partition' : 'In der Firmware eingebettet' }}</small>
      </article>
      <article class="panel status-card">
        <span class="label">WWW-Speicher</span>
        <strong>{{ formatBytes(status.usedBytes) }} / {{ formatBytes(status.partitionSize) }}</strong>
        <div class="track"><span :style="{ width: storagePercent + '%' }"></span></div>
      </article>
      <article class="panel status-card">
        <span class="label">Update-Prüfung</span>
        <strong>{{ lastCheckText }}</strong>
        <small>Online-Abfrage höchstens einmal innerhalb von 24 Stunden</small>
      </article>
    </section>

    <div class="content-grid">
      <section class="panel update-card">
        <div class="card-heading">
          <div>
            <span class="kicker">Verfügbare WebUI</span>
            <h2>WebUI-Datei herunterladen</h2>
            <p>Der ESP lädt nur das kleine gemeinsame Update-Manifest. Die BIN-Datei wird direkt von deinem Browser heruntergeladen.</p>
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
          <div><span>Installiert</span><strong>{{ installedVersion }}</strong></div>
          <div><span>Verfügbar</span><strong>{{ release.version }}</strong></div>
          <div><span>Image-Größe</span><strong>{{ formatBytes(release.size) }}</strong></div>
          <div><span>Firmware mindestens</span><strong>{{ release.minFirmwareVersion || '—' }}</strong></div>
        </div>

        <BAlert v-if="release.version && !updateAvailable" variant="success" :model-value="true">
          Die installierte WebUI ist aktuell.
        </BAlert>

        <div class="actions">
          <a
            v-if="updateAvailable && release.downloadUrl"
            class="btn btn-success action-btn"
            :href="release.downloadUrl"
            target="_blank"
            rel="noopener noreferrer"
          >
            <AppIcon name="download" /> WebUI-BIN herunterladen
          </a>
          <a
            v-if="release.releaseUrl"
            class="btn btn-outline-secondary action-btn"
            :href="release.releaseUrl"
            target="_blank"
            rel="noopener noreferrer"
          >
            <AppIcon name="externalLink" /> Release auf GitHub
          </a>
          <BButton variant="outline-secondary" :disabled="loading" @click="refreshCachedStatus">
            <AppIcon name="refresh" /> Gespeicherten Status neu laden
          </BButton>
        </div>
      </section>

      <section class="panel update-card">
        <div class="card-heading">
          <div>
            <span class="kicker">Manuelle Installation</span>
            <h2>WWW-Image hochladen</h2>
            <p>Hier ausschließlich <code>webui_*.bin</code> oder <code>spiffs.bin</code> mit exakt {{ formatBytes(status.partitionSize) }} verwenden.</p>
          </div>
        </div>

        <label class="file-drop" :class="{ disabled: busy, invalid: !!fileError }">
          <input type="file" accept=".bin,application/octet-stream" :disabled="busy" @change="selectFile">
          <AppIcon name="upload" />
          <span>{{ selectedFile ? selectedFile.name : 'WebUI-BIN auswählen' }}</span>
          <small v-if="selectedFile">{{ formatBytes(selectedFile.size) }}</small>
        </label>

        <BAlert v-if="fileError" variant="danger" :model-value="true">{{ fileError }}</BAlert>

        <div v-if="busy" class="progress-panel">
          <div class="progress-copy"><span>WebUI wird übertragen …</span><strong>{{ progress }}%</strong></div>
          <div class="track"><span :style="{ width: progress + '%' }"></span></div>
        </div>

        <BAlert variant="info" :model-value="true">
          Das Gerät prüft Partitionsgröße, internes Manifest und optional SHA-256. Ist das Image ungültig oder unvollständig, bleibt das eingebettete New Design als Fallback aktiv.
        </BAlert>

        <div class="actions">
          <BButton variant="primary" :disabled="busy || !selectedFile || !!fileError" @click="installManual">
            <span v-if="busy" class="spinner-border spinner-border-sm me-2"></span>
            <AppIcon v-else name="upload" /> {{ busy ? 'WebUI wird installiert …' : 'WebUI installieren' }}
          </BButton>
          <BButton v-if="selectedFile" variant="outline-secondary" :disabled="busy" @click="clearFile">
            <AppIcon name="close" /> Auswahl entfernen
          </BButton>
        </div>
      </section>
    </div>
  </div>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import axios from 'axios'
import { useUiStore, useUpdateStore } from './stores.js'

const WEBUI_API_VERSION = 1
const EMBEDDED_WEBUI_VERSION = typeof __WEBUI_VERSION__ !== 'undefined' ? __WEBUI_VERSION__ : 'unbekannt'

const uiStore = useUiStore()
const updateStore = useUpdateStore()
const loading = ref(true)
const busy = ref(false)
const progress = ref(0)
const manifestError = ref('')
const fileError = ref('')
const selectedFile = ref(null)
const release = ref({})
const firmwareVersion = ref('')
const updateFetchedAt = ref(null)
// Manual update search (Korrekturauftrag §6.2/§6.3). This page does not use
// vue-i18n (German-only like the rest of the file); the labels stay local.
const checkNowBusy = ref(false)
const checkNowIdleLabel = 'Jetzt nach Updates suchen'
const checkNowLabel = 'Suche läuft …'
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

const installedVersion = computed(() => status.value.effectiveVersion || status.value.version || EMBEDDED_WEBUI_VERSION)
const storagePercent = computed(() => {
  const total = status.value.totalBytes || status.value.partitionSize || 0
  return total ? Math.min(100, Math.round((status.value.usedBytes || 0) * 100 / total)) : 0
})
const updateAvailable = computed(() => !!release.value.version && compareVersions(installedVersion.value, release.value.version) < 0)
const lastCheckText = computed(() => updateFetchedAt.value ? new Date(Number(updateFetchedAt.value)).toLocaleString() : 'Noch nicht geprüft')

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
  const [storageResponse, systemResponse] = await Promise.all([
    axios.get('/api/webui/status', { timeout: 8000, silent: true }),
    axios.get('/sysinfo.json', { timeout: 8000, silent: true })
  ])
  status.value = { ...status.value, ...storageResponse.data }
  firmwareVersion.value = systemResponse.data?.sysInfo?.currentVersion || ''
}

const loadCachedRelease = async () => {
  manifestError.value = ''
  release.value = {}
  try {
    const response = await axios.get('/api/check_update', { timeout: 8000, silent: true })
    updateFetchedAt.value = response.data?.fetchedAt || null
    const item = response.data?.webui
    if (!item) {
      manifestError.value = 'Im gespeicherten Update-Manifest ist noch keine separate WebUI-Datei enthalten.'
      return
    }
    if (item.design !== 'newdesign') {
      manifestError.value = 'Das bereitgestellte WebUI-Image gehört nicht zum New Design.'
      return
    }
    release.value = item
    if (Number(item.size) !== Number(status.value.partitionSize)) {
      manifestError.value = `Die angebotene WebUI hat eine falsche Größe. Erwartet werden ${formatBytes(status.value.partitionSize)}.`
    } else if (Number(item.apiVersion) !== WEBUI_API_VERSION) {
      manifestError.value = 'Diese WebUI benötigt eine andere Firmware-API.'
    } else if (compareVersions(firmwareVersion.value, item.minFirmwareVersion) < 0) {
      manifestError.value = `Die Firmware ist zu alt. Benötigt wird mindestens ${item.minFirmwareVersion}.`
    }
  } catch (error) {
    manifestError.value = error.response?.data?.message || error.message || 'Gespeicherter Update-Status konnte nicht geladen werden.'
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
      uiStore.pushToast({ type: 'success', title: 'Update verfügbar', message: `WebUI-Version ${release.value.version} ist verfügbar.`, duration: 5000 })
    } else if (outcome === 'no-update' || outcome === 'updated') {
      uiStore.pushToast({ type: 'info', title: 'Alles aktuell', message: 'Es ist keine neuere WebUI vorhanden.', duration: 4000 })
    } else if (outcome === 'cooldown') {
      uiStore.pushToast({ type: 'info', title: 'Kurz gewartet', message: 'Es läuft bereits eine Prüfung oder die 60-Sekunden-Sperre ist noch aktiv.', duration: 4000 })
    } else {
      uiStore.pushToast({ type: 'error', title: 'Prüfung fehlgeschlagen', message: updateStore.checkError || 'Die Update-Prüfung konnte nicht durchgeführt werden.', duration: 6000 })
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
    fileError.value = 'Ungültige Datei: Es wird ein WebUI-Image mit der Endung .bin benötigt.'
  } else if (name.startsWith('firmware_')) {
    fileError.value = 'Falsche Datei: Das ist eine Firmware-Datei. Bitte unter System → Firmware installieren.'
  } else if (Number(selectedFile.value.size) !== Number(status.value.partitionSize)) {
    fileError.value = `Falsche Image-Größe: Erwartet werden exakt ${formatBytes(status.value.partitionSize)} (${status.value.partitionSize} Byte).`
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
    uiStore.pushToast({ type: 'success', title: 'WebUI installiert', message: `Aktive WebUI-Version: ${installedVersion.value}`, duration: 3500 })
    window.setTimeout(() => window.location.reload(), 1200)
  } catch (error) {
    const message = typeof error.response?.data === 'string'
      ? error.response.data
      : (error.response?.data?.message || error.message || 'WebUI-Upload fehlgeschlagen.')
    uiStore.pushToast({ type: 'error', title: 'WebUI-Upload fehlgeschlagen', message, duration: 7000 })
  } finally {
    busy.value = false
    progress.value = 0
  }
}

onMounted(refreshCachedStatus)
</script>

<style scoped>
.status-grid { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:14px; margin-bottom:18px; }
.content-grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:18px; }
.panel { background:var(--color-surface); border:1px solid var(--color-border); border-radius:var(--radius-lg); }
.status-card { padding:16px; display:flex; flex-direction:column; gap:5px; }
.status-card .label,.status-card small { color:var(--color-text-secondary); font-size: var(--fs-xs); }
.status-card strong { font-size: var(--fs-md); overflow-wrap:anywhere; }
.update-card { padding:20px; display:flex; flex-direction:column; gap:16px; }
.card-heading { display:flex; justify-content:space-between; gap:14px; align-items:flex-start; }
.card-heading h2 { margin:.25rem 0; font-size: var(--fs-lg); }
.card-heading p { margin:0; color:var(--color-text-secondary); }
.kicker { color:var(--color-primary-strong); font-size: var(--fs-2xs); font-weight:800; text-transform:uppercase; letter-spacing:.04em; }
.version-badge { padding:5px 9px; border-radius:999px; background:var(--color-primary-soft); font-weight:800; white-space:nowrap; }
.release-grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:10px; }
.release-grid div { padding:12px; border-radius:10px; background:var(--color-bg-alt); display:flex; flex-direction:column; gap:4px; }
.release-grid span { color:var(--color-text-secondary); font-size: var(--fs-xs); }
.release-grid strong { overflow-wrap:anywhere; }
.actions { display:flex; flex-wrap:wrap; gap:10px; }
.action-btn { display:inline-flex; gap:8px; align-items:center; text-decoration:none; }
/* Manual "search now" row — mirrors the Firmware tab so both sub-tabs share
   the same primary action placement (Korrekturauftrag §6.5). */
.check-now-row { display:flex; margin-bottom:4px; }
.check-now-btn { width:auto; }
.file-drop { min-height:145px; border:2px dashed var(--color-border-strong); border-radius:14px; display:flex; flex-direction:column; gap:7px; align-items:center; justify-content:center; text-align:center; cursor:pointer; padding:18px; }
.file-drop input { display:none; }
.file-drop.invalid { border-color:var(--color-danger); }
.file-drop.disabled { opacity:.6; cursor:not-allowed; }
.file-drop small { color:var(--color-text-secondary); }
.track { height:10px; border-radius:999px; background:var(--color-bg-alt); overflow:hidden; }
.track span { display:block; height:100%; background:var(--color-primary); }
.progress-panel { display:flex; flex-direction:column; gap:8px; }
.progress-copy { display:flex; justify-content:space-between; gap:12px; }
@media(max-width:900px){ .status-grid,.content-grid { grid-template-columns:1fr; } }
</style>
