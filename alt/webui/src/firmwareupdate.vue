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

      <BFormGroup :label="t('firmware.updateFile')" label-cols-sm="4">
        <BFormFile
          v-model="file"
          accept=".bin"
          :placeholder="t('firmware.noFileChosen')"
          :browse-text="t('firmware.browse')"
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
          {{ t('firmware.restart') }}
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
const restartLoading = ref(false)

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
    }
  }
}

onMounted(() => {
  sysInfoStore.update()
})
</script>
