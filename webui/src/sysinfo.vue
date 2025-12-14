<template>
  <div>
    <BCard
      :header="t('sysinfo.title')"
      header-tag="h6"
      header-bg-variant="secondary"
      header-text-variant="white"
      class="mb-3"
    >
      <BForm @submit.stop.prevent>
        <BFormGroup :label="t('sysinfo.serial')" label-cols-sm="4">
          <BFormInput type="text" :model-value="sysInfoStore.serial" disabled />
        </BFormGroup>
        <BFormGroup :label="t('sysinfo.boardRevision')" label-cols-sm="4">
          <BFormInput
            type="text"
            :model-value="sysInfoStore.boardRevision"
            disabled
          />
        </BFormGroup>
        <BFormGroup :label="t('sysinfo.uptime')" label-cols-sm="4">
          <BFormInput type="text" :model-value="uptimeFormatted" disabled />
        </BFormGroup>
        <BFormGroup :label="t('sysinfo.resetReason')" label-cols-sm="4">
          <BFormInput
            type="text"
            :model-value="sysInfoStore.resetReason"
            disabled
          />
        </BFormGroup>
        <BFormGroup :label="t('sysinfo.cpuUsage')" label-cols-sm="4">
          <BProgress :max="100" height="2.25rem" class="form-control p-0">
            <BProgressBar :value="sysInfoStore.cpuUsage">
              <span
                class="justify-content-center d-flex position-absolute w-100 text-dark"
              >
                {{ sysInfoStore.cpuUsage.toFixed(2) }}%
              </span>
            </BProgressBar>
          </BProgress>
        </BFormGroup>
        <BFormGroup :label="t('sysinfo.memoryUsage')" label-cols-sm="4">
          <BProgress :max="100" height="2.25rem" class="form-control p-0">
            <BProgressBar :value="sysInfoStore.memoryUsage">
              <span
                class="justify-content-center d-flex position-absolute w-100 text-dark"
              >
                {{ sysInfoStore.memoryUsage.toFixed(2) }}%
              </span>
            </BProgressBar>
          </BProgress>
        </BFormGroup>
        <BFormGroup :label="t('sysinfo.ethernetStatus')" label-cols-sm="4">
          <BFormInput type="text" :model-value="ethernetStatus" disabled />
        </BFormGroup>
        <BFormGroup
          :label="t('sysinfo.rawUartRemoteAddress')"
          label-cols-sm="4"
        >
          <BFormInput
            type="text"
            :model-value="sysInfoStore.rawUartRemoteAddress"
            disabled
          />
        </BFormGroup>
        <BFormGroup :label="t('sysinfo.radioModuleType')" label-cols-sm="4">
          <BFormInput
            type="text"
            :model-value="sysInfoStore.radioModuleType"
            disabled
          />
        </BFormGroup>
        <BFormGroup :label="t('sysinfo.radioModuleSerial')" label-cols-sm="4">
          <BFormInput
            type="text"
            :model-value="sysInfoStore.radioModuleSerial"
            disabled
          />
        </BFormGroup>
        <BFormGroup :label="t('sysinfo.radioModuleFirmware')" label-cols-sm="4">
          <BFormInput
            type="text"
            :model-value="sysInfoStore.radioModuleFirmwareVersion"
            disabled
          />
        </BFormGroup>
        <BFormGroup
          :label="t('sysinfo.radioModuleBidCosRadioMAC')"
          label-cols-sm="4"
        >
          <BFormInput
            type="text"
            :model-value="sysInfoStore.radioModuleBidCosRadioMAC"
            disabled
          />
        </BFormGroup>
        <BFormGroup
          :label="t('sysinfo.radioModuleHmIPRadioMAC')"
          label-cols-sm="4"
        >
          <BFormInput
            type="text"
            :model-value="sysInfoStore.radioModuleHmIPRadioMAC"
            disabled
          />
        </BFormGroup>
        <BFormGroup :label="t('sysinfo.radioModuleSGTIN')" label-cols-sm="4">
          <BFormInput
            type="text"
            :model-value="sysInfoStore.radioModuleSGTIN"
            disabled
          />
        </BFormGroup>
      </BForm>
    </BCard>
  </div>
</template>

<script setup>
import { onMounted, onBeforeUnmount, computed } from "vue";
import { useI18n } from "vue-i18n";
import { useSysInfoStore } from "./stores.js";

const { t } = useI18n();

const sysInfoStore = useSysInfoStore();

// Ethernet status
const ethernetStatus = computed(() => {
  if (!sysInfoStore.ethernetConnected) return "Disconnected";
  return `${sysInfoStore.ethernetSpeed} Mbit/s (${sysInfoStore.ethernetDuplex})`;
});

// Uptime formatting
const uptimeFormatted = computed(() => {
  const seconds = sysInfoStore.uptimeSeconds;
  const days = Math.floor(seconds / 86400);
  const hours = Math.floor((seconds % 86400) / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  const secs = seconds % 60;

  if (days > 0) {
    return `${days}d ${hours}h ${minutes}m`;
  } else if (hours > 0) {
    return `${hours}h ${minutes}m ${secs}s`;
  } else {
    return `${minutes}m ${secs}s`;
  }
});

let updateTimer = null;

const startPolling = () => {
  if (!updateTimer) {
    sysInfoStore.update();
    updateTimer = setInterval(() => {
      sysInfoStore.update();
    }, 1000);
  }
};

const stopPolling = () => {
  if (updateTimer) {
    clearInterval(updateTimer);
    updateTimer = null;
  }
};

// Optimize performance by pausing polling when tab is hidden
const handleVisibilityChange = () => {
  if (document.hidden) {
    stopPolling();
  } else {
    startPolling();
  }
};

onMounted(() => {
  startPolling();
  document.addEventListener("visibilitychange", handleVisibilityChange);
});

onBeforeUnmount(() => {
  stopPolling();
  document.removeEventListener("visibilitychange", handleVisibilityChange);
});
</script>

<style scoped></style>
