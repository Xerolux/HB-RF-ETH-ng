<template>
  <BCard>
    <template #header>
      <h5>{{ t('title') }}</h5>
    </template>

    <BAlert variant="info" :model-value="true" class="mb-3">
      {{ t('description') }}
    </BAlert>

    <!-- SNMP Configuration -->
    <h6 class="mt-3">{{ t('snmp.title') }}</h6>
    <BForm>
      <BFormGroup label-cols-sm="4" :label="t('snmp.enabled')">
        <BFormCheckbox v-model="snmpConfig.enabled" switch />
      </BFormGroup>

      <template v-if="snmpConfig.enabled">
        <BFormGroup label-cols-sm="4" :label="t('snmp.port')">
          <BFormInput v-model.number="snmpConfig.port" type="number" min="1" max="65535" />
          <BFormText>{{ t('snmp.portHelp') }}</BFormText>
        </BFormGroup>

        <BFormGroup label-cols-sm="4" :label="t('snmp.community')">
          <BFormInput v-model="snmpConfig.community" />
          <BFormText>{{ t('snmp.communityHelp') }}</BFormText>
        </BFormGroup>

        <BFormGroup label-cols-sm="4" :label="t('snmp.location')">
          <BFormInput v-model="snmpConfig.location" />
          <BFormText>{{ t('snmp.locationHelp') }}</BFormText>
        </BFormGroup>

        <BFormGroup label-cols-sm="4" :label="t('snmp.contact')">
          <BFormInput v-model="snmpConfig.contact" />
          <BFormText>{{ t('snmp.contactHelp') }}</BFormText>
        </BFormGroup>
      </template>
    </BForm>

    <hr />

    <!-- CheckMK Configuration -->
    <h6 class="mt-3">{{ t('checkmk.title') }}</h6>
    <BForm>
      <BFormGroup label-cols-sm="4" :label="t('checkmk.enabled')">
        <BFormCheckbox v-model="checkmkConfig.enabled" switch />
      </BFormGroup>

      <template v-if="checkmkConfig.enabled">
        <BFormGroup label-cols-sm="4" :label="t('checkmk.port')">
          <BFormInput v-model.number="checkmkConfig.port" type="number" min="1" max="65535" />
          <BFormText>{{ t('checkmk.portHelp') }}</BFormText>
        </BFormGroup>

        <BFormGroup label-cols-sm="4" :label="t('checkmk.allowedHosts')">
          <BFormInput v-model="checkmkConfig.allowedHosts" />
          <BFormText>{{ t('checkmk.allowedHostsHelp') }}</BFormText>
        </BFormGroup>
      </template>
    </BForm>

    <hr />

    <BAlert v-if="showSuccess" variant="success" dismissible @dismissed="showSuccess = false">
      {{ t('saveSuccess') }}
    </BAlert>

    <BAlert v-if="showError" variant="danger" dismissible @dismissed="showError = false">
      {{ t('saveError') }}
    </BAlert>

    <BButton variant="primary" @click="saveConfig" :disabled="saving">
      <span v-if="saving">{{ t('saving') }}</span>
      <span v-else>{{ t('save') }}</span>
    </BButton>
  </BCard>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useMonitoringStore } from './stores.js'
import { storeToRefs } from 'pinia'

const { t } = useI18n({
  locale: navigator.language,
  fallbackLocale: 'en',
  messages: {
    de: {
      title: 'Monitoring',
      description: 'Konfigurieren Sie SNMP und CheckMK Monitoring für das HB-RF-ETH Gateway.',
      save: 'Speichern',
      saving: 'Speichern...',
      saveSuccess: 'Konfiguration erfolgreich gespeichert!',
      saveError: 'Fehler beim Speichern der Konfiguration!',
      snmp: {
        title: 'SNMP Agent',
        enabled: 'SNMP aktivieren',
        port: 'Port',
        portHelp: 'Standard: 161',
        community: 'Community String',
        communityHelp: 'Standard: "public" - Bitte ändern für Produktivumgebung!',
        location: 'Standort (Location)',
        locationHelp: 'Optional: z.B. "Serverraum, Gebäude A"',
        contact: 'Kontakt',
        contactHelp: 'Optional: z.B. "admin@example.com"'
      },
      checkmk: {
        title: 'CheckMK Agent',
        enabled: 'CheckMK aktivieren',
        port: 'Port',
        portHelp: 'Standard: 6556',
        allowedHosts: 'Erlaubte Client-IPs',
        allowedHostsHelp: 'Komma-getrennte IP-Adressen (z.B. "192.168.1.10,192.168.1.20") oder "*" für alle'
      }
    },
    en: {
      title: 'Monitoring',
      description: 'Configure SNMP and CheckMK monitoring for the HB-RF-ETH gateway.',
      save: 'Save',
      saving: 'Saving...',
      saveSuccess: 'Configuration saved successfully!',
      saveError: 'Error saving configuration!',
      snmp: {
        title: 'SNMP Agent',
        enabled: 'Enable SNMP',
        port: 'Port',
        portHelp: 'Default: 161',
        community: 'Community String',
        communityHelp: 'Default: "public" - Please change for production!',
        location: 'Location',
        locationHelp: 'Optional: e.g. "Server room, Building A"',
        contact: 'Contact',
        contactHelp: 'Optional: e.g. "admin@example.com"'
      },
      checkmk: {
        title: 'CheckMK Agent',
        enabled: 'Enable CheckMK',
        port: 'Port',
        portHelp: 'Default: 6556',
        allowedHosts: 'Allowed Client IPs',
        allowedHostsHelp: 'Comma-separated IP addresses (e.g. "192.168.1.10,192.168.1.20") or "*" for all'
      }
    }
  }
})

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
