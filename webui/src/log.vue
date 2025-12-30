<template>
  <div class="mt-4">
    <BCard :title="t('log.title')">
      <div class="mb-3 d-flex justify-content-between align-items-center">
        <div class="d-flex gap-2">
          <BButton variant="primary" @click="resetAndFetch" :disabled="loading">
            <BSpinner small v-if="loading" />
            <span v-else>↻</span>
            {{ t('log.refresh') }}
          </BButton>
          <BButton variant="secondary" @click="downloadLog" :disabled="!logContent">
            {{ t('log.download') }}
          </BButton>
          <BButton variant="danger" @click="clearLog" :disabled="!logContent">
             🗑️ {{ t('common.clear', 'Clear') }}
          </BButton>
        </div>
        <div class="d-flex align-items-center gap-3">
             <BFormCheckbox v-model="autoScroll">
              {{ t('log.autoScroll', 'Auto Scroll') }}
           </BFormCheckbox>
           <BFormCheckbox v-model="autoRefresh" switch>
              {{ t('log.autoRefresh') }}
           </BFormCheckbox>
        </div>
      </div>

      <div class="log-container bg-dark text-light p-3 rounded" ref="logContainerRef">
        <pre class="m-0" style="white-space: pre-wrap; word-wrap: break-word; font-family: 'Consolas', 'Monaco', monospace; font-size: 0.85rem;">{{ logContent || t('log.noLog') }}</pre>
      </div>
    </BCard>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, watch, nextTick } from 'vue'
import { useI18n } from 'vue-i18n'
import axios from 'axios'

const { t } = useI18n()
const logContent = ref('')
const loading = ref(false)
const autoRefresh = ref(false)
const autoScroll = ref(true)
const logContainerRef = ref(null)

let refreshInterval = null
// Track the offset (length) of data we have currently
let currentOffset = 0

const fetchLogDelta = async () => {
  if (loading.value && !autoRefresh.value) return; // Prevent concurrent manual fetches

  // If we are auto-refreshing, we don't show the spinner to avoid flickering
  if (!autoRefresh.value) loading.value = true

  try {
    const response = await axios.get('/api/log', {
        params: { offset: currentOffset },
        transformResponse: [data => data] // Prevent axios from trying to parse JSON
    })

    const newContent = response.data
    if (newContent && newContent.length > 0) {
        logContent.value += newContent
        currentOffset += newContent.length

        if (autoScroll.value) {
            nextTick(() => {
                scrollToBottom()
            })
        }
    }
  } catch (error) {
    console.error('Failed to fetch log', error)
  } finally {
    loading.value = false
  }
}

const resetAndFetch = () => {
    logContent.value = ''
    currentOffset = 0
    fetchLogDelta()
}

const clearLog = () => {
    logContent.value = ''
    // We don't reset offset on backend (it's a ring buffer), but effectively we clear the view
    // If we want to really clear backend, we'd need an API.
    // For now, just clear view and move offset to "end" by doing a fetch that ignores result?
    // Actually, simply clearing the view is fine.
    // Ideally we should sync with backend, but currentOffset is our local pointer.
    // If we clear local, we should probably keep currentOffset as is, so we only get NEW logs.
    // But if the user wants to clear the screen, that's what we do.
}

const scrollToBottom = () => {
    if (logContainerRef.value) {
        logContainerRef.value.scrollTop = logContainerRef.value.scrollHeight
    }
}

const downloadLog = async () => {
  // Download full log
  const response = await axios.get('/api/log/download', {
        transformResponse: [data => data]
    })

  const blob = new Blob([response.data], { type: 'text/plain' })
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
        // Fetch immediately then interval
        fetchLogDelta()
        refreshInterval = setInterval(fetchLogDelta, 2000) // 2 seconds poll
    } else {
        if (refreshInterval) clearInterval(refreshInterval)
    }
})

onMounted(() => {
  // Initial fetch
  fetchLogDelta()
})

onUnmounted(() => {
  if (refreshInterval) clearInterval(refreshInterval)
})
</script>

<style scoped>
.log-container {
  max-height: 600px;
  overflow-y: auto;
  min-height: 300px;
}
</style>
