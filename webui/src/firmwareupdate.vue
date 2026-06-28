<template>
  <div class="firmware-page">
    <!-- Countdown Overlay -->
    <Transition name="fade">
      <div v-if="showCountdown" class="countdown-overlay">
        <div class="countdown-card">
          <div class="spinner-container">
            <div class="spinner-ring"></div>
            <div class="spinner-icon"><AppIcon name="refresh" /></div>
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

    <div class="page-header page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow">
          <AppIcon name="firmware" />
          {{ t('firmware.title') }}
        </span>
        <h1 class="hero-title">{{ t('firmware.title') }}</h1>
        <p class="hero-subtitle">{{ t('firmware.subtitle') }}</p>
      </div>
      <div class="hero-meta version-badge">
        <span class="label">{{ t('sysinfo.version') }}</span>
        <span class="value">{{ sysInfoStore.currentVersion }}</span>
        <BButton variant="outline-secondary" size="sm" @click="showChangelogModal = true" class="changelog-btn">
          <AppIcon name="logs" />
          {{ t('changelog.title') }}
        </BButton>
      </div>
    </div>

    <div class="quick-actions">
      <button class="chip-btn" type="button" @click="manualCheckForUpdate" :disabled="updateStore.isChecking">
        <AppIcon name="refresh" />
        {{ updateStore.isChecking ? t('firmware.checking') : t('firmware.checkNow') }}
      </button>
      <button class="chip-btn" type="button" @click="scrollToOta">
        <AppIcon name="download" />
        {{ t('firmware.jumpToOta') }}
      </button>
      <span class="chip-btn static">
        <AppIcon name="clock" />
        {{ updateStore.lastCheck ? t('firmware.lastCheckAt', { time: formatLastCheck(updateStore.lastCheck) }) : t('firmware.noRecentCheck') }}
      </span>
    </div>

    <!-- Update Available Banner -->
    <Transition name="slide-down">
      <div v-if="showUpdateBanner" class="alert-banner" :class="updateStore.isPrerelease ? 'warning' : 'info'">
        <div class="banner-icon"><AppIcon name="download" /></div>
        <div class="banner-content">
          <strong>
            {{ t('firmware.updateAvailable', { latestVersion: sysInfoStore.latestVersion }) }}
            <span v-if="updateStore.isPrerelease" class="beta-badge">{{ t('firmware.beta') }}</span>
          </strong>
          <p>{{ t('firmware.newVersionAvailable', { version: sysInfoStore.latestVersion }) }}</p>
          <details v-if="updateStore.releaseNotes" class="release-notes-preview">
            <summary>{{ t('firmware.releaseNotesPreview') }}</summary>
            <div class="release-notes-body" v-html="releaseNotesExcerpt"></div>
            <a v-if="updateStore.releaseUrl" :href="updateStore.releaseUrl" target="_blank" rel="noopener noreferrer" class="release-link">
              {{ t('firmware.viewOnGithub') }} <AppIcon name="externalLink" class="external-link-icon" />
            </a>
          </details>
        </div>
        <BButton variant="light" size="sm" @click="showChangelogModal = true" class="banner-action">
          {{ t('firmware.viewUpdate') }}
        </BButton>
      </div>
    </Transition>

    <div class="content-grid">
      <!-- File Upload Card -->
      <div class="update-card">
        <div class="card-header">
          <div class="header-icon bg-primary-light text-primary"><AppIcon name="upload" /></div>
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
              <div class="upload-icon"><AppIcon name="upload" /></div>
              <span class="upload-text">{{ t('firmware.selectFile') }}</span>
            </template>

            <template v-else>
              <div class="file-preview">
                <div class="file-icon"><AppIcon name="file" /></div>
                <div class="file-details">
                  <span class="file-name">{{ file.name }}</span>
                  <span class="file-size">{{ formatSize(file.size) }}</span>
                </div>
                <button @click.stop="clearFile" class="remove-file-btn"><AppIcon name="close" /></button>
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
          <div class="header-icon bg-success-light text-success"><AppIcon name="globe" /></div>
          <div class="header-text">
            <h3>{{ t('firmware.networkUpdate') }}</h3>
            <p>{{ t('firmware.networkUpdateHint') }}</p>
          </div>
        </div>

        <div class="card-body">
          <div v-if="updateStore.latestVersion && updateStore.latestVersion !== 'n/a' && updateStore.updateAvailable" class="update-info">
            <div class="update-available">
              <span class="update-icon"><AppIcon name="download" /></span>
              <div class="update-text">
                <strong>
                  {{ t('firmware.updateAvailable') }}
                  <span v-if="updateStore.isPrerelease" class="beta-badge">{{ t('firmware.beta') }}</span>
                </strong>
                <span class="version-info">v{{ updateStore.latestVersion }}</span>
              </div>
            </div>
            <details v-if="updateStore.releaseNotes" class="release-notes-preview">
              <summary>{{ t('firmware.releaseNotesPreview') }}</summary>
              <div class="release-notes-body" v-html="releaseNotesExcerpt"></div>
            </details>
          </div>
          <div v-else class="update-info">
            <div class="no-update">
              <span class="check-icon"><AppIcon name="check" /></span>
              <span>{{ t('firmware.upToDate') }}</span>
            </div>
            <button class="check-btn" @click="manualCheckForUpdate" :disabled="updateStore.isChecking">
              <span v-if="updateStore.isChecking" class="spinner-border spinner-border-sm"></span>
              {{ updateStore.isChecking ? t('firmware.checking') : t('firmware.checkNow') }}
            </button>
            <div class="form-text mt-2 text-muted" style="font-size: 0.75rem; line-height: 1.2;">
               {{ t('privacy.updateCheck') }}
            </div>
            <div v-if="updateStore.lastCheck" class="last-check">
              {{ t('firmware.lastCheck') }}: {{ formatLastCheck(updateStore.lastCheck) }}
            </div>
          </div>

          <!-- Beta channel toggle -->
          <div class="beta-toggle-row">
            <div class="form-check form-switch">
              <input
                class="form-check-input"
                type="checkbox"
                role="switch"
                id="betaChannelSwitch"
                :checked="updateStore.betaChannel"
                @change="onBetaToggle"
                :disabled="betaToggleSaving"
              >
            </div>
            <label for="betaChannelSwitch" class="beta-toggle-label">
              {{ t('firmware.betaChannel') }}
              <span class="beta-toggle-hint">{{ t('firmware.betaChannelHint') }}</span>
            </label>
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
            :disabled="!updateStore.updateAvailable || !updateStore.downloadUrl || otaUpdating"
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
        <div class="tile-icon"><AppIcon name="refresh" /></div>
        <div class="tile-text">
          <h4>{{ t('firmware.restart') }}</h4>
          <p>{{ t('firmware.restartHint') }}</p>
        </div>
      </div>

      <div class="action-tile danger" @click="factoryResetClick">
        <div class="tile-icon"><AppIcon name="restore" /></div>
        <div class="tile-text">
          <h4>{{ t('firmware.factoryReset') }}</h4>
          <p>{{ t('firmware.factoryResetHint') }}</p>
        </div>
      </div>
    </div>

    <!-- Changelog Modal -->
    <ChangelogModal v-model="showChangelogModal" />

  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSysInfoStore, useUpdateStore, useFirmwareUpdateStore, useUiStore } from './stores.js'
import axios from 'axios'
import { marked } from 'marked'
import ChangelogModal from './components/ChangelogModal.vue'

const { t } = useI18n()
const sysInfoStore = useSysInfoStore()
const updateStore = useUpdateStore()
const firmwareUpdateStore = useFirmwareUpdateStore()
const uiStore = useUiStore()

// State
const file = ref(null)
const fileInput = ref(null)
const isDragging = ref(false)
const uploading = ref(false)
const uploadProgress = ref(0)
const otaUpdating = ref(false)
const otaProgress = ref(0)
const otaSection = ref(null)
const showCountdown = ref(false)
const countdown = ref(30)
const showChangelogModal = ref(false)
const betaToggleSaving = ref(false)

const showUpdateBanner = computed(() => {
  const current = sysInfoStore.currentVersion
  const latest = sysInfoStore.latestVersion
  if (!current || !latest || latest === 'n/a') return false
  return updateStore.compareVersions(current, latest) < 0
})

const escapeHtml = (value) => value
  .replaceAll('&', '&amp;')
  .replaceAll('<', '&lt;')
  .replaceAll('>', '&gt;')
  .replaceAll('"', '&quot;')
  .replaceAll("'", '&#39;')

// Render the GitHub release body markdown to safe HTML. The body is
// author-controlled (release.yml generates RELEASE_NOTES.md) but we still
// escape before passing to marked so injected markup can't sneak through.
const releaseNotesExcerpt = computed(() => {
  const body = updateStore.releaseNotes
  if (!body) return ''
  try {
    return marked.parse(escapeHtml(body))
  } catch (e) {
    return `<pre>${escapeHtml(body)}</pre>`
  }
})

const onBetaToggle = async (event) => {
  const enabled = event.target.checked
  betaToggleSaving.value = true
  try {
    await axios.post('/settings.json', { betaChannel: enabled })
    updateStore.betaChannel = enabled
    uiStore.pushToast({
      type: 'success',
      title: t('common.success'),
      message: enabled ? t('firmware.betaChannelOn') : t('firmware.betaChannelOff'),
      duration: 2000
    })
    // Re-check immediately so the user sees the effect of the toggle.
    await updateStore.checkForUpdate(sysInfoStore.currentVersion)
  } catch (error) {
    // Revert checkbox on error.
    event.target.checked = !enabled
    uiStore.pushToast({
      type: 'error',
      title: t('common.error'),
      message: error.response?.data?.error || error.message
    })
  } finally {
    betaToggleSaving.value = false
  }
}

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
    await firmwareUpdateStore.update(file.value, {
      onUploadProgress: (p) => {
        if (p.lengthComputable) uploadProgress.value = Math.round((p.loaded / p.total) * 100)
      }
    })
    startCountdown()
  } catch (error) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: error.response?.data?.error || error.message })
  } finally {
    uploading.value = false
    uploadProgress.value = 0
  }
}

const startOtaUpdate = async () => {
  const version = updateStore.latestVersion
  if (!version || version === 'n/a') return

  // Prefer the asset URL advertised by the GitHub release. Fall back to the
  // legacy mirror only if the device somehow has a version but no URL
  // (e.g. asset upload is still in progress) - this should be rare.
  const updateUrl = updateStore.downloadUrl
  if (!updateUrl) {
    uiStore.pushToast({
      type: 'error',
      title: t('common.error'),
      message: t('firmware.noDownloadUrl')
    })
    return
  }

  otaUpdating.value = true
  otaProgress.value = 0

  uiStore.pushToast({ type: 'info', title: t('firmware.title'), message: t('firmware.otaProgress'), duration: 2200 })

  try {
    const response = await axios.post('/api/ota_url', { url: updateUrl })

    if (response.data.success) {
      await pollOtaStatus()
      uiStore.pushToast({ type: 'success', title: t('common.success'), message: t('firmware.otaSuccess'), duration: 2400 })
      setTimeout(startCountdown, 1000)
    } else {
      // Firmware returns HTTP 200 with { success: false, error } for several
      // legitimate conditions (already-in-progress, invalid URL, ...). Surface
      // it instead of silently doing nothing.
      uiStore.pushToast({ type: 'error', title: t('common.error'), message: response.data.error || 'OTA update failed' })
    }
  } catch (error) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: error.response?.data?.error || error.message || 'OTA update failed' })
  } finally {
    otaUpdating.value = false
  }
}

let otaPollTimer = null
let otaPollCancelled = false

const pollOtaStatus = () => {
  otaPollCancelled = false
  return new Promise((resolve, reject) => {
    // The device may briefly stop answering while it writes flash - tolerate
    // a bounded number of consecutive fetch errors instead of resolving
    // (= reporting success) on the first 3 s timeout, which used to start
    // the reboot countdown even when the OTA later failed.
    let consecutiveErrors = 0

    const poll = async () => {
      otaPollTimer = null
      if (otaPollCancelled) {
        resolve()
        return
      }
      try {
        const res = await axios.get('/api/ota_status', { timeout: 3000, silent: true })
        const { status, progress, error: otaError } = res.data

        consecutiveErrors = 0
        otaProgress.value = progress || 0

        if (status === 'downloading') {
          otaPollTimer = setTimeout(poll, 1000)
        } else if (status === 'success') {
          otaProgress.value = 100
          resolve()
        } else if (status === 'failed') {
          reject(new Error(otaError || 'OTA update failed'))
        } else {
          resolve()
        }
      } catch (err) {
        consecutiveErrors++
        if (consecutiveErrors >= 10) {
          reject(new Error('Lost connection to the device during the update'))
        } else {
          otaPollTimer = setTimeout(poll, 2000)
        }
      }
    }
    otaPollTimer = setTimeout(poll, 1000)
  })
}

let countdownTimer = null

const startCountdown = () => {
  // Clear any previous countdown interval so repeated triggers (e.g. restart
  // clicked twice, or both try/catch branches firing on a network drop) cannot
  // stack two intervals racing to reload the page.
  if (countdownTimer) {
    clearInterval(countdownTimer)
    countdownTimer = null
  }
  showCountdown.value = true
  countdown.value = 30
  countdownTimer = setInterval(() => {
    countdown.value--
    if (countdown.value <= 0) {
      clearInterval(countdownTimer)
      countdownTimer = null
      window.location.reload()
    }
  }, 1000)
}

const restartClick = async () => {
  if (!confirm(t('firmware.restartConfirm'))) return
  try {
    await axios.post('/api/restart')
    uiStore.pushToast({ type: 'info', title: t('common.success'), message: t('firmware.restartingText'), duration: 1200 })
    startCountdown()
  } catch (e) {
    startCountdown() // Assume success if network drops
  }
}

const factoryResetClick = async () => {
  if (confirm(t('firmware.factoryResetConfirm'))) {
    try {
      await axios.post('/api/factory-reset')
      uiStore.pushToast({ type: 'warning', title: t('common.success'), message: t('firmware.restartingText'), duration: 1200 })
      startCountdown()
    } catch (e) {
      startCountdown()
    }
  }
}

const scrollToOta = () => otaSection.value?.scrollIntoView({ behavior: 'smooth' })

const manualCheckForUpdate = async () => {
  if (!sysInfoStore.currentVersion) {
    await sysInfoStore.update()
  }
  await updateStore.checkForUpdate(sysInfoStore.currentVersion)

  if (updateStore.checkError) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: `${t('firmware.checkFailed')}: ${updateStore.checkError}` })
  } else {
    uiStore.pushToast({ type: 'success', title: t('common.success'), message: t('firmware.checkSuccess'), duration: 2200 })
  }
}

const formatLastCheck = (dateStr) => {
  if (!dateStr) return ''
  const date = new Date(dateStr)
  return date.toLocaleString()
}

let updateCheckInterval = null

onMounted(async () => {
  try {
    await sysInfoStore.update()
    // Read the device's cached snapshot first so the page renders without
    // waiting for a GitHub API call. The device has its own 24h background
    // task that talks to GitHub; we don't want every page open to consume
    // a (rate-limited) API call.
    await updateStore.checkForUpdate(sysInfoStore.currentVersion, { cached: true })

    // If the device has never fetched (or was just rebooted and the 30 s
    // startup timer hasn't fired yet), trigger one explicit refresh so the
    // user doesn't stare at "Firmware is up to date" with latestVersion=n/a.
    if (updateStore.latestVersion === 'n/a' && !updateStore.fetchInProgress) {
      updateStore.checkForUpdate(sysInfoStore.currentVersion).catch(() => {})
    }
  } catch (e) {
    console.warn('Initial update check failed:', e.response?.status || e.message)
  }
  // Periodically re-read the cache while the page is open so the
  // "last check" indicator and download URL stay fresh.
  updateCheckInterval = setInterval(() => {
    if (sysInfoStore.currentVersion) {
      updateStore.checkForUpdate(sysInfoStore.currentVersion, { cached: true })
    }
  }, 60 * 1000)
})

onUnmounted(() => {
  if (updateCheckInterval) {
    clearInterval(updateCheckInterval)
  }
  if (countdownTimer) {
    clearInterval(countdownTimer)
    countdownTimer = null
  }
  otaPollCancelled = true
  if (otaPollTimer) {
    clearTimeout(otaPollTimer)
    otaPollTimer = null
  }
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
}

.version-badge {
  margin-left: auto;
  text-align: right;
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
  color: var(--color-text);
  background: transparent;
}

.version-badge .changelog-btn:hover {
  border-color: var(--color-text);
  background-color: var(--color-text);
  color: var(--color-surface);
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

.banner-icon { font-size: 1.5rem; display: flex; }
.banner-content { flex: 1; }
.banner-content p { margin: 0; opacity: 0.9; font-size: 0.9375rem; }

/* Content Grid */
.content-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
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

.upload-icon { font-size: 2.5rem; margin-bottom: var(--spacing-sm); opacity: 0.5; display: flex; }
.upload-text { color: var(--color-text-secondary); font-weight: 500; }

.file-preview {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  width: 100%;
}

.file-icon { font-size: 2rem; display: flex; }
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

/* Update Info */
.update-info {
  margin-bottom: var(--spacing-md);
}

.update-available {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
  padding: var(--spacing-md);
  background: linear-gradient(135deg, rgba(16, 185, 129, 0.1) 0%, rgba(5, 150, 105, 0.1) 100%);
  border: 1px solid rgba(16, 185, 129, 0.3);
  border-radius: var(--radius-lg);
}

.update-icon {
  font-size: 1.5rem;
  animation: bounce 2s infinite;
  display: flex;
}

.chip-btn.static {
  cursor: default;
}

.update-text {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.update-text strong {
  color: #059669;
  font-size: 0.9375rem;
}

.version-info {
  font-size: 0.8125rem;
  color: var(--color-text-secondary);
}

/* Beta badge shown next to a pre-release version */
.beta-badge {
  display: inline-block;
  margin-left: 6px;
  padding: 1px 8px;
  font-size: 0.6875rem;
  font-weight: 700;
  letter-spacing: 0.04em;
  text-transform: uppercase;
  color: #92400e;
  background: #fde68a;
  border-radius: var(--radius-full);
  vertical-align: middle;
}

/* Beta channel toggle row */
.beta-toggle-row {
  display: flex;
  align-items: flex-start;
  gap: 12px;
  margin: var(--spacing-md) 0;
  padding: var(--spacing-sm) var(--spacing-md);
  background: var(--color-bg);
  border-radius: var(--radius-md);
}

.beta-toggle-row .form-switch {
  margin: 0;
  flex-shrink: 0;
}

.beta-toggle-row .form-check-input {
  cursor: pointer;
}

.beta-toggle-label {
  display: flex;
  flex-direction: column;
  gap: 2px;
  font-size: 0.875rem;
  font-weight: 600;
  color: var(--color-text);
  cursor: pointer;
}

.beta-toggle-hint {
  font-size: 0.75rem;
  font-weight: 400;
  color: var(--color-text-secondary);
  line-height: 1.3;
}

/* Release notes preview (collapsible <details>) */
.release-notes-preview {
  margin: var(--spacing-sm) 0 0;
  padding: var(--spacing-sm) var(--spacing-md);
  background: rgba(255, 255, 255, 0.55);
  border-radius: var(--radius-md);
  font-size: 0.8125rem;
  line-height: 1.5;
}

.release-notes-preview > summary {
  cursor: pointer;
  font-weight: 600;
  color: var(--color-text);
  list-style: none;
}

.release-notes-preview > summary::-webkit-details-marker {
  display: none;
}

.release-notes-preview > summary::before {
  content: '▸';
  display: inline-block;
  margin-right: 6px;
  transition: transform 0.15s ease;
}

.release-notes-preview[open] > summary::before {
  transform: rotate(90deg);
}

.release-notes-body {
  margin-top: var(--spacing-sm);
  max-height: 240px;
  overflow-y: auto;
  color: var(--color-text);
}

.release-notes-body :deep(h1),
.release-notes-body :deep(h2),
.release-notes-body :deep(h3) {
  font-size: 0.9rem;
  margin: 0.5rem 0 0.25rem;
}

.release-notes-body :deep(ul),
.release-notes-body :deep(ol) {
  margin: 0.25rem 0 0.5rem;
  padding-left: 1.25rem;
}

.release-notes-body :deep(li) {
  margin-bottom: 0.25rem;
}

.release-notes-body :deep(code) {
  background: var(--color-border-light);
  padding: 0.1rem 0.3rem;
  border-radius: 3px;
  font-size: 0.85em;
}

.release-link {
  display: inline-block;
  margin-top: 0.25rem;
  font-size: 0.75rem;
  font-weight: 600;
  color: var(--color-primary);
  text-decoration: none;
}

.release-link:hover {
  text-decoration: underline;
}

.external-link-icon {
  font-size: 0.95rem;
  vertical-align: -0.15em;
  margin-left: 0.2rem;
}

/* Inside the update banner the preview sits on a tinted background */
.alert-banner .release-notes-preview {
  background: rgba(255, 255, 255, 0.18);
  color: white;
}

.alert-banner .release-notes-preview > summary,
.alert-banner .release-notes-body {
  color: white;
}

.no-update {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
  padding: var(--spacing-sm) var(--spacing-md);
  background: var(--color-bg);
  border-radius: var(--radius-md);
  color: var(--color-text-secondary);
  font-size: 0.875rem;
}

.check-icon {
  font-size: 1.125rem;
  display: flex;
}

.check-btn {
  margin-top: var(--spacing-sm);
  padding: 6px 14px;
  font-size: 0.8125rem;
  font-weight: 500;
  color: var(--color-text-secondary);
  background: var(--color-bg);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-md);
  cursor: pointer;
  transition: all 0.2s;
  display: inline-flex;
  align-items: center;
  gap: 6px;
}

.check-btn:hover:not(:disabled) {
  background: var(--color-border-light);
  color: var(--color-text);
}

.check-btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.last-check {
  margin-top: 4px;
  font-size: 0.75rem;
  color: var(--color-text-secondary);
  opacity: 0.7;
}

@keyframes bounce {
  0%, 100% { transform: translateY(0); }
  50% { transform: translateY(-3px); }
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

.tile-icon { font-size: 2rem; display: flex; }
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
  color: white;
  display: inline-flex;
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

  .quick-actions {
    display: flex;
    flex-direction: column;
    align-items: stretch;
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
