<template>
  <div>
    <BAlert v-if="updateAvailable" variant="info" show dismissible>
      {{ t('firmware.newVersionAvailable', { version: sysInfoStore.latestVersion }) }}
      <BButton size="sm" variant="primary" class="ms-2" @click="performOnlineUpdate" :disabled="updating">
        {{ updating ? t('common.loading') : t('firmware.updateNow') }}
      </BButton>
    </BAlert>
    <SysInfo />
  </div>
</template>

<script setup>
import SysInfo from './sysinfo.vue'
import { useSysInfoStore } from './stores.js'
import { computed, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import axios from 'axios'

const { t } = useI18n()
const sysInfoStore = useSysInfoStore()
const updating = ref(false)

const updateAvailable = computed(() => {
  if (!sysInfoStore.latestVersion || !sysInfoStore.currentVersion) return false
  if (sysInfoStore.latestVersion === 'n/a') return false

  // Semantic version comparison
  const v1 = sysInfoStore.latestVersion.split('.').map(Number)
  const v2 = sysInfoStore.currentVersion.split('.').map(Number)

  for (let i = 0; i < Math.max(v1.length, v2.length); i++) {
    const num1 = v1[i] || 0
    const num2 = v2[i] || 0
    if (num1 > num2) return true
    if (num1 < num2) return false
  }
  return false
})

const performOnlineUpdate = async () => {
  if (!confirm(t('firmware.restartConfirm'))) return

  updating.value = true
  try {
    const res = await axios.post('/api/firmware/update-online')
    if (res.data.success) {
      alert(t('firmware.onlineUpdateSuccess'))
    } else {
      alert(t('firmware.onlineUpdateError'))
    }
  } catch (e) {
    alert(t('firmware.onlineUpdateError'))
  } finally {
    updating.value = false
  }
}
</script>

<style scoped>
</style>
