<template>
  <div class="mt-4">
    <BCard :title="t('log.title')">
      <div class="mb-3 d-flex justify-content-between align-items-center">
        <div class="d-flex gap-2">
          <BButton variant="primary" @click="fetchLog" :disabled="loading">
            <BSpinner small v-if="loading" />
            <span v-else>↻</span>
            {{ t('log.refresh') }}
          </BButton>
          <BButton variant="secondary" @click="downloadLog" :disabled="loading || !logContent">
            {{ t('log.download') }}
          </BButton>
        </div>
        <div>
           <BFormCheckbox v-model="autoRefresh" switch>
              {{ t('log.autoRefresh') }}
           </BFormCheckbox>
        </div>
      </div>

      <div class="log-container bg-dark text-light p-3 rounded" ref="logContainer">
        <pre class="m-0" style="white-space: pre-wrap; word-wrap: break-word; font-family: 'Consolas', 'Monaco', monospace; font-size: 0.85rem;">{{ logContent || t('log.noLog') }}</pre>
      </div>
    </BCard>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import axios from 'axios'

const { t } = useI18n()
const logContent = ref('')
const loading = ref(false)
const autoRefresh = ref(false)
let refreshInterval = null

const fetchLog = async () => {
  loading.value = true
  try {
    const response = await axios.get('/api/log', {
        transformResponse: [data => data] // Prevent axios from trying to parse JSON
    })
    logContent.value = response.data
  } catch (error) {
    console.error('Failed to fetch log', error)
  } finally {
    loading.value = false
  }
}

const downloadLog = () => {
  const blob = new Blob([logContent.value], { type: 'text/plain' })
  const url = window.URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = `hb-rf-eth-log-${new Date().toISOString().slice(0, 19).replace(/:/g, '-')}.txt`
  document.body.appendChild(a)
  a.click()
  window.URL.revokeObjectURL(url)
  document.body.removeChild(a)
}

watch(autoRefresh, (val) => {
    if (val) {
        fetchLog()
        refreshInterval = setInterval(fetchLog, 5000)
    } else {
        if (refreshInterval) clearInterval(refreshInterval)
    }
})

onMounted(() => {
  fetchLog()
})

onUnmounted(() => {
  if (refreshInterval) clearInterval(refreshInterval)
})
</script>

<style scoped>
.log-container {
  max-height: 600px;
  overflow-y: auto;
}
</style>
