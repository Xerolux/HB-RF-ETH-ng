<template>
  <div class="firmware-page">
    <!-- Countdown Overlay -->
    <Transition name="countdown">
      <div v-if="showCountdown" class="countdown-overlay">
        <div class="countdown-content">
          <div class="countdown-icon">🔄</div>
          <h2 class="countdown-title">{{ t('firmware.restarting') || 'Restarting...' }}</h2>
          <div class="countdown-timer">{{ countdown }}s</div>
          <p class="countdown-text">{{ t('firmware.restartingText') || 'Device is restarting. Page will reload automatically.' }}</p>
          <div class="countdown-progress">
            <div class="countdown-bar" :style="{ width: ((30 - countdown) / 30 * 100) + '%' }"></div>
          </div>
        </div>
      </div>
    </Transition>

    <!-- Header Section -->
    <div class="firmware-header">
      <span class="header-icon">📦</span>
      <div class="header-content">
        <h1 class="header-title">{{ t('firmware.title') || 'Firmware Update' }}</h1>
        <p class="header-subtitle">{{ t('firmware.subtitle') || 'Update your device firmware' }}</p>
      </div>
      <div class="version-badge">
        <span class="version-label">{{ t('firmware.version') || 'Version' }}</span>
        <span class="version-number">{{ sysInfoStore.currentVersion }}</span>
      </div>
    </div>

    <!-- OTA Password Warning Banner -->
    <Transition name="banner">
      <div v-if="!otaPasswordSet && otaPasswordChecked" class="warning-banner">
        <span class="warning-banner-icon">⚠️</span>
        <div class="warning-banner-content">
          <strong>{{ t('firmware.otaPasswordNotSet') || 'OTA Password Not Set' }}</strong>
          {{ t('firmware.otaPasswordNotSetText') || 'You must set an OTA password before firmware updates can be performed.' }}
        </div>
        <BButton variant="light" size="sm" @click="goToSettings">
          {{ t('firmware.goToSettings') || 'Go to Settings' }}
        </BButton>
      </div>
    </Transition>

    <!-- Update Available Banner -->
    <Transition name="banner">
      <div v-if="showUpdateBanner" class="update-banner">
        <span class="update-banner-icon">🎉</span>
        <div class="update-banner-content">
          <strong>{{ t('firmware.updateAvailable') || 'Update Available' }}</strong>
          {{ t('firmware.newVersionAvailable', { version: sysInfoStore.latestVersion }) || `New version ${sysInfoStore.latestVersion} is available!` }}
        </div>
        <BButton variant="light" size="sm" @click="scrollToOta">
          {{ t('firmware.viewUpdate') || 'View' }}
        </BButton>
      </div>
    </Transition>

    <!-- Update Methods Grid -->
    <div class="methods-grid">
      <!-- File Upload Card -->
      <div class="method-card upload-card">
        <div class="card-header">
          <span class="card-icon">📤</span>
          <div>
            <h3 class="card-title">{{ t('firmware.fileUpload') || 'File Upload' }}</h3>
            <p class="card-description">{{ t('firmware.fileUploadHint') || 'Upload a .bin firmware file from your computer' }}</p>
          </div>
        </div>

        <div class="upload-area" :class="{ 'has-file': file }">
          <input
            type="file"
            ref="fileInput"
            accept=".bin"
            @change="handleFileSelect"
            class="file-input"
          />
          <div v-if="!file" class="upload-placeholder" @click="$refs.fileInput.click()">
            <span class="upload-icon">📁</span>
            <span>{{ t('firmware.selectFile') || 'Select .bin file' }}</span>
          </div>
          <div v-else class="file-info">
            <span class="file-name">{{ file.name }}</span>
            <BButton size="sm" variant="outline-danger" @click="clearFile">
              ✕
            </BButton>
          </div>
        </div>

        <BButton
          variant="primary"
          size="lg"
          block
          :disabled="!file || uploading"
          @click="uploadFirmware"
          class="upload-btn"
        >
          <span v-if="uploading" class="spinner-border spinner-border-sm me-2"></span>
          <span>{{ uploading ? (t('firmware.uploading') || 'Uploading...' ) : (t('firmware.upload') || 'Upload Firmware') }}</span>
        </BButton>

        <!-- Upload Progress -->
        <div v-if="uploadProgress > 0" class="upload-progress">
          <div class="progress-bar">
            <div class="progress-fill" :style="{ width: uploadProgress + '%' }"></div>
          </div>
          <span class="progress-text">{{ uploadProgress }}%</span>
        </div>
      </div>

      <!-- Network URL Card -->
      <div class="method-card url-card" ref="otaSection">
        <div class="card-header">
          <span class="card-icon">🌐</span>
          <div>
            <h3 class="card-title">{{ t('firmware.networkUpdate') || 'Network Update' }}</h3>
            <p class="card-description">{{ t('firmware.networkUpdateHint') || 'Download firmware from a URL' }}</p>
          </div>
        </div>

        <BFormGroup>
          <BFormInput
            v-model="otaUrl"
            :placeholder="t('firmware.urlPlaceholder') || 'https://example.com/firmware.bin'"
            :disabled="otaUpdating"
            size="lg"
            class="url-input"
          />
          <BFormText>
            {{ t('firmware.urlHint') || 'Enter the URL to the firmware .bin file' }}
          </BFormText>
        </BFormGroup>

        <div class="quick-urls" v-if="sysInfoStore.latestVersion && sysInfoStore.latestVersion !== 'n/a'">
          <small class="quick-url-label">{{ t('firmware.quickUrl') || 'Quick URL:' }}</small>
          <BButton size="sm" variant="outline-primary" @click="setGithubUrl">
            GitHub v{{ sysInfoStore.latestVersion }}
          </BButton>
        </div>

        <BButton
          variant="success"
          size="lg"
          block
          :disabled="!otaUrl || otaUpdating"
          @click="startOtaUpdate"
          class="ota-btn"
        >
          <span v-if="otaUpdating" class="spinner-border spinner-border-sm me-2"></span>
          <span>{{ otaUpdating ? (t('firmware.downloading') || 'Downloading...' ) : (t('firmware.downloadInstall') || 'Download & Install') }}</span>
        </BButton>

        <!-- Download Progress -->
        <div v-if="otaProgress > 0" class="upload-progress">
          <div class="progress-bar">
            <div class="progress-fill ota-progress" :style="{ width: otaProgress + '%' }"></div>
          </div>
          <span class="progress-text">{{ otaProgress }}%</span>
        </div>
      </div>
    </div>

    <!-- Actions Grid -->
    <div class="actions-grid">
      <!-- Restart Action -->
      <div class="action-card">
        <div class="action-icon restart-icon">🔄</div>
        <div class="action-content">
          <h4>{{ t('firmware.restart') || 'Restart Device' }}</h4>
          <p>{{ t('firmware.restartHint') || 'Restart the device without changing settings' }}</p>
        </div>
        <BButton variant="warning" size="lg" @click="restartClick">
          {{ t('firmware.restart') || 'Restart' }}
        </BButton>
      </div>

      <!-- Factory Reset Action -->
      <div class="action-card">
        <div class="action-icon reset-icon">🔧</div>
        <div class="action-content">
          <h4>{{ t('firmware.factoryReset') || 'Factory Reset' }}</h4>
          <p>{{ t('firmware.factoryResetHint') || 'Reset all settings to factory defaults' }}</p>
        </div>
        <BButton variant="danger" size="lg" @click="factoryResetClick">
          {{ t('firmware.factoryReset') || 'Factory Reset' }}
        </BButton>
      </div>
    </div>

    <!-- Info Section -->
    <div class="info-section">
      <div class="info-card">
        <div class="info-icon">📖</div>
        <div class="info-content">
          <h4>{{ t('firmware.infoTitle') || 'About Firmware Updates' }}</h4>
          <p>{{ t('firmware.infoText') || 'Firmware can be updated via file upload or downloaded directly from a URL. The device will restart automatically after the update.' }}</p>
          <p class="info-link">
            <a href="https://github.com/Xerolux/HB-RF-ETH-ng/releases" target="_blank">
              <span>🔗</span>
              {{ t('firmware.viewReleases') || 'View Releases on GitHub' }}
            </a>
          </p>
        </div>
      </div>
    </div>

    <!-- Status Modal -->
    <BModal v-model="showStatusModal" :title="statusTitle" :header-class="statusHeaderClass" centered hide-footer hide-header-close>
      <div class="status-modal">
        <div class="status-icon">{{ statusIcon }}</div>
        <h3>{{ statusTitle }}</h3>
        <p>{{ statusMessage }}</p>
        <BButton v-if="statusCountdown > 0" variant="primary" disabled class="status-countdown">
          {{ statusCountdown }}s
        </BButton>
      </div>
    </BModal>

    <!-- OTA Password Prompt Modal -->
    <BModal v-model="showOtaPasswordPrompt" :title="t('firmware.otaPasswordRequired') || 'OTA Password Required'" centered hide-header-close>
      <div class="ota-prompt-modal">
        <div class="ota-prompt-icon">🔒</div>
        <p>{{ t('firmware.otaPasswordPromptText') || 'Please enter your OTA password to continue with the firmware update.' }}</p>
        <BForm @submit.stop.prevent="confirmOtaPassword">
          <BFormGroup :label="t('firmware.otaPassword') || 'OTA Password'">
            <BFormInput
              type="password"
              v-model="otaPassword"
              :placeholder="t('firmware.enterOtaPassword') || 'Enter OTA password'"
              size="lg"
              autofocus
              @keyup.enter="confirmOtaPassword"
            />
          </BFormGroup>
          <div class="ota-prompt-actions">
            <BButton variant="secondary" @click="cancelOtaPassword">
              {{ t('common.cancel') || 'Cancel' }}
            </BButton>
            <BButton variant="primary" @click="confirmOtaPassword" :disabled="!otaPassword">
              {{ t('common.confirm') || 'Confirm' }}
            </BButton>
          </div>
        </BForm>
      </div>
    </BModal>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSysInfoStore, useFirmwareUpdateStore } from './stores.js'
import axios from 'axios'

const { t } = useI18n()

const sysInfoStore = useSysInfoStore()
const firmwareUpdateStore = useFirmwareUpdateStore()

// File upload
const file = ref(null)
const fileInput = ref(null)
const uploading = ref(false)
const uploadProgress = ref(0)

// OTA URL update
const otaUrl = ref('')
const otaUpdating = ref(false)
const otaProgress = ref(0)
const otaSection = ref(null)

// OTA Password prompt
const otaPassword = ref('')
const showOtaPasswordPrompt = ref(false)
const pendingAction = ref(null) // 'upload' or 'url'
const otaPasswordSet = ref(false)
const otaPasswordChecked = ref(false)

// NOTE: Password is NOT persisted to localStorage for security reasons.
// Users must re-enter the OTA password for each firmware update session.
// This is a security best practice to prevent password theft via XSS.

// Countdown
const showCountdown = ref(false)
const countdown = ref(30)
const countdownTimer = ref(null)

// Status modal
const showStatusModal = ref(false)
const statusTitle = ref('')
const statusMessage = ref('')
const statusIcon = ref('')
const statusHeaderClass = ref('')
const statusCountdown = ref(0)

const showUpdateBanner = computed(() => {
  const current = sysInfoStore.currentVersion
  const latest = sysInfoStore.latestVersion
  return current && latest && latest !== 'n/a' && latest !== current
})

// File handling
const handleFileSelect = (event) => {
  const selectedFile = event.target.files[0]
  if (selectedFile && selectedFile.name.endsWith('.bin')) {
    file.value = selectedFile
  }
}

const clearFile = () => {
  file.value = null
  if (fileInput.value) {
    fileInput.value.value = ''
  }
}

// Upload firmware
const uploadFirmware = async () => {
  if (!file.value) return

  // Check if OTA password is needed
  if (!otaPassword.value) {
    pendingAction.value = 'upload'
    showOtaPasswordPrompt.value = true
    return
  }

  uploading.value = true
  uploadProgress.value = 0

  try {
    const config = {
      onUploadProgress: (progressEvent) => {
        if (progressEvent.lengthComputable) {
          uploadProgress.value = Math.round((progressEvent.loaded / progressEvent.total) * 100)
        }
      }
    }

    await firmwareUpdateStore.update(file.value, config)

    showStatusModal.value = false
    startCountdown()
  } catch (error) {
    showStatusModal.value = true
    statusTitle.value = t('firmware.uploadError') || 'Upload Failed'
    statusMessage.value = error.response?.data?.error || error.message || t('firmware.uploadErrorText') || 'Failed to upload firmware. Please try again.'
    statusIcon.value = '❌'
    statusHeaderClass.value = 'modal-header-danger'
  } finally {
    uploading.value = false
    uploadProgress.value = 0
  }
}

// Confirm OTA password and proceed with action
const confirmOtaPassword = () => {
  showOtaPasswordPrompt.value = false
  if (pendingAction.value === 'upload') {
    executeUpload()
  } else if (pendingAction.value === 'url') {
    executeOtaUpdate()
  }
  pendingAction.value = null
}

const cancelOtaPassword = () => {
  showOtaPasswordPrompt.value = false
  pendingAction.value = null
}

const executeUpload = async () => {
  if (!file.value) return

  uploading.value = true
  uploadProgress.value = 0

  try {
    const config = {
      onUploadProgress: (progressEvent) => {
        if (progressEvent.lengthComputable) {
          uploadProgress.value = Math.round((progressEvent.loaded / progressEvent.total) * 100)
        }
      }
    }

    await firmwareUpdateStore.update(file.value, config)

    showStatusModal.value = false
    startCountdown()
  } catch (error) {
    showStatusModal.value = true
    statusTitle.value = t('firmware.uploadError') || 'Upload Failed'
    statusMessage.value = error.response?.data?.error || error.message || t('firmware.uploadErrorText') || 'Failed to upload firmware. Please try again.'
    statusIcon.value = '❌'
    statusHeaderClass.value = 'modal-header-danger'
  } finally {
    uploading.value = false
    uploadProgress.value = 0
  }
}

// Set GitHub URL for quick update
const setGithubUrl = () => {
  const version = sysInfoStore.latestVersion
  if (version && version !== 'n/a') {
    otaUrl.value = `https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v${version}/firmware.bin`
  }
}

// Scroll to OTA section
const scrollToOta = () => {
  if (otaSection.value) {
    otaSection.value.scrollIntoView({ behavior: 'smooth' })
  }
}

// Check OTA password status
const checkOtaPasswordStatus = async () => {
  try {
    const response = await axios.get('/api/ota-password-status')
    otaPasswordSet.value = response.data.isSet || false
  } catch (e) {
    otaPasswordSet.value = false
  } finally {
    otaPasswordChecked.value = true
  }
}

// Navigate to settings
const goToSettings = () => {
  window.location.href = '#/settings'
}

// OTA URL Update
const startOtaUpdate = async () => {
  if (!otaUrl.value) return

  // Check if OTA password is needed
  if (!otaPassword.value) {
    pendingAction.value = 'url'
    showOtaPasswordPrompt.value = true
    return
  }

  await executeOtaUpdate()
}

const executeOtaUpdate = async () => {
  if (!otaUrl.value) return

  otaUpdating.value = true
  otaProgress.value = 0

  showStatusModal.value = true
  statusTitle.value = t('firmware.downloading') || 'Downloading...'
  statusMessage.value = t('firmware.otaProgress') || 'Downloading and installing firmware...'
  statusIcon.value = '📥'
  statusHeaderClass.value = 'modal-header-info'

  try {
    // Simulate progress (we don't get real progress from URL OTA)
    const progressInterval = setInterval(() => {
      if (otaProgress.value < 90) {
        otaProgress.value += 10
      }
    }, 500)

    const response = await axios.post('/api/ota_url', { url: otaUrl.value, otaPassword: otaPassword.value })

    clearInterval(progressInterval)

    if (response.data.success) {
      otaProgress.value = 100
      statusMessage.value = t('firmware.otaSuccess') || 'Update successful! Restarting device...'
      setTimeout(() => startCountdown(), 1000)
    }
  } catch (error) {
    showStatusModal.value = true
    statusTitle.value = t('firmware.otaError') || 'Update Failed'
    statusMessage.value = error.response?.data?.error || error.message || t('firmware.otaErrorText') || 'Failed to download firmware. Check the URL and try again.'
    statusIcon.value = '❌'
    statusHeaderClass.value = 'modal-header-danger'
    otaUpdating.value = false
    otaProgress.value = 0
  }
}

// Start countdown after successful update
const startCountdown = () => {
  showCountdown.value = true
  showStatusModal.value = false
  countdown.value = 30

  countdownTimer.value = setInterval(() => {
    countdown.value--

    if (countdown.value <= 0) {
      clearInterval(countdownTimer.value)
      window.location.reload()
    }
  }, 1000)
}

// Restart
const restartClick = async () => {
  showStatusModal.value = true
  statusTitle.value = t('firmware.restarting') || 'Restarting...'
  statusMessage.value = t('firmware.restartProgress') || 'Device is restarting...'
  statusIcon.value = '🔄'
  statusHeaderClass.value = 'modal-header-warning'

  try {
    await axios.post('/api/restart')
    startCountdown()
  } catch (error) {
    startCountdown()
  }
}

// Factory Reset
const factoryResetClick = async () => {
  if (!confirm(t('firmware.factoryResetConfirm') || 'This will erase all settings and restart the device. Continue?')) {
    return
  }

  showStatusModal.value = true
  statusTitle.value = t('firmware.resetting') || 'Resetting...'
  statusMessage.value = t('firmware.resetProgress') || 'Device is resetting to factory settings...'
  statusIcon.value = '🔧'
  statusHeaderClass.value = 'modal-header-danger'

  try {
    await axios.post('/api/factory-reset')
    startCountdown()
  } catch (error) {
    startCountdown()
  }
}

onMounted(() => {
  sysInfoStore.update()
  checkOtaPasswordStatus()
})
</script>

<style scoped>
.firmware-page {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-lg);
}

/* Countdown Overlay */
.countdown-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.9);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 9999;
  backdrop-filter: blur(10px);
}

.countdown-content {
  text-align: center;
  color: white;
  max-width: 400px;
  padding: var(--spacing-xl);
}

.countdown-icon {
  font-size: 4rem;
  margin-bottom: var(--spacing-lg);
  animation: rotate 2s linear infinite;
}

@keyframes rotate {
  from { transform: rotate(0deg); }
  to { transform: rotate(360deg); }
}

.countdown-title {
  font-size: 1.5rem;
  font-weight: 700;
  margin: 0 0 var(--spacing-md) 0;
}

.countdown-timer {
  font-size: 3rem;
  font-weight: 700;
  margin: 0 0 var(--spacing-md) 0;
}

.countdown-text {
  opacity: 0.9;
  margin: 0 0 var(--spacing-lg) 0;
}

.countdown-progress {
  height: 8px;
  background: rgba(255, 255, 255, 0.2);
  border-radius: var(--radius-full);
  overflow: hidden;
}

.countdown-bar {
  height: 100%;
  background: linear-gradient(90deg, #667eea 0%, #764ba2 100%);
  transition: width 1s linear;
}

.countdown-enter-active,
.countdown-leave-active {
  transition: opacity 0.3s;
}

.countdown-enter-from,
.countdown-leave-to {
  opacity: 0;
}

/* Header */
.firmware-header {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  padding: var(--spacing-xl) var(--spacing-lg);
  background: var(--color-surface);
  border-radius: var(--radius-xl);
  box-shadow: var(--shadow-lg);
  flex-wrap: wrap;
}

.header-icon {
  font-size: 3rem;
  filter: drop-shadow(0 2px 8px rgba(0, 0, 0, 0.1));
}

.header-content {
  flex: 1;
  min-width: 200px;
}

.header-title {
  font-size: 1.5rem;
  font-weight: 700;
  margin: 0 0 var(--spacing-xs) 0;
}

.header-subtitle {
  color: var(--color-text-secondary);
  margin: 0;
}

.version-badge {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: var(--spacing-md);
  background: var(--color-bg);
  border-radius: var(--radius-lg);
}

.version-label {
  font-size: 0.75rem;
  color: var(--color-text-secondary);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

.version-number {
  font-size: 1.5rem;
  font-weight: 700;
  color: var(--color-primary);
}

/* Update Banner */
.warning-banner {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  padding: var(--spacing-md) var(--spacing-lg);
  background: linear-gradient(135deg, #f59e0b 0%, #d97706 100%);
  border-radius: var(--radius-lg);
  color: white;
  box-shadow: var(--shadow-lg);
  flex-wrap: wrap;
}

.warning-banner-icon {
  font-size: 2rem;
}

.warning-banner-content {
  flex: 1;
  min-width: 200px;
}

.update-banner {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  padding: var(--spacing-md) var(--spacing-lg);
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border-radius: var(--radius-lg);
  color: white;
  box-shadow: var(--shadow-lg);
  flex-wrap: wrap;
}

.update-banner-icon {
  font-size: 2rem;
}

.update-banner-content {
  flex: 1;
  min-width: 200px;
}

.banner-enter-active,
.banner-leave-active {
  transition: all 0.3s;
}

.banner-enter-from,
.banner-leave-to {
  opacity: 0;
  transform: translateY(-10px);
}

/* Methods Grid */
.methods-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
  gap: var(--spacing-lg);
}

.method-card {
  background: var(--color-surface);
  border-radius: var(--radius-xl);
  padding: var(--spacing-xl);
  box-shadow: var(--shadow-md);
  border: 1px solid var(--color-border);
  display: flex;
  flex-direction: column;
  gap: var(--spacing-md);
}

.card-header {
  display: flex;
  align-items: flex-start;
  gap: var(--spacing-md);
}

.card-icon {
  font-size: 2.5rem;
}

.card-title {
  font-size: 1.125rem;
  font-weight: 600;
  margin: 0 0 var(--spacing-xs) 0;
}

.card-description {
  color: var(--color-text-secondary);
  margin: 0;
}

/* Upload Area */
.upload-area {
  border: 2px dashed var(--color-border);
  border-radius: var(--radius-lg);
  padding: var(--spacing-xl);
  text-align: center;
  transition: all var(--transition-fast);
  cursor: pointer;
  background: var(--color-bg);
}

.upload-area:hover {
  border-color: var(--color-primary);
  background: var(--color-primary-light);
}

.upload-area.has-file {
  border-style: solid;
  border-color: var(--color-success);
  background: var(--color-success-light);
}

.file-input {
  display: none;
}

.upload-placeholder {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: var(--spacing-sm);
  color: var(--color-text-secondary);
}

.upload-icon {
  font-size: 2rem;
  opacity: 0.7;
}

.file-info {
  display: flex;
  align-items: center;
  justify-content: space-between;
  width: 100%;
}

.file-name {
  font-weight: 500;
  color: var(--color-text);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

/* URL Input */
.url-input {
  border: 2px solid var(--color-border);
  border-radius: var(--radius-lg);
}

.url-input:focus {
  border-color: var(--color-primary);
  box-shadow: 0 0 0 4px rgba(255, 107, 53, 0.1);
}

.quick-urls {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
  margin: var(--spacing-sm) 0;
}

.quick-url-label {
  color: var(--color-text-secondary);
}

.ota-progress {
  background: linear-gradient(90deg, #28a745 0%, #20c997 100%);
}

/* Progress Bar */
.upload-progress {
  margin-top: var(--spacing-md);
}

.progress-bar {
  height: 8px;
  background: var(--color-bg);
  border-radius: var(--radius-full);
  overflow: hidden;
}

.progress-fill {
  height: 100%;
  background: linear-gradient(90deg, var(--color-primary) 0%, var(--color-primary-dark) 100%);
  transition: width 0.3s ease;
}

.progress-text {
  display: block;
  text-align: center;
  font-size: 0.875rem;
  font-weight: 600;
  color: var(--color-primary);
  margin-top: var(--spacing-xs);
}

/* Buttons */
.upload-btn,
.ota-btn {
  margin-top: var(--spacing-md);
}

/* Actions Grid */
.actions-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: var(--spacing-lg);
}

.action-card {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  padding: var(--spacing-lg);
  background: var(--color-surface);
  border-radius: var(--radius-xl);
  box-shadow: var(--shadow-md);
  border: 1px solid var(--color-border);
}

.action-icon {
  font-size: 2.5rem;
  flex-shrink: 0;
}

.restart-icon {
  filter: drop-shadow(0 2px 4px rgba(255, 152, 0, 0.3));
}

.reset-icon {
  filter: drop-shadow(0 2px 4px rgba(220, 53, 69, 0.3));
}

.action-content {
  flex: 1;
}

.action-content h4 {
  font-size: 1rem;
  font-weight: 600;
  margin: 0 0 var(--spacing-xs) 0;
}

.action-content p {
  font-size: 0.875rem;
  color: var(--color-text-secondary);
  margin: 0;
}

/* Info Section */
.info-section {
  display: grid;
  grid-template-columns: 1fr;
}

.info-card {
  display: flex;
  align-items: flex-start;
  gap: var(--spacing-md);
  padding: var(--spacing-lg);
  background: var(--color-surface);
  border-radius: var(--radius-lg);
  border: 1px solid var(--color-border);
  box-shadow: var(--shadow-md);
}

.info-icon {
  font-size: 2rem;
}

.info-content h4 {
  font-size: 1rem;
  font-weight: 600;
  margin: 0 0 var(--spacing-xs) 0;
}

.info-content p {
  color: var(--color-text-secondary);
  margin: 0 0 var(--spacing-sm) 0;
}

.info-link {
  margin: 0;
}

.info-link a {
  display: inline-flex;
  align-items: center;
  gap: var(--spacing-sm);
  color: var(--color-primary);
  text-decoration: none;
  font-weight: 500;
}

.info-link a:hover {
  text-decoration: underline;
}

/* Status Modal */
:deep(.modal-content) {
  border-radius: var(--radius-xl);
  border: none;
}

:deep(.modal-header-warning) {
  background: linear-gradient(135deg, #ed8936 0%, #dd6b20 100%);
  color: white;
}

:deep(.modal-header-danger) {
  background: linear-gradient(135deg, #f56565 0%, #e53e3e 100%);
  color: white;
}

:deep(.modal-header-info) {
  background: linear-gradient(135deg, #4299e1 0%, #3182ce 100%);
  color: white;
}

.status-modal {
  text-align: center;
  padding: var(--spacing-lg);
}

.status-icon {
  font-size: 4rem;
  margin-bottom: var(--spacing-md);
}

.status-modal h3 {
  font-size: 1.25rem;
  font-weight: 600;
  margin: 0 0 var(--spacing-sm) 0;
}

.status-modal p {
  color: var(--color-text-secondary);
  margin: 0;
}

.status-countdown {
  width: 80px;
  height: 80px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 1.5rem;
  font-weight: 700;
  margin: var(--spacing-lg) auto 0;
  background: var(--color-bg);
  border: 4px solid var(--color-primary);
}

/* OTA Password Prompt Modal */
.ota-prompt-modal {
  text-align: center;
  padding: var(--spacing-lg);
}

.ota-prompt-icon {
  font-size: 3rem;
  margin-bottom: var(--spacing-md);
}

.ota-prompt-modal p {
  color: var(--color-text-secondary);
  margin-bottom: var(--spacing-lg);
}

.ota-prompt-actions {
  display: flex;
  gap: var(--spacing-md);
  justify-content: center;
  margin-top: var(--spacing-lg);
}

.ota-prompt-actions .btn {
  min-width: 100px;
}

/* Mobile adjustments */
@media (max-width: 640px) {
  .firmware-header {
    padding: var(--spacing-lg) var(--spacing-md);
    flex-direction: column;
    text-align: center;
  }

  .version-badge {
    width: 100%;
  }

  .methods-grid,
  .actions-grid {
    grid-template-columns: 1fr;
  }

  .countdown-timer {
    font-size: 2.5rem;
  }
}
</style>
