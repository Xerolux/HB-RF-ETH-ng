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
        >{{ t('firmware.onlineUpdate') }}</BButton>
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
        >{{ t('firmware.checkUpdate') }}</BButton>
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
          variant="success"
          block
          class="mt-3"
          :disabled="firmwareUpdateStore.progress > 0"
          @click="onlineUpdateClickFromModal"
        >{{ t('firmware.onlineUpdate') }}</BButton>
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
        >{{ t('firmware.upload') }}</BButton>
      </BFormGroup>
      <BFormGroup label-cols-sm="9">
        <BButton
          variant="warning"
          block
          :disabled="firmwareUpdateStore.progress > 0"
          @click="restartClick"
        >{{ t('firmware.restart') }}</BButton>
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

const showReleaseNotes = ref(false)
const releaseNotes = ref('')
const loadingNotes = ref(false)
const releaseNotesError = ref(false)

const fetchReleaseNotes = async () => {
  loadingNotes.value = true
  releaseNotesError.value = false
  releaseNotes.value = ''
  try {
    // GitHub API to get release notes
    // Note: This relies on client-side internet access
    const response = await axios.get(`https://api.github.com/repos/Xerolux/HB-RF-ETH-ng/releases/tags/v${sysInfoStore.latestVersion}`)
    releaseNotes.value = response.data.body
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
    try {
      await fetch('/api/restart', { method: 'POST' })
      alert(t('common.rebootingWait'))
      setTimeout(() => {
          window.location.reload()
      }, 10000)
    } catch (error) {
      // Expected - device will restart and connection will be lost
    }
  }
}

const checkUpdateClick = async () => {
    try {
        const response = await axios.post('/api/check_update')
        if (response.data && response.data.latestVersion) {
            sysInfoStore.latestVersion = response.data.latestVersion
            if (sysInfoStore.currentVersion < sysInfoStore.latestVersion) {
                alert(t('firmware.updateAvailable', { latestVersion: sysInfoStore.latestVersion }))
            } else {
                alert(t('firmware.noUpdateAvailable'))
            }
        }
    } catch (e) {
        console.error(e)
        alert('Failed to check updates')
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
