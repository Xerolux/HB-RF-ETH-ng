<template>
  <div class="firmware-page">
    <!-- Countdown Overlay -->
    <Transition name="fade">
      <div v-if="showCountdown" class="countdown-overlay">
        <div class="countdown-card">
          <div class="spinner-container">
            <div class="spinner-ring"></div>
            <div class="spinner-icon">🔄</div>
          </div>
          <h2 class="countdown-title">{{ t('firmware.restarting') }}</h2>
          <div class="countdown-value">{{ countdown }}</div>
          <p class="countdown-text">{{ t('firmware.restartingText') }}</p>
          <div class="progress-track">
            <div class="progress-fill" :style="{ width: ((30 - countdown) / 30 * 100) + '%' }"></div>
          </div>
        </div>
      </div>
    </Transition>

    <div class="page-header">
      <div class="icon-wrapper">📦</div>
      <div class="text-wrapper">
        <h1>{{ t('firmware.title') }}</h1>
        <p>{{ t('firmware.subtitle') }}</p>
      </div>
      <div class="version-badge">
        <span class="label">{{ t('firmware.version') }}</span>
        <span class="value">{{ sysInfoStore.currentVersion }}</span>
        <BButton variant="outline-light" size="sm" @click="showChangelogModal = true" class="changelog-btn">
          📋 {{ t('changelog.title') }}
        </BButton>
      </div>
    </div>

    <!-- Update Available Banner -->
    <Transition name="slide-down">
      <div v-if="showUpdateBanner" class="alert-banner info">
        <div class="banner-icon">🎉</div>
        <div class="banner-content">
          <strong>{{ t('firmware.updateAvailable') }}</strong>
          <p>{{ t('firmware.newVersionAvailable', { version: sysInfoStore.latestVersion }) }}</p>
        </div>
        <BButton variant="light" size="sm" @click="scrollToOta" class="banner-action">
          {{ t('firmware.viewUpdate') }}
        </BButton>
      </div>
    </Transition>

    <div class="content-grid">
      <!-- File Upload Card -->
      <div class="update-card">
        <div class="card-header">
          <div class="header-icon bg-primary-light text-primary">📤</div>
          <div class="header-text">
            <h3>{{ t('firmware.fileUpload') }}</h3>
            <p>{{ t('firmware.fileUploadHint') }}</p>
          </div>
        </div>

        <div class="card-body">
          <div
            class="upload-zone"
            :class="{ 'has-file': file, 'dragging': isDragging }"
            @dragover.prevent="isDragging = true"
            @dragleave.prevent="isDragging = false"
            @drop.prevent="handleDrop"
            @click="$refs.fileInput.click()"
          >
            <input
              type="file"
              ref="fileInput"
              accept=".bin"
              @change="handleFileSelect"
              class="hidden-input"
            />

            <template v-if="!file">
              <div class="upload-icon">☁️</div>
              <span class="upload-text">{{ t('firmware.selectFile') }}</span>
            </template>

            <template v-else>
              <div class="file-preview">
                <div class="file-icon">📄</div>
                <div class="file-details">
                  <span class="file-name">{{ file.name }}</span>
                  <span class="file-size">{{ formatSize(file.size) }}</span>
                </div>
                <button @click.stop="clearFile" class="remove-file-btn">✕</button>
              </div>
            </template>
          </div>

          <div v-if="uploadProgress > 0" class="progress-container">
            <div class="progress-bar">
              <div class="progress-value" :style="{ width: uploadProgress + '%' }"></div>
            </div>
            <span class="progress-label">{{ uploadProgress }}%</span>
          </div>

          <BButton
            variant="primary"
            size="lg"
            block
            :disabled="!file || uploading"
            @click="uploadFirmware"
            class="action-btn"
          >
            <span v-if="uploading" class="spinner-border spinner-border-sm me-2"></span>
            {{ uploading ? t('firmware.uploading') : t('firmware.upload') }}
          </BButton>
        </div>
      </div>

      <!-- Network Update Card -->
      <div class="update-card" ref="otaSection">
        <div class="card-header">
          <div class="header-icon bg-success-light text-success">🌐</div>
          <div class="header-text">
            <h3>{{ t('firmware.networkUpdate') }}</h3>
            <p>{{ t('firmware.networkUpdateHint') }}</p>
          </div>
        </div>

        <div class="card-body">
          <div class="url-input-group">
            <BFormInput
              v-model="otaUrl"
              :placeholder="t('firmware.urlPlaceholder')"
              :disabled="otaUpdating"
              class="modern-input"
            />
          </div>

          <div class="quick-actions" v-if="sysInfoStore.latestVersion && sysInfoStore.latestVersion !== 'n/a'">
            <button class="chip-btn" @click="setGithubUrl">
              <span class="chip-icon">🐙</span>
              GitHub v{{ sysInfoStore.latestVersion }}
            </button>
          </div>

          <div v-if="otaProgress > 0" class="progress-container">
            <div class="progress-bar">
              <div class="progress-value success" :style="{ width: otaProgress + '%' }"></div>
            </div>
            <span class="progress-label">{{ otaProgress }}%</span>
          </div>

          <BButton
            variant="success"
            size="lg"
            block
            :disabled="!otaUrl || otaUpdating"
            @click="startOtaUpdate"
            class="action-btn"
          >
            <span v-if="otaUpdating" class="spinner-border spinner-border-sm me-2"></span>
            {{ otaUpdating ? t('firmware.downloading') : t('firmware.downloadInstall') }}
          </BButton>
        </div>
      </div>
    </div>

    <!-- System Actions -->
    <div class="system-actions">
      <div class="action-tile warning" @click="restartClick">
        <div class="tile-icon">🔄</div>
        <div class="tile-text">
          <h4>{{ t('firmware.restart') }}</h4>
          <p>{{ t('firmware.restartHint') }}</p>
        </div>
      </div>

      <div class="action-tile danger" @click="factoryResetClick">
        <div class="tile-icon">🔧</div>
        <div class="tile-text">
          <h4>{{ t('firmware.factoryReset') }}</h4>
          <p>{{ t('firmware.factoryResetHint') }}</p>
        </div>
      </div>
    </div>

    <!-- Status Modal -->
    <BModal v-model="showStatusModal" centered hide-footer hide-header no-close-on-backdrop no-close-on-esc content-class="status-modal-content">
      <div class="status-modal-body" :class="statusType">
        <div class="status-icon-large">{{ statusIcon }}</div>
        <h3>{{ statusTitle }}</h3>
        <p>{{ statusMessage }}</p>
        <BButton v-if="!statusPersistent" @click="showStatusModal = false" variant="secondary" class="mt-3">
          {{ t('common.close') }}
        </BButton>
      </div>
    </BModal>

    <!-- Changelog Modal -->
    <ChangelogModal v-model="showChangelogModal" />

  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSysInfoStore, useFirmwareUpdateStore } from './stores.js'
import axios from 'axios'
import ChangelogModal from './components/ChangelogModal.vue'

const { t } = useI18n()
const sysInfoStore = useSysInfoStore()
const firmwareUpdateStore = useFirmwareUpdateStore()

// State
const file = ref(null)
const fileInput = ref(null)
const isDragging = ref(false)
const uploading = ref(false)
const uploadProgress = ref(0)
const otaUrl = ref('')
const otaUpdating = ref(false)
const otaProgress = ref(0)
const otaSection = ref(null)
const showCountdown = ref(false)
const countdown = ref(30)
const showStatusModal = ref(false)
const statusTitle = ref('')
const statusMessage = ref('')
const statusIcon = ref('')
const statusType = ref('info') // info, success, error, warning
const statusPersistent = ref(false)
const showChangelogModal = ref(false)

const showUpdateBanner = computed(() => {
  const current = sysInfoStore.currentVersion
  const latest = sysInfoStore.latestVersion
  return current && latest && latest !== 'n/a' && latest !== current
})

const formatSize = (bytes) => {
  if (bytes === 0) return '0 B'
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
}

const handleFileSelect = (event) => {
  const selectedFile = event.target.files[0]
  if (selectedFile && selectedFile.name.endsWith('.bin')) {
    file.value = selectedFile
  }
}

const handleDrop = (event) => {
  isDragging.value = false
  const selectedFile = event.dataTransfer.files[0]
  if (selectedFile && selectedFile.name.endsWith('.bin')) {
    file.value = selectedFile
  }
}

const clearFile = () => {
  file.value = null
  if (fileInput.value) fileInput.value.value = ''
}

const uploadFirmware = async () => {
  if (!file.value) return
  executeUpload()
}

const executeUpload = async () => {
  uploading.value = true
  uploadProgress.value = 0

  try {
    const config = {
      onUploadProgress: (p) => {
        if (p.lengthComputable) uploadProgress.value = Math.round((p.loaded / p.total) * 100)
      }
    }
    await firmwareUpdateStore.update(file.value, config)
    startCountdown()
  } catch (error) {
    showStatus('Error', error.response?.data?.error || error.message, '❌', 'error')
  } finally {
    uploading.value = false
    uploadProgress.value = 0
  }
}

const startOtaUpdate = async () => {
  if (!otaUrl.value) return
  executeOtaUpdate()
}

const executeOtaUpdate = async () => {
  otaUpdating.value = true
  otaProgress.value = 0

  // Show non-closable status modal
  showStatus('Downloading', t('firmware.otaProgress'), '📥', 'info', true)

  try {
    const progressInterval = setInterval(() => {
      if (otaProgress.value < 90) otaProgress.value += 5
    }, 200)

    const response = await axios.post('/api/ota_url', { url: otaUrl.value })

    clearInterval(progressInterval)
    otaProgress.value = 100

    if (response.data.success) {
      showStatus('Success', t('firmware.otaSuccess'), '✓', 'success', true)
      setTimeout(startCountdown, 1000)
    }
  } catch (error) {
    showStatus('Error', error.response?.data?.error || error.message, '❌', 'error')
  } finally {
    otaUpdating.value = false
  }
}

const showStatus = (title, message, icon, type = 'info', persistent = false) => {
  statusTitle.value = title
  statusMessage.value = message
  statusIcon.value = icon
  statusType.value = type
  statusPersistent.value = persistent
  showStatusModal.value = true
}

const startCountdown = () => {
  showStatusModal.value = false
  showCountdown.value = true
  countdown.value = 30
  const timer = setInterval(() => {
    countdown.value--
    if (countdown.value <= 0) {
      clearInterval(timer)
      window.location.reload()
    }
  }, 1000)
}

const restartClick = async () => {
  try {
    await axios.post('/api/restart')
    startCountdown()
  } catch (e) {
    startCountdown() // Assume success if network drops
  }
}

const factoryResetClick = async () => {
  if (confirm(t('firmware.factoryResetConfirm'))) {
    try {
      await axios.post('/api/factory-reset')
      startCountdown()
    } catch (e) {
      startCountdown()
    }
  }
}

const setGithubUrl = () => {
  const version = sysInfoStore.latestVersion
  if (version && version !== 'n/a') {
    otaUrl.value = `https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v${version}/firmware.bin`
  }
}

const scrollToOta = () => otaSection.value?.scrollIntoView({ behavior: 'smooth' })

onMounted(() => {
  sysInfoStore.update()
})
</script>

<style scoped>
.firmware-page {
  padding-bottom: 60px;
  max-width: 900px;
  margin: 0 auto;
}

/* Page Header */
.page-header {
  display: flex;
  align-items: center;
  gap: var(--spacing-lg);
  margin-bottom: var(--spacing-xl);
  padding: var(--spacing-lg);
  background: var(--color-surface);
  border-radius: var(--radius-xl);
  box-shadow: var(--shadow-md);
}

.icon-wrapper {
  font-size: 2.5rem;
  width: 60px;
  height: 60px;
  display: flex;
  align-items: center;
  justify-content: center;
  background: var(--color-bg);
  border-radius: var(--radius-lg);
}

.text-wrapper h1 {
  font-size: 1.5rem;
  margin: 0;
}

.text-wrapper p {
  margin: 0;
  color: var(--color-text-secondary);
}

.version-badge {
  margin-left: auto;
  text-align: right;
  background: var(--color-bg);
  padding: 8px 16px;
  border-radius: var(--radius-lg);
  display: flex;
  align-items: center;
  gap: 12px;
}

.version-badge .label {
  display: block;
  font-size: 0.75rem;
  color: var(--color-text-secondary);
  text-transform: uppercase;
}

.version-badge .value {
  font-size: 1.25rem;
  font-weight: 700;
  color: var(--color-primary);
}

.version-badge .changelog-btn {
  font-size: 0.875rem;
  padding: 4px 12px;
  border: 1px solid var(--color-border);
  transition: all 0.2s;
}

.version-badge .changelog-btn:hover {
  border-color: var(--color-primary);
  color: var(--color-primary);
}

/* Banners */
.alert-banner {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  padding: var(--spacing-md) var(--spacing-lg);
  border-radius: var(--radius-lg);
  color: white;
  margin-bottom: var(--spacing-lg);
  box-shadow: var(--shadow-md);
}

.alert-banner.warning {
  background: linear-gradient(135deg, #f59e0b 0%, #d97706 100%);
}

.alert-banner.info {
  background: linear-gradient(135deg, #3b82f6 0%, #2563eb 100%);
}

.banner-icon { font-size: 1.5rem; }
.banner-content { flex: 1; }
.banner-content p { margin: 0; opacity: 0.9; font-size: 0.9375rem; }

/* Content Grid */
.content-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
  gap: var(--spacing-lg);
  margin-bottom: var(--spacing-xl);
}

.update-card {
  background: var(--color-surface);
  border-radius: var(--radius-xl);
  box-shadow: var(--shadow-md);
  overflow: hidden;
  display: flex;
  flex-direction: column;
}

.card-header {
  padding: var(--spacing-lg);
  display: flex;
  gap: var(--spacing-md);
  align-items: center;
  border-bottom: 1px solid var(--color-border-light);
}

.header-icon {
  width: 48px;
  height: 48px;
  border-radius: 12px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 1.5rem;
}

.header-text h3 {
  font-size: 1.125rem;
  margin: 0;
}

.header-text p {
  margin: 0;
  color: var(--color-text-secondary);
  font-size: 0.875rem;
}

.card-body {
  padding: var(--spacing-lg);
  flex: 1;
  display: flex;
  flex-direction: column;
}

/* Upload Zone */
.upload-zone {
  border: 2px dashed var(--color-border);
  border-radius: var(--radius-lg);
  padding: var(--spacing-lg);
  text-align: center;
  cursor: pointer;
  transition: all 0.2s;
  background: var(--color-bg);
  min-height: 140px;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  margin-bottom: var(--spacing-md);
}

.upload-zone:hover, .upload-zone.dragging {
  border-color: var(--color-primary);
  background: var(--color-primary-light);
}

.upload-zone.has-file {
  border-style: solid;
  border-color: var(--color-success);
  background: var(--color-success-light);
}

.upload-icon { font-size: 2.5rem; margin-bottom: var(--spacing-sm); opacity: 0.5; }
.upload-text { color: var(--color-text-secondary); font-weight: 500; }

.file-preview {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  width: 100%;
}

.file-icon { font-size: 2rem; }
.file-details { flex: 1; text-align: left; overflow: hidden; }
.file-name { display: block; font-weight: 600; text-overflow: ellipsis; overflow: hidden; white-space: nowrap; }
.file-size { color: var(--color-text-secondary); font-size: 0.8125rem; }
.remove-file-btn {
  background: transparent; border: none; font-size: 1.25rem; color: var(--color-text-secondary); cursor: pointer;
}

/* Modern Input */
.modern-input {
  border: 2px solid var(--color-border);
  border-radius: var(--radius-lg);
  padding: 12px;
  font-size: 1rem;
}
.modern-input:focus {
  border-color: var(--color-primary);
  box-shadow: 0 0 0 4px rgba(255, 107, 53, 0.1);
}

/* Quick Actions */
.quick-actions {
  margin-top: var(--spacing-sm);
  margin-bottom: var(--spacing-md);
}

.chip-btn {
  background: var(--color-bg);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-full);
  padding: 6px 12px;
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 0.8125rem;
  font-weight: 600;
  color: var(--color-text);
  cursor: pointer;
  transition: all 0.2s;
}

.chip-btn:hover {
  background: var(--color-border-light);
  transform: translateY(-1px);
}

/* Progress */
.progress-container {
  margin-bottom: var(--spacing-md);
}

.progress-bar {
  height: 6px;
  background: var(--color-border-light);
  border-radius: var(--radius-full);
  overflow: hidden;
}

.progress-value {
  height: 100%;
  background: var(--color-primary);
  transition: width 0.3s ease;
}

.progress-value.success { background: var(--color-success); }

.progress-label {
  display: block;
  text-align: right;
  font-size: 0.75rem;
  font-weight: 600;
  margin-top: 4px;
}

.action-btn {
  margin-top: auto;
  border-radius: var(--radius-lg);
  padding: 12px;
}

/* System Actions */
.system-actions {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: var(--spacing-lg);
}

.action-tile {
  background: var(--color-surface);
  padding: var(--spacing-lg);
  border-radius: var(--radius-xl);
  box-shadow: var(--shadow-md);
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  cursor: pointer;
  transition: all 0.2s;
}

.action-tile:hover {
  transform: translateY(-2px);
  box-shadow: var(--shadow-lg);
}

.action-tile.warning:hover { background: var(--color-warning-light); }
.action-tile.danger:hover { background: var(--color-danger-light); }

.tile-icon { font-size: 2rem; }
.tile-text h4 { margin: 0; font-size: 1rem; }
.tile-text p { margin: 0; font-size: 0.8125rem; color: var(--color-text-secondary); }

/* Countdown Overlay */
.countdown-overlay {
  position: fixed;
  top: 0; left: 0; right: 0; bottom: 0;
  background: rgba(0,0,0,0.8);
  backdrop-filter: blur(8px);
  z-index: 9999;
  display: flex;
  align-items: center;
  justify-content: center;
}

.countdown-card {
  text-align: center;
  color: white;
}

.spinner-container {
  position: relative;
  width: 80px;
  height: 80px;
  margin: 0 auto var(--spacing-lg);
}

.spinner-ring {
  position: absolute;
  top: 0; left: 0; width: 100%; height: 100%;
  border: 4px solid rgba(255,255,255,0.1);
  border-top-color: white;
  border-radius: 50%;
  animation: spin 1s linear infinite;
}

.spinner-icon {
  position: absolute;
  top: 50%; left: 50%;
  transform: translate(-50%, -50%);
  font-size: 2rem;
}

@keyframes spin { to { transform: rotate(360deg); } }

.countdown-value {
  font-size: 4rem;
  font-weight: 700;
  margin: var(--spacing-md) 0;
}

.progress-track {
  width: 300px;
  height: 4px;
  background: rgba(255,255,255,0.2);
  border-radius: 2px;
  margin: 0 auto;
}

.progress-fill {
  height: 100%;
  background: white;
  transition: width 1s linear;
}

/* Modals */
:deep(.status-modal-content) {
  border-radius: var(--radius-xl);
  overflow: hidden;
  border: none;
}

.status-modal-body {
  padding: var(--spacing-xl);
  text-align: center;
}

.status-modal-body.error { border-top: 6px solid var(--color-danger); }
.status-modal-body.success { border-top: 6px solid var(--color-success); }
.status-modal-body.info { border-top: 6px solid var(--color-info); }

.status-icon-large { font-size: 4rem; margin-bottom: var(--spacing-md); }

@media (max-width: 768px) {
  .firmware-page {
    padding-bottom: 40px;
  }

  .page-header {
    flex-direction: column;
    text-align: center;
    padding: var(--spacing-md);
    gap: var(--spacing-md);
  }

  .icon-wrapper {
    font-size: 2rem;
    width: 48px;
    height: 48px;
  }

  .text-wrapper h1 {
    font-size: 1.25rem;
  }

  .version-badge {
    width: 100%;
    margin: 0;
    justify-content: center;
    flex-wrap: wrap;
    gap: var(--spacing-sm);
  }

  .content-grid {
    grid-template-columns: 1fr;
    gap: var(--spacing-md);
  }

  .card-header {
    padding: var(--spacing-md);
    flex-direction: row;
    align-items: flex-start;
  }

  .header-icon {
    width: 40px;
    height: 40px;
    font-size: 1.25rem;
  }

  .header-text h3 {
    font-size: 1rem;
  }

  .card-body {
    padding: var(--spacing-md);
  }

  .upload-zone {
    min-height: 100px;
    padding: var(--spacing-md);
  }

  .upload-icon {
    font-size: 2rem;
  }

  .system-actions {
    grid-template-columns: 1fr;
    gap: var(--spacing-md);
  }

  .action-tile {
    padding: var(--spacing-md);
  }

  .alert-banner {
    flex-direction: column;
    text-align: center;
    gap: var(--spacing-sm);
    padding: var(--spacing-md);
  }

  /* Countdown */
  .progress-track {
    width: 80%;
    max-width: 300px;
  }

  .countdown-value {
    font-size: 3rem;
  }
}

/* Utility Colors */
.bg-primary-light { background-color: var(--color-primary-light); }
.bg-success-light { background-color: var(--color-success-light); }
.text-primary { color: var(--color-primary); }
.text-success { color: var(--color-success); }
</style>
