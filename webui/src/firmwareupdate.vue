<template>
  <div class="firmware-page page-shell">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow"><AppIcon name="firmware" /> Firmware</span>
        <h1 class="hero-title">Firmware aktualisieren</h1>
        <p class="hero-subtitle">Updates werden einmal täglich gesucht. Die Installation erfolgt ausschließlich über eine lokal ausgewählte BIN-Datei.</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="firmware" /> Installiert: {{ sysInfoStore.currentVersion || '—' }}</span>
        <span class="meta-chip"><AppIcon name="clock" /> {{ lastCheckText }}</span>
      </div>
    </section>

    <div class="content-grid">
      <section class="update-card">
        <div class="card-header">
          <div class="header-icon bg-success-light text-success"><AppIcon name="download" /></div>
          <div class="header-text">
            <h2>Verfügbare Firmware</h2>
            <p>Der ESP lädt nur das kleine Update-Manifest. Die Firmware-Datei wird von deinem Browser heruntergeladen.</p>
          </div>
        </div>
        <div class="card-body">
          <BAlert v-if="updateStore.checkError" variant="warning" :model-value="true">
            Update-Prüfung fehlgeschlagen: {{ updateStore.checkError }}
          </BAlert>

          <div v-if="updateStore.updateAvailable" class="release-box">
            <div>
              <span class="release-label">Neue Version</span>
              <strong>v{{ updateStore.latestVersion }}</strong>
              <small v-if="updateStore.publishedAt">Veröffentlicht: {{ formatDate(updateStore.publishedAt) }}</small>
            </div>
            <span v-if="updateStore.isPrerelease" class="beta-badge">Beta</span>
          </div>
          <div v-else class="release-box is-current">
            <div>
              <span class="release-label">Status</span>
              <strong>{{ updateStore.latestVersion && updateStore.latestVersion !== 'n/a' ? 'Firmware ist aktuell' : 'Noch kein Prüfergebnis vorhanden' }}</strong>
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
              <AppIcon name="download" /> Firmware-BIN herunterladen
            </a>
            <a
              v-if="updateStore.releaseUrl"
              class="btn btn-outline-secondary action-btn"
              :href="updateStore.releaseUrl"
              target="_blank"
              rel="noopener noreferrer"
            >
              <AppIcon name="externalLink" /> Release auf GitHub
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
              Beta-Versionen anzeigen
              <span class="beta-toggle-hint">Die Änderung gilt bei der nächsten automatischen Prüfung innerhalb des 24-Stunden-Zyklus.</span>
            </label>
          </div>
        </div>
      </section>

      <section class="update-card">
        <div class="card-header">
          <div class="header-icon bg-primary-light text-primary"><AppIcon name="upload" /></div>
          <div class="header-text">
            <h2>Firmware manuell installieren</h2>
            <p>Hier ausschließlich <code>firmware_*.bin</code> verwenden. WebUI-Dateien gehören unter <strong>System → WebUI</strong>.</p>
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
              <span class="upload-text">Firmware-BIN auswählen oder hierher ziehen</span>
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
            Während des Schreibens Stromversorgung und Netzwerk nicht unterbrechen. Nach erfolgreichem Upload startet das Gerät mit dauerhaft aktivem Neustart-Sync neu.
          </BAlert>

          <BButton variant="primary" size="lg" block class="action-btn"
                   :disabled="!file || !!fileError || uploading" @click="uploadFirmware">
            <span v-if="uploading" class="spinner-border spinner-border-sm me-2"></span>
            <AppIcon v-else name="upload" /> {{ uploading ? 'Firmware wird übertragen …' : 'Firmware installieren' }}
          </BButton>
        </div>
      </section>
    </div>

    <section class="system-actions">
      <button type="button" class="action-tile warning" @click="restartClick">
        <div class="tile-icon"><AppIcon name="refresh" /></div>
        <div class="tile-text"><h4>Neustart</h4><p>Gerät mit aktivem Neustart-Sync neu starten</p></div>
      </button>
      <button type="button" class="action-tile danger" @click="openFactoryResetModal">
        <div class="tile-icon"><AppIcon name="restore" /></div>
        <div class="tile-text"><h4>Werkseinstellungen</h4><p>Alle gespeicherten Einstellungen löschen</p></div>
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

const { t } = useI18n()
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
  if (!updateStore.lastCheck) return 'Noch nicht geprüft'
  return `Geprüft: ${new Date(updateStore.lastCheck).toLocaleString()}`
})

const formatDate = value => value ? new Date(value).toLocaleString() : '—'
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
    fileError.value = 'Ungültige Datei: Es wird eine Firmware-Datei mit der Endung .bin benötigt.'
    return
  }
  if (name.startsWith('webui_') || name === 'spiffs.bin' || Number(selectedFile.size) === WEBUI_IMAGE_SIZE) {
    fileError.value = 'Falsche Datei: Das ist ein WebUI-/WWW-Image. Bitte unter System → WebUI installieren.'
    return
  }
  if (selectedFile.size < 1024) {
    fileError.value = 'Die ausgewählte Datei ist zu klein und kann keine gültige Firmware sein.'
    return
  }

  try {
    const firstByte = new Uint8Array(await selectedFile.slice(0, 1).arrayBuffer())[0]
    if (firstByte !== ESP_IMAGE_MAGIC) {
      fileError.value = 'Ungültiges Firmware-Image: Der ESP32-Dateikopf 0xE9 fehlt. Die Datei wurde nicht angenommen.'
      return
    }
  } catch {
    fileError.value = 'Die ausgewählte Datei konnte nicht geprüft werden.'
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
    uiStore.pushToast({ type: 'success', title: 'Firmware übertragen', message: 'Das Gerät startet jetzt neu.', duration: 2500 })
    restartUiStore.start({ includeFlashPause: true, syncSeconds: 40, restartSeconds: 30 })
  } catch (error) {
    const message = typeof error.response?.data === 'string'
      ? error.response.data
      : (error.response?.data?.error || error.message || 'Firmware-Upload fehlgeschlagen.')
    uiStore.pushToast({ type: 'error', title: 'Firmware-Upload fehlgeschlagen', message, duration: 7000 })
  } finally {
    uploading.value = false
    uploadProgress.value = 0
  }
}

const onBetaToggle = async event => {
  const enabled = event.target.checked
  betaToggleSaving.value = true
  try {
    await axios.post('/settings.json', { betaChannel: enabled })
    updateStore.betaChannel = enabled
    uiStore.pushToast({ type: 'success', title: 'Update-Kanal gespeichert', message: 'Die Auswahl gilt bei der nächsten täglichen Prüfung.' })
  } catch (error) {
    event.target.checked = !enabled
    uiStore.pushToast({ type: 'error', title: 'Speichern fehlgeschlagen', message: error.response?.data?.error || error.message })
  } finally {
    betaToggleSaving.value = false
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
.content-grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:18px; }
.update-card { background:var(--color-surface); border:1px solid var(--color-border); border-radius:var(--radius-lg); overflow:hidden; }
.card-header { display:flex; gap:14px; align-items:flex-start; padding:20px; border-bottom:1px solid var(--color-border); }
.header-icon { width:44px; height:44px; flex:0 0 auto; border-radius:var(--radius-md); display:flex; align-items:center; justify-content:center; }
.header-text h2 { margin:0; font-size: var(--fs-lg); font-weight: var(--font-weight-semibold); }
.header-text p { margin:.35rem 0 0; color:var(--color-text-secondary); }
.card-body { padding:20px; display:flex; flex-direction:column; gap:16px; }
.release-box { display:flex; justify-content:space-between; gap:14px; align-items:center; padding:16px; border-radius:var(--radius-md); background:var(--color-success-soft); }
.release-box.is-current { background:var(--color-bg-alt); }
.release-box div { display:flex; flex-direction:column; gap:3px; }
.release-label, .release-box small { color:var(--color-text-secondary); font-size: var(--fs-xs); }
.release-box strong { font-size: var(--fs-lg); }
.beta-badge { padding:4px 8px; border-radius:var(--radius-pill); background:var(--color-warning-soft); font-size: var(--fs-2xs); font-weight: var(--font-weight-bold); }
.actions { display:flex; flex-wrap:wrap; gap:10px; }
.action-btn { display:inline-flex; align-items:center; justify-content:center; gap:8px; text-decoration:none; }
.beta-toggle-row { display:flex; gap:12px; align-items:flex-start; padding-top:14px; border-top:1px solid var(--color-border); }
.beta-toggle-label { display:flex; flex-direction:column; gap:3px; font-weight: var(--font-weight-bold); }
.beta-toggle-hint { font-size: var(--fs-xs); font-weight: var(--font-weight-normal); color:var(--color-text-secondary); }
.upload-zone { min-height:150px; border:2px dashed var(--color-border-strong); border-radius:var(--radius-lg); display:flex; align-items:center; justify-content:center; cursor:pointer; padding:18px; text-align:center; }
.upload-zone.dragging { border-color:var(--color-primary); background:var(--color-primary-soft); }
.upload-zone.invalid { border-color:var(--color-danger); }
.hidden-input { display:none; }
.upload-icon { font-size: var(--fs-3xl); margin-bottom:8px; }
.upload-text { display:block; font-weight: var(--font-weight-bold); }
.file-preview { width:100%; display:flex; gap:12px; align-items:center; text-align:left; }
.file-details { min-width:0; flex:1; display:flex; flex-direction:column; }
.file-name { overflow-wrap:anywhere; font-weight: var(--font-weight-bold); }
.file-size { color:var(--color-text-secondary); font-size: var(--fs-xs); }
.remove-file-btn { border:0; background:transparent; color:var(--color-danger); padding:8px; }
.progress-container { display:flex; align-items:center; gap:10px; }
.progress-bar { flex:1; height:10px; border-radius:var(--radius-pill); background:var(--color-bg-alt); overflow:hidden; }
.progress-value { height:100%; background:var(--color-primary); }
.system-actions { margin-top:18px; display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:14px; }
.action-tile { width:100%; display:flex; gap:12px; align-items:center; text-align:left; padding:16px; border:1px solid var(--color-border); border-radius:var(--radius-lg); background:var(--color-surface); color:var(--color-text); cursor: pointer; }
.action-tile h4 { font-weight: var(--font-weight-semibold); }
.action-tile h4,.action-tile p { margin:0; }
.action-tile p { color:var(--color-text-secondary); font-size: var(--fs-xs); }
.action-tile.warning { border-color: var(--color-warning); background: var(--color-warning-soft); }
.action-tile.danger { border-color:var(--color-danger); background: var(--color-danger-soft); }
.action-tile.danger h4 { color: var(--color-danger); }
@media(max-width:900px){ .content-grid,.system-actions { grid-template-columns:1fr; } }
</style>
