<template>
  <BCard>
    <template #header>
      <h5>{{ t('monitoring.title') }}</h5>
    </template>

    <BAlert variant="info" :model-value="true" class="mb-3">
      {{ t('monitoring.description') }}
    </BAlert>

    <!-- SNMP Configuration -->
    <h6 class="mt-3">{{ t('monitoring.snmp.title') }}</h6>
    <BForm>
      <BFormGroup label-cols-sm="4" :label="t('monitoring.snmp.enabled')">
        <BFormCheckbox v-model="snmpConfig.enabled" switch />
      </BFormGroup>

      <template v-if="snmpConfig.enabled">
        <BFormGroup label-cols-sm="4" :label="t('monitoring.snmp.port')">
          <BFormInput v-model.number="snmpConfig.port" type="number" min="1" max="65535" />
          <BFormText>{{ t('monitoring.snmp.portHelp') }}</BFormText>
        </BFormGroup>

        <BFormGroup label-cols-sm="4" :label="t('monitoring.snmp.community')">
          <BFormInput v-model="snmpConfig.community" />
          <BFormText>{{ t('monitoring.snmp.communityHelp') }}</BFormText>
        </BFormGroup>

        <BFormGroup label-cols-sm="4" :label="t('monitoring.snmp.location')">
          <BFormInput v-model="snmpConfig.location" />
          <BFormText>{{ t('monitoring.snmp.locationHelp') }}</BFormText>
        </BFormGroup>

        <BFormGroup label-cols-sm="4" :label="t('monitoring.snmp.contact')">
          <BFormInput v-model="snmpConfig.contact" />
          <BFormText>{{ t('monitoring.snmp.contactHelp') }}</BFormText>
        </BFormGroup>
      </template>
    </BForm>

    <hr />

    <!-- CheckMK Configuration -->
    <h6 class="mt-3">{{ t('monitoring.checkmk.title') }}</h6>
    <BForm>
      <BFormGroup label-cols-sm="4" :label="t('monitoring.checkmk.enabled')">
        <BFormCheckbox v-model="checkmkConfig.enabled" switch />
      </BFormGroup>

      <template v-if="checkmkConfig.enabled">
        <BFormGroup label-cols-sm="4" :label="t('monitoring.checkmk.port')">
          <BFormInput v-model.number="checkmkConfig.port" type="number" min="1" max="65535" />
          <BFormText>{{ t('monitoring.checkmk.portHelp') }}</BFormText>
        </BFormGroup>

        <BFormGroup label-cols-sm="4" :label="t('monitoring.checkmk.allowedHosts')">
          <BFormInput v-model="checkmkConfig.allowedHosts" />
          <BFormText>{{ t('monitoring.checkmk.allowedHostsHelp') }}</BFormText>
        </BFormGroup>
      </template>
    </BForm>

    <hr />

    <BAlert v-if="showSuccess" variant="success" dismissible @dismissed="showSuccess = false">
      {{ t('monitoring.saveSuccess') }}
    </BAlert>

    <BAlert v-if="showError" variant="danger" dismissible @dismissed="showError = false">
      {{ t('monitoring.saveError') }}
    </BAlert>

    <BButton variant="primary" @click="saveConfig" :disabled="saving">
      <span v-if="saving">{{ t('monitoring.saving') }}</span>
      <span v-else>{{ t('monitoring.save') }}</span>
    </BButton>
  </BCard>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useMonitoringStore } from './stores.js'
import { storeToRefs } from 'pinia'

const { t } = useI18n()

const monitoringStore = useMonitoringStore()
const { snmp: snmpConfig, checkmk: checkmkConfig } = storeToRefs(monitoringStore)

const showSuccess = ref(false)
const showError = ref(false)
const saving = ref(false)

onMounted(async () => {
  try {
    await monitoringStore.load()
  } catch (error) {
    // Error is logged in store
  }
})

const saveConfig = async () => {
  saving.value = true
  showSuccess.value = false
  showError.value = false

  try {
    await monitoringStore.save({
      snmp: snmpConfig.value,
      checkmk: checkmkConfig.value
    })
    showSuccess.value = true
  } catch (error) {
    showError.value = true
  } finally {
    saving.value = false
  }
}
</script>
