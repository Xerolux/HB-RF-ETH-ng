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
        <BFormInput
          type="text"
          :model-value="sysInfoStore.currentVersion"
          disabled
        />
      </BFormGroup>
      <BAlert variant="info" :model-value="true" class="mb-3">
        {{ t("firmware.versionInfo") }}
        <a
          href="https://github.com/Xerolux/HB-RF-ETH-ng"
          class="alert-link"
          target="_new"
          >GitHub (Fork v2.1)</a
        >
      </BAlert>
      <BAlert
        variant="warning"
        :model-value="
          sysInfoStore.currentVersion < sysInfoStore.latestVersion &&
          sysInfoStore.latestVersion != 'n/a'
        "
      >
        {{
          t("firmware.updateAvailable", {
            latestVersion: sysInfoStore.latestVersion,
          })
        }}
      </BAlert>

      <BFormGroup
        v-if="
          sysInfoStore.currentVersion < sysInfoStore.latestVersion &&
          sysInfoStore.latestVersion != 'n/a'
        "
        label-cols-sm="9"
        class="mb-3"
      >
        <BButton
          variant="success"
          block
          :disabled="firmwareUpdateStore.progress > 0"
          @click="onlineUpdateClick"
          >{{ t("firmware.onlineUpdate") }}</BButton
        >
      </BFormGroup>

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
        >{{ t("firmware.uploadSuccess") }}</BAlert
      >
      <BAlert
        variant="danger"
        :model-value="showError"
        dismissible
        fade
        @update:model-value="showError = null"
        >{{ t("firmware.uploadError") }}</BAlert
      >
      <BFormGroup label-cols-sm="9">
        <BButton
          variant="primary"
          block
          :disabled="file == null || firmwareUpdateStore.progress > 0"
          @click="firmwareUpdateClick"
          >{{ t("firmware.upload") }}</BButton
        >
      </BFormGroup>
      <BFormGroup label-cols-sm="9">
        <BButton
          variant="warning"
          block
          :disabled="firmwareUpdateStore.progress > 0"
          @click="restartClick"
          >{{ t("firmware.restart") }}</BButton
        >
      </BFormGroup>
    </BForm>
  </BCard>
</template>

<script setup>
import { ref, onMounted } from "vue";
import { useI18n } from "vue-i18n";
import { useSysInfoStore, useFirmwareUpdateStore } from "./stores.js";

const { t } = useI18n();

const sysInfoStore = useSysInfoStore();
const firmwareUpdateStore = useFirmwareUpdateStore();

const file = ref(null);
const showError = ref(false);
const showSuccess = ref(false);

const onlineUpdateClick = async () => {
  if (confirm(t("firmware.onlineUpdateConfirm"))) {
    showError.value = null;
    showSuccess.value = null;

    // Set a fake progress to show activity or use a different indicator
    firmwareUpdateStore.progress = 1;

    try {
      const response = await fetch("/api/online_update", { method: "POST" });
      if (response.ok) {
        // The device will restart, so maybe show a message "Update started, device will restart..."
        alert(t("firmware.onlineUpdateStarted"));
      } else {
        showError.value = true;
      }
      firmwareUpdateStore.progress = 0;
    } catch (error) {
      showError.value = true;
      firmwareUpdateStore.progress = 0;
    }
  }
};

const firmwareUpdateClick = async () => {
  showError.value = null;
  showSuccess.value = null;

  try {
    await firmwareUpdateStore.update(file.value);
    showSuccess.value = true;
    file.value = null;
  } catch (error) {
    showError.value = true;
  }
};

const restartClick = async () => {
  if (confirm(t("firmware.restartConfirm"))) {
    try {
      await fetch("/api/restart", { method: "POST" });
    } catch (error) {
      // Expected - device will restart and connection will be lost
    }
  }
};

onMounted(() => {
  sysInfoStore.update();
});
</script>

<style scoped></style>
