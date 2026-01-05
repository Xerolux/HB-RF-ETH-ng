<template>
  <BCard
    :header="t('firmware.title')"
    header-tag="h6"
    header-bg-variant="secondary"
    header-text-variant="white"
    class="mb-3"
  >
    <BForm @submit.stop.prevent>
      <BFormGroup :label="t('firmware.installedVersion')" label-cols-sm="4">
        <BFormInput type="text" :model-value="sysInfoStore.currentVersion" disabled />
      </BFormGroup>
      <BFormGroup :label="t('firmware.latestVersion')" label-cols-sm="4">
        <BFormInput type="text" :model-value="sysInfoStore.latestVersion" disabled />
      </BFormGroup>
      <BAlert
        variant="info"
        :model-value="true"
        class="mb-3"
      >
        {{ t('firmware.versionInfo') }}
        <a
          href="https://github.com/Xerolux/HB-RF-ETH-ng/releases"
          class="alert-link"
          target="_new"
        >GitHub Releases</a>
      </BAlert>
      <BAlert
        variant="success"
        :model-value="sysInfoStore.currentVersion >= sysInfoStore.latestVersion && sysInfoStore.latestVersion != 'n/a'"
      >
        {{ t('firmware.upToDate') }}
      </BAlert>
      <BAlert
        variant="warning"
        :model-value="sysInfoStore.currentVersion < sysInfoStore.latestVersion && sysInfoStore.latestVersion != 'n/a'"
      >
        {{ t('firmware.updateAvailable', { currentVersion: sysInfoStore.currentVersion, latestVersion: sysInfoStore.latestVersion }) }}
      </BAlert>
      <BAlert
        variant="danger"
        :model-value="sysInfoStore.latestVersion == 'n/a'"
      >
        {{ t('firmware.versionCheckFailed') }}
      </BAlert>

      <BFormGroup
        v-if="sysInfoStore.currentVersion < sysInfoStore.latestVersion && sysInfoStore.latestVersion != 'n/a'"
        label-cols-sm="9"
        class="mb-3"
      >
        <BButton
          variant="info"
          block
          class="mb-2"
          @click="showReleaseNotes = true"
        >{{ t('firmware.showReleaseNotes') }}</BButton>
        <BButton
          variant="success"
          block
          :disabled="firmwareUpdateStore.progress > 0"
          @click="onlineUpdateClick"
        >
          <BSpinner small v-if="firmwareUpdateStore.progress > 0" class="me-2" />
          {{ firmwareUpdateStore.progress > 0 ? t('firmware.updating') : t('firmware.onlineUpdate') }}
        </BButton>
      </BFormGroup>

      <BFormGroup
        v-if="sysInfoStore.currentVersion >= sysInfoStore.latestVersion || sysInfoStore.latestVersion == 'n/a'"
        label-cols-sm="9"
        class="mb-3"
      >
        <BButton
            variant="primary"
            block
            @click="checkUpdateClick"
            :disabled="checkUpdateLoading"
        >
          <BSpinner small v-if="checkUpdateLoading" class="me-2" />
          {{ checkUpdateLoading ? t('common.loading') : t('firmware.checkUpdate') }}
        </BButton>
      </BFormGroup>

      <BFormGroup :label="t('firmware.updateFile')" label-cols-sm="4">
        <BFormFile
          v-model="file"
          accept=".bin"
          :placeholder="t('firmware.noFileChosen')"
          :browse-text="t('firmware.browse')"
        />
      </BFormGroup>

    <BModal
      v-model="showReleaseNotes"
      :title="t('firmware.releaseNotesTitle', { version: sysInfoStore.latestVersion })"
      size="lg"
      scrollable
      ok-only
      :ok-title="t('common.close')"
      @show="fetchReleaseNotes"
    >
      <div v-if="loadingNotes" class="text-center p-4">
        <BSpinner label="Loading..." />
      </div>
      <div v-else-if="releaseNotesError" class="text-danger">
        {{ t('firmware.releaseNotesError') }}
      </div>
      <div v-else>
        <!-- Use v-html carefully, ideally use a markdown renderer but for now pre-wrap -->
        <pre class="release-notes-content">{{ releaseNotes }}</pre>
        <BButton
          v-if="releaseDownloadUrl"
          :href="releaseDownloadUrl"
          target="_blank"
          variant="primary"
          block
          class="mt-2"
        >{{ t('firmware.downloadFirmware') }}</BButton>
        <BButton
          variant="success"
          block
          class="mt-3"
          :disabled="firmwareUpdateStore.progress > 0"
          @click="onlineUpdateClickFromModal"
        >
          <BSpinner small v-if="firmwareUpdateStore.progress > 0" class="me-2" />
          {{ firmwareUpdateStore.progress > 0 ? t('firmware.updating') : t('firmware.onlineUpdate') }}
        </BButton>
      </div>
    </BModal>
      <BProgress
        :value="firmwareUpdateStore.progress"
        :max="100"
        class="mb-3"
        v-if="firmwareUpdateStore.progress > 0"
        animated
      />
      <BAlert
        variant="success"
        :model-value="showSuccess"
        dismissible
        fade
        @update:model-value="showSuccess = null"
      >{{ t("firmware.uploadSuccess") }}</BAlert>
      <BAlert
        variant="danger"
        :model-value="showError"
        dismissible
        fade
        @update:model-value="showError = null"
      >{{ t("firmware.uploadError") }}</BAlert>
      <BFormGroup label-cols-sm="9">
        <BButton
          variant="primary"
          block
          :disabled="file == null || firmwareUpdateStore.progress > 0"
          @click="firmwareUpdateClick"
        >
          <BSpinner small v-if="firmwareUpdateStore.progress > 0" class="me-2" />
          {{ firmwareUpdateStore.progress > 0 ? t('firmware.uploading') : t('firmware.upload') }}
        </BButton>
      </BFormGroup>
      <BFormGroup label-cols-sm="9">
        <BButton
          variant="warning"
          block
          :disabled="firmwareUpdateStore.progress > 0 || restartLoading"
          @click="restartClick"
        >
          <BSpinner small v-if="restartLoading" class="me-2" />
          <svg v-else aria-hidden="true" xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-arrow-counterclockwise me-2" viewBox="0 0 16 16">
            <path fill-rule="evenodd" d="M8 3a5 5 0 1 1-4.546 2.914.5.5 0 0 0-.908-.417A6 6 0 1 0 8 2v1z"/>
            <path d="M8 4.466V.534a.25.25 0 0 0-.41-.192L5.23 2.308a.25.25 0 0 0 0 .384l2.36 1.966A.25.25 0 0 0 8 4.466z"/>
          </svg>
          {{ restartLoading ? t('common.rebootingWait') : t('firmware.restart') }}
        </BButton>
      </BFormGroup>
    </BForm>
  </BCard>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSysInfoStore, useFirmwareUpdateStore } from './stores.js'
import axios from 'axios'

const { t } = useI18n()

const sysInfoStore = useSysInfoStore()
const firmwareUpdateStore = useFirmwareUpdateStore()

const file = ref(null)
const showError = ref(false)
const showSuccess = ref(false)
const checkUpdateLoading = ref(false)
const restartLoading = ref(false)

const showReleaseNotes = ref(false)
const releaseNotes = ref('')
const releaseDownloadUrl = ref('')
const loadingNotes = ref(false)
const releaseNotesError = ref(false)

const fetchReleaseNotes = async () => {
  loadingNotes.value = true
  releaseNotesError.value = false
  releaseNotes.value = ''
  releaseDownloadUrl.value = ''
  try {
    const response = await axios.post('/api/check_update')
    if (response.data) {
      sysInfoStore.latestVersion = response.data.latestVersion || sysInfoStore.latestVersion
      if (response.data.releaseNotes) {
        releaseNotes.value = response.data.releaseNotes
        sysInfoStore.releaseNotes = response.data.releaseNotes
      } else {
        releaseNotes.value = t('firmware.releaseNotesError')
      }
      if (response.data.downloadUrl) {
        releaseDownloadUrl.value = response.data.downloadUrl
        sysInfoStore.downloadUrl = response.data.downloadUrl
      }
    }
  } catch (error) {
    console.error('Failed to fetch release notes:', error)
    releaseNotesError.value = true
  } finally {
    loadingNotes.value = false
  }
}

const onlineUpdateClickFromModal = () => {
    showReleaseNotes.value = false
    onlineUpdateClick()
}

const onlineUpdateClick = async () => {
  if (confirm(t('firmware.onlineUpdateConfirm'))) {
    showError.value = null
    showSuccess.value = null

    // Set a fake progress to show activity or use a different indicator
    firmwareUpdateStore.progress = 1

    try {
        const response = await fetch('/api/online_update', { method: 'POST' })
        if (response.ok) {
            // The device will restart, so maybe show a message "Update started, device will restart..."
            alert(t('firmware.onlineUpdateStarted'))
        } else {
            showError.value = true
        }
        firmwareUpdateStore.progress = 0
    } catch (error) {
        showError.value = true
        firmwareUpdateStore.progress = 0
    }
  }
}

const firmwareUpdateClick = async () => {
  showError.value = null
  showSuccess.value = null

  try {
    await firmwareUpdateStore.update(file.value)
    showSuccess.value = true
    file.value = null
  } catch (error) {
    showError.value = true
  }
}

const restartClick = async () => {
  if (confirm(t('firmware.restartConfirm'))) {
    restartLoading.value = true
    try {
      await fetch('/api/restart', { method: 'POST' })
      alert(t('common.rebootingWait'))
      setTimeout(() => {
          window.location.reload()
      }, 10000)
    } catch (error) {
      // Expected - device will restart and connection will be lost
    } finally {
      // restartLoading.value = false // No need to reset if page reloads, but good practice if it fails
    }
  }
}

const checkUpdateClick = async () => {
    checkUpdateLoading.value = true
    try {
        const response = await axios.post('/api/check_update')
        if (response.data && response.data.latestVersion) {
            sysInfoStore.latestVersion = response.data.latestVersion
            sysInfoStore.releaseNotes = response.data.releaseNotes || sysInfoStore.releaseNotes
            sysInfoStore.downloadUrl = response.data.downloadUrl || sysInfoStore.downloadUrl
            if (sysInfoStore.currentVersion < sysInfoStore.latestVersion) {
                releaseNotes.value = response.data.releaseNotes || ''
                releaseDownloadUrl.value = response.data.downloadUrl || ''
                alert(t('firmware.updateAvailable', { latestVersion: sysInfoStore.latestVersion }))
            } else {
                alert(t('firmware.noUpdateAvailable'))
            }
        }
    } catch (e) {
        console.error(e)
        alert('Failed to check updates')
    } finally {
        checkUpdateLoading.value = false
    }
}

onMounted(() => {
  sysInfoStore.update()
})
</script>

<style scoped>
.release-notes-content {
    white-space: pre-wrap;
    word-break: break-word;
    font-family: inherit;
    background: #f8f9fa;
    padding: 1rem;
    border-radius: 0.25rem;
}
</style>
