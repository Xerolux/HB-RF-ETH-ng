<template>
  <BCard
    :header="t('title')"
    header-tag="h6"
    header-bg-variant="secondary"
    header-text-variant="white"
    class="mb-3"
  >
    <BForm @submit.stop.prevent>
      <BFormGroup :label="t('installedVersion')" label-cols-sm="4">
        <BFormInput type="text" :model-value="sysInfoStore.currentVersion" disabled />
      </BFormGroup>
      <BAlert
        variant="info"
        :model-value="true"
        class="mb-3"
      >
        {{ t('versionInfo') }}
        <a
          href="https://github.com/Xerolux/HB-RF-ETH-ng"
          class="alert-link"
          target="_new"
        >GitHub (Fork v2.1)</a>
      </BAlert>
      <BAlert
        variant="warning"
        :model-value="sysInfoStore.currentVersion < sysInfoStore.latestVersion && sysInfoStore.latestVersion != 'n/a'"
      >
        {{ t('updateAvailable', { latestVersion: sysInfoStore.latestVersion }) }}
      </BAlert>
      <BFormGroup :label="t('updateFile')" label-cols-sm="4">
        <BFormFile
          v-model="file"
          accept=".bin"
          :placeholder="t('noFileChosen')"
          :browse-text="t('browse')"
        />
      </BFormGroup>
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
      >{{ t("uploadSuccess") }}</BAlert>
      <BAlert
        variant="danger"
        :model-value="showError"
        dismissible
        fade
        @update:model-value="showError = null"
      >{{ t("uploadError") }}</BAlert>
      <BFormGroup label-cols-sm="9">
        <BButton
          variant="primary"
          block
          :disabled="file == null || firmwareUpdateStore.progress > 0"
          @click="firmwareUpdateClick"
        >{{ t('upload') }}</BButton>
      </BFormGroup>
      <BFormGroup label-cols-sm="9">
        <BButton
          variant="warning"
          block
          :disabled="firmwareUpdateStore.progress > 0"
          @click="restartClick"
        >{{ t('restart') }}</BButton>
      </BFormGroup>
    </BForm>
  </BCard>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSysInfoStore, useFirmwareUpdateStore } from './stores.js'

const { t } = useI18n({
  locale: navigator.language,
  fallbackLocale: 'en',
  messages: {
    de: {
      title: 'Firmware',
      installedVersion: 'Installierte Version',
      versionInfo: 'Modernisierte Fork v2.1 von Xerolux (2025) - Basierend auf der Original-Arbeit von Alexander Reinert.',
      updateAvailable: 'Ein Update auf Version {latestVersion} ist verfügbar.',
      updateFile: 'Firmware Datei',
      noFileChosen: 'Keine Datei ausgewählt',
      browse: 'Datei auswählen',
      upload: 'Hochladen',
      restart: 'System neu starten',
      uploadSuccess: 'Die Firmware wurde erfolgreich hochgeladen. System startet in 3 Sekunden automatisch neu...',
      uploadError: 'Es ist ein Fehler aufgetreten.',
      restartConfirm: 'Möchten Sie das System wirklich neu starten?'
    },
    en: {
      title: 'Firmware',
      installedVersion: 'Installed version',
      versionInfo: 'Modernized fork v2.1 by Xerolux (2025) - Based on the original work by Alexander Reinert.',
      updateAvailable: 'An update to version {latestVersion} is available.',
      updateFile: 'Firmware file',
      noFileChosen: 'No file chosen',
      browse: 'Browse',
      upload: 'Upload',
      restart: 'Restart system',
      uploadSuccess: 'Firmware update successfully uploaded. System will restart automatically in 3 seconds...',
      uploadError: 'An error occured.',
      restartConfirm: 'Do you really want to restart the system?'
    }
  }
})

const sysInfoStore = useSysInfoStore()
const firmwareUpdateStore = useFirmwareUpdateStore()

const file = ref(null)
const showError = ref(false)
const showSuccess = ref(false)

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
  if (confirm(t('restartConfirm'))) {
    try {
      await fetch('/api/restart', { method: 'POST' })
    } catch (error) {
      // Expected - device will restart and connection will be lost
    }
  }
}

onMounted(() => {
  sysInfoStore.update()
})
</script>

<style scoped>
</style>
