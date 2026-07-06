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
      </div>
    </div>

    <div class="quick-actions">
      <button class="chip-btn" type="button" @click="manualCheckForUpdate" :disabled="updateStore.isChecking">
        <AppIcon name="refresh" />
        {{ updateStore.isChecking ? t('firmware.checking') : t('firmware.checkNow') }}
      </button>
      <span class="chip-btn static">
        <AppIcon name="clock" />
        {{ updateStore.lastCheck ? t('firmware.lastCheckAt', { time: formatLastCheck(updateStore.lastCheck) }) : t('firmware.noRecentCheck') }}
      </span>
    </div>

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
      <div class="update-card network-update-card">
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
            <button class="changelog-link" type="button" @click="showChangelogModal = true">
              <AppIcon name="logs" />
              {{ t('changelog.title') }}
              <AppIcon name="arrowRight" />
            </button>
          </div>
          <div v-else-if="updateStore.checkError" class="update-info">
            <div class="no-update">
              <span class="check-icon"><AppIcon name="alert" /></span>
              <span>{{ t('firmware.checkFailed') }}: {{ updateStore.checkError }}</span>
            </div>
            <button class="check-btn" @click="manualCheckForUpdate" :disabled="updateStore.isChecking">
              <span v-if="updateStore.isChecking" class="spinner-border spinner-border-sm"></span>
              {{ updateStore.isChecking ? t('firmware.checking') : t('firmware.retry') }}
            </button>
            <div v-if="updateStore.lastCheck" class="last-check">
              {{ t('firmware.lastCheck') }}: {{ formatLastCheck(updateStore.lastCheck) }}
            </div>
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

    <!-- Firmware Archive -->
    <div class="update-card archive-card">
      <div class="card-header">
        <div class="header-icon bg-warning-light text-warning"><AppIcon name="download" /></div>
        <div class="header-text">
          <h3>{{ t('firmware.archiveTitle') }}</h3>
          <p>{{ t('firmware.archiveHint') }}</p>
        </div>
      </div>

      <div class="card-body">
        <div class="archive-toolbar">
          <div class="archive-filters" role="group" :aria-label="t('firmware.archiveFilter')">
            <button
              v-for="filter in archiveFilters"
              :key="filter.value"
              type="button"
              :class="['filter-btn', { active: archiveFilter === filter.value }]"
              @click="setArchiveFilter(filter.value)"
            >
              {{ filter.label }}
            </button>
          </div>
          <button class="check-btn archive-refresh" type="button" @click="loadFirmwareArchive" :disabled="archiveLoading">
            <span v-if="archiveLoading" class="spinner-border spinner-border-sm"></span>
            <AppIcon v-else name="refresh" />
            {{ archiveLoading ? t('firmware.archiveLoading') : t('firmware.archiveRefresh') }}
          </button>
        </div>

        <div class="archive-warning">
          <AppIcon name="alert" />
          <span>{{ t('firmware.archiveWarning') }}</span>
        </div>

        <div v-if="archiveError" class="archive-error">
          <AppIcon name="alert" />
          <span>{{ archiveError }}</span>
        </div>

        <div v-if="archiveLoading && firmwareArchive.length === 0" class="archive-empty">
          <span class="spinner-border spinner-border-sm"></span>
          {{ t('firmware.archiveLoading') }}
        </div>

        <div v-else-if="filteredFirmwareArchive.length === 0" class="archive-empty">
          <AppIcon name="info" />
          {{ t('firmware.archiveEmpty') }}
        </div>

        <div v-else class="archive-picker">
          <label class="archive-select-label" for="firmwareArchiveSelect">{{ t('firmware.archiveSelect') }}</label>
          <select id="firmwareArchiveSelect" v-model="selectedArchiveVersion" class="archive-select">
            <option
              v-for="release in filteredFirmwareArchive"
              :key="release.id"
              :value="release.version"
            >
              {{ archiveOptionLabel(release) }}
            </option>
          </select>

          <div v-if="selectedArchiveRelease" class="archive-selected">
            <div class="archive-main-row">
              <div class="archive-version">
                <div class="archive-title-row">
                  <strong>v{{ selectedArchiveRelease.version }}</strong>
                  <span v-if="selectedArchiveRelease.prerelease" class="beta-badge">{{ t('firmware.beta') }}</span>
                  <span v-if="selectedArchiveRelease.isCurrent" class="current-badge">{{ t('firmware.archiveCurrent') }}</span>
                </div>
                <div class="archive-meta">
                  <span>{{ formatReleaseDate(selectedArchiveRelease.publishedAt) }}</span>
                  <span v-if="selectedArchiveRelease.assetName">· {{ selectedArchiveRelease.assetName }}</span>
                  <span v-if="selectedArchiveRelease.assetSize">· {{ formatSize(selectedArchiveRelease.assetSize) }}</span>
                </div>
              </div>
              <div class="archive-actions">
                <a class="archive-link" :href="selectedArchiveRelease.releaseUrl" target="_blank" rel="noopener noreferrer">
                  <AppIcon name="externalLink" />
                  {{ t('firmware.viewOnGithub') }}
                </a>
                <button
                  class="archive-install-btn"
                  type="button"
                  :disabled="otaUpdating || !selectedArchiveRelease.downloadUrl || selectedArchiveRelease.isCurrent"
                  @click="installArchivedFirmware(selectedArchiveRelease)"
                >
                  <span v-if="archiveInstallingVersion === selectedArchiveRelease.version" class="spinner-border spinner-border-sm"></span>
                  <AppIcon v-else name="download" />
                  {{ selectedArchiveRelease.isCurrent ? t('firmware.archiveInstalled') : t('firmware.archiveInstall') }}
                </button>
              </div>
            </div>
            <details v-if="selectedArchiveRelease.notes" class="archive-notes" open>
              <summary>
                <AppIcon name="logs" />
                {{ t('firmware.archiveReleaseNotes') }}
              </summary>
              <pre>{{ selectedArchiveRelease.notes }}</pre>
            </details>
          </div>
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
import { computed, ref, onMounted, onUnmounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSysInfoStore, useUpdateStore, useFirmwareUpdateStore, useUiStore } from './stores.js'
import axios from 'axios'
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
const showCountdown = ref(false)
const countdown = ref(30)
const showChangelogModal = ref(false)
const betaToggleSaving = ref(false)
const archiveFilter = ref('stable')
const archiveLoading = ref(false)
const archiveError = ref('')
const firmwareArchive = ref([])
const archiveInstallingVersion = ref('')
const selectedArchiveVersion = ref('')

const ARCHIVE_MANIFEST_URL = '/api/firmware_archive'
const ARCHIVE_MANIFEST_FALLBACK_URL = 'https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/main/archive.json'

const archiveFilters = computed(() => [
  { value: 'stable', label: t('firmware.archiveStable') },
  { value: 'beta', label: t('firmware.archiveBeta') },
  { value: 'all', label: t('firmware.archiveAll') }
])

const normalizeVersion = (version) => String(version || '').replace(/^v/i, '')

const filteredFirmwareArchive = computed(() => {
  if (archiveFilter.value === 'stable') {
    return firmwareArchive.value.filter((release) => !release.prerelease)
  }
  if (archiveFilter.value === 'beta') {
    return firmwareArchive.value.filter((release) => release.prerelease)
  }
  return firmwareArchive.value
})

const prefersBetaArchive = () => (
  updateStore.betaChannel
  || /(?:beta|alpha|rc)/i.test(sysInfoStore.currentVersion || '')
)

const selectedArchiveRelease = computed(() => (
  filteredFirmwareArchive.value.find((release) => release.version === selectedArchiveVersion.value)
  || filteredFirmwareArchive.value[0]
  || null
))

const selectDefaultArchiveRelease = () => {
  const releases = filteredFirmwareArchive.value
  if (releases.length === 0) {
    selectedArchiveVersion.value = ''
    return
  }
  const currentSelectionStillVisible = releases.some((release) => release.version === selectedArchiveVersion.value)
  if (!currentSelectionStillVisible) {
    selectedArchiveVersion.value = (releases.find((release) => !release.isCurrent) || releases[0]).version
  }
}

const setArchiveFilter = (filter) => {
  archiveFilter.value = filter
  selectDefaultArchiveRelease()
}

const syncArchiveFilterWithUpdateChannel = () => {
  archiveFilter.value = prefersBetaArchive() ? 'beta' : 'stable'
  selectDefaultArchiveRelease()
}

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

const formatReleaseDate = (dateStr) => {
  if (!dateStr) return ''
  return new Date(dateStr).toLocaleDateString()
}

const archiveOptionLabel = (release) => {
  const channel = release.prerelease ? t('firmware.archiveBeta') : t('firmware.archiveStable')
  const current = release.isCurrent ? ` - ${t('firmware.archiveCurrent')}` : ''
  const date = formatReleaseDate(release.publishedAt)
  return `v${release.version} - ${channel}${date ? ` - ${date}` : ''}${current}`
}

const normalizeArchiveEntry = (entry, index = 0) => {
  const version = normalizeVersion(entry.version || entry.tagName || entry.tag_name || entry.name)
  if (!version) return null

  const tagName = entry.tagName || entry.tag_name || `v${version}`
  const prerelease = entry.prerelease ?? entry.isPrerelease ?? /(?:beta|alpha|rc)/i.test(version)
  const downloadUrl = entry.downloadUrl || entry.download_url || ''
  const currentVersion = normalizeVersion(sysInfoStore.currentVersion)

  return {
    id: entry.id || tagName || `${version}-${index}`,
    version,
    name: entry.name || `HB-RF-ETH-ng ${tagName}`,
    prerelease: !!prerelease,
    publishedAt: entry.publishedAt || entry.published_at || '',
    releaseUrl: entry.releaseUrl || entry.html_url || `https://github.com/Xerolux/HB-RF-ETH-ng/releases/tag/${tagName}`,
    downloadUrl,
    assetName: entry.assetName || entry.asset_name || (downloadUrl ? downloadUrl.split('/').pop() : ''),
    assetSize: entry.assetSize || entry.asset_size || entry.size || 0,
    notes: entry.notes || entry.body || '',
    isCurrent: currentVersion && normalizeVersion(version) === currentVersion
  }
}

const parseArchiveManifest = (data) => {
  const entries = Array.isArray(data) ? data : data?.releases
  if (!Array.isArray(entries)) {
    throw new Error(t('firmware.archiveLoadError'))
  }

  return entries
    .map(normalizeArchiveEntry)
    .filter((release) => release?.version && release.downloadUrl)
}

const fetchArchiveManifest = (url) => axios.get(url, {
  headers: { Accept: 'application/json' },
  params: { _: Date.now() },
  timeout: 15000,
  silent: true
})

const loadArchiveManifest = async () => {
  try {
    const response = await fetchArchiveManifest(ARCHIVE_MANIFEST_URL)
    return parseArchiveManifest(response.data)
  } catch (primaryError) {
    // Some firmware builds run the GitHub proxy with very little free heap.
    // Fall back to the static raw manifest directly from the browser so the
    // archive remains usable even when the device-side proxy cannot fetch it.
    const response = await fetchArchiveManifest(ARCHIVE_MANIFEST_FALLBACK_URL)
    return parseArchiveManifest(response.data)
  }
}

const loadFirmwareArchive = async () => {
  archiveLoading.value = true
  archiveError.value = ''

  try {
    firmwareArchive.value = await loadArchiveManifest()
    selectDefaultArchiveRelease()
  } catch (error) {
    const status = error.response?.status
    const serverMessage = typeof error.response?.data === 'string' ? error.response.data : ''
    archiveError.value = status
      ? `${t('firmware.archiveLoadError')} (${status}${serverMessage ? `: ${serverMessage}` : ''})`
      : error.message || t('firmware.archiveLoadError')
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: archiveError.value })
  } finally {
    archiveLoading.value = false
  }
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

  await startOtaFromUrl(updateUrl, version)
}

const installArchivedFirmware = async (release) => {
  if (!release?.downloadUrl || release.isCurrent) return

  const confirmed = window.confirm(t('firmware.archiveInstallConfirm', { version: release.version }))
  if (!confirmed) return

  archiveInstallingVersion.value = release.version
  try {
    await startOtaFromUrl(release.downloadUrl, release.version)
  } finally {
    archiveInstallingVersion.value = ''
  }
}

const startOtaFromUrl = async (updateUrl, version) => {
  otaUpdating.value = true
  otaProgress.value = 0

  // Remember target version so app.vue can show a success modal after reboot
  localStorage.setItem('otaUpdateVersion', version)

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
      localStorage.removeItem('otaUpdateVersion')
      uiStore.pushToast({ type: 'error', title: t('common.error'), message: response.data.error || t('firmware.otaFailed') })
    }
  } catch (error) {
    localStorage.removeItem('otaUpdateVersion')
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: error.response?.data?.error || error.message || t('firmware.otaFailed') })
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
          reject(new Error(otaError || t('firmware.otaFailed')))
        } else {
          resolve()
        }
      } catch (err) {
        consecutiveErrors++
        if (consecutiveErrors >= 10) {
          reject(new Error(t('firmware.lostConnection')))
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
        window.location.href = '/'
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
  // Load system info and read the cached update snapshot independently: a
  // failure fetching system info must not stop us from showing the device's
  // cached firmware status (and vice versa).
  try {
    await sysInfoStore.update()
  } catch (e) {
    console.warn('Failed to load system info:', e.response?.status || e.message)
  }

  try {
    // Only read the device's cached snapshot - opening the Firmware page must
    // never trigger a live manifest fetch. The device runs its own 24h
    // background check, and the user can force a check on demand with the
    // "Check now" button. This keeps every page visit from consuming a
    // manifest request. If the cache is still empty (e.g. fresh
    // boot before the background task ran), the page shows "n/a" until the
    // background task populates it, which the cached poll below picks up.
    await updateStore.checkForUpdate(sysInfoStore.currentVersion, { cached: true })
  } catch (e) {
    console.warn('Initial cached update read failed:', e.response?.status || e.message)
  }
  syncArchiveFilterWithUpdateChannel()
  loadFirmwareArchive()
  // Periodically re-read the cache while the page is open so the
  // "last check" indicator and download URL stay fresh (cached read only,
  // no GitHub call).
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
/* Hide the native file input: it sits inside the clickable upload-zone, so a
 * visible input would open the file dialog twice (native click + the zone's
 * programmatic click). Display:none keeps it functional but non-interactive. */
.hidden-input {
  display: none;
}

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

.changelog-link {
  width: 100%;
  margin-top: var(--spacing-sm);
  padding: 12px 14px;
  border: 1px solid var(--color-border-light);
  border-radius: var(--radius-md);
  background: var(--color-surface);
  color: var(--color-text);
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 0.875rem;
  font-weight: 700;
  text-align: left;
  transition: border-color 0.2s, background 0.2s;
}

.changelog-link > :last-child {
  margin-left: auto;
}

.changelog-link:hover {
  border-color: var(--color-success);
  background: var(--color-success-light);
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

/* Firmware Archive */
.archive-card {
  margin-bottom: var(--spacing-xl);
}

.archive-toolbar,
.archive-main-row,
.archive-actions {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
}

.archive-toolbar {
  justify-content: space-between;
  flex-wrap: wrap;
  margin-bottom: var(--spacing-md);
}

.archive-filters {
  display: inline-flex;
  padding: 4px;
  border-radius: var(--radius-full);
  background: var(--color-bg);
  border: 1px solid var(--color-border-light);
}

.filter-btn {
  border: none;
  background: transparent;
  color: var(--color-text-secondary);
  border-radius: var(--radius-full);
  padding: 6px 12px;
  font-size: 0.8125rem;
  font-weight: 700;
  cursor: pointer;
}

.filter-btn.active {
  background: var(--color-surface);
  color: var(--color-primary);
  box-shadow: var(--shadow-sm);
}

.archive-refresh {
  margin-top: 0;
}

.archive-warning,
.archive-error,
.archive-empty {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
  padding: var(--spacing-sm) var(--spacing-md);
  border-radius: var(--radius-md);
  font-size: 0.875rem;
}

.archive-warning {
  margin-bottom: var(--spacing-md);
  color: var(--color-warning);
  background: var(--color-warning-light);
}

.archive-error {
  margin-bottom: var(--spacing-md);
  color: var(--color-danger);
  background: var(--color-danger-light);
}

.archive-empty {
  color: var(--color-text-secondary);
  background: var(--color-bg);
}

.archive-picker {
  display: grid;
  gap: var(--spacing-sm);
}

.archive-select-label {
  font-size: 0.8125rem;
  font-weight: 800;
  color: var(--color-text-secondary);
}

.archive-select {
  width: 100%;
  min-height: 44px;
  border: 1px solid var(--color-border);
  border-radius: var(--radius-lg);
  background: var(--color-surface);
  color: var(--color-text);
  padding: 9px 12px;
  font-size: 0.9375rem;
  font-weight: 700;
}

.archive-select:focus {
  border-color: var(--color-primary);
  outline: 3px solid var(--color-primary-light);
}

.archive-selected {
  padding: var(--spacing-md);
  border: 1px solid var(--color-border-light);
  border-radius: var(--radius-lg);
  background: var(--color-bg);
}

.archive-main-row {
  justify-content: space-between;
  align-items: flex-start;
}

.archive-version {
  min-width: 0;
}

.archive-title-row {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
}

.archive-meta {
  margin-top: 4px;
  color: var(--color-text-secondary);
  font-size: 0.8125rem;
  overflow-wrap: anywhere;
}

.current-badge {
  display: inline-flex;
  align-items: center;
  padding: 1px 8px;
  border-radius: var(--radius-full);
  color: var(--color-success);
  background: var(--color-success-light);
  font-size: 0.6875rem;
  font-weight: 800;
  text-transform: uppercase;
}

.archive-actions {
  flex-wrap: wrap;
  justify-content: flex-end;
}

.archive-link,
.archive-install-btn {
  border: 1px solid var(--color-border);
  border-radius: var(--radius-md);
  padding: 7px 10px;
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 0.8125rem;
  font-weight: 700;
  text-decoration: none;
}

.archive-link {
  color: var(--color-text-secondary);
  background: var(--color-surface);
}

.archive-install-btn {
  color: #fff;
  background: var(--color-primary);
  border-color: var(--color-primary);
  cursor: pointer;
}

.archive-install-btn:disabled {
  opacity: 0.55;
  cursor: not-allowed;
}

.archive-notes {
  margin-top: var(--spacing-sm);
  border-top: 1px solid var(--color-border-light);
  padding-top: var(--spacing-sm);
}

.archive-notes summary {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  cursor: pointer;
  color: var(--color-text-secondary);
  font-size: 0.8125rem;
  font-weight: 800;
}

.archive-notes pre {
  margin: var(--spacing-sm) 0 0;
  padding: var(--spacing-sm);
  max-height: 220px;
  overflow: auto;
  white-space: pre-wrap;
  word-break: break-word;
  border-radius: var(--radius-md);
  color: var(--color-text);
  background: var(--color-surface);
  border: 1px solid var(--color-border-light);
  font-family: inherit;
  font-size: 0.8125rem;
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
