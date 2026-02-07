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
          <BButton variant="info" @click="pasteLog" :disabled="!logContent || pasting">
            <BSpinner small v-if="pasting" />
            <span v-else>&#x1F4CB;</span>
            {{ t('log.paste', 'Paste') }}
          </BButton>
          <BButton variant="danger" @click="clearLog" :disabled="!logContent">
             &#x1F5D1; {{ t('common.clear', 'Clear') }}
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

      <BAlert v-if="pasteUrl" variant="success" show dismissible @dismissed="pasteUrl = ''">
        <div class="d-flex align-items-center gap-2">
          <span>{{ t('log.pasteSuccess', 'Log uploaded:') }}</span>
          <a :href="pasteUrl" target="_blank" class="text-break">{{ pasteUrl }}</a>
          <BButton size="sm" variant="outline-light" @click="copyPasteUrl">
            {{ copied ? '✓' : t('log.copyLink', 'Copy Link') }}
          </BButton>
        </div>
      </BAlert>

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
const pasting = ref(false)
const pasteUrl = ref('')
const copied = ref(false)

let refreshInterval = null
let currentOffset = 0

const fetchLogDelta = async () => {
  if (loading.value && !autoRefresh.value) return;

  if (!autoRefresh.value) loading.value = true

  try {
    const response = await axios.get('/api/log', {
        params: { offset: currentOffset },
        transformResponse: [data => data]
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
}

const scrollToBottom = () => {
    if (logContainerRef.value) {
        logContainerRef.value.scrollTop = logContainerRef.value.scrollHeight
    }
}

const pasteLog = async () => {
  if (!logContent.value) return
  pasting.value = true
  pasteUrl.value = ''
  copied.value = false

  try {
    const response = await axios.post('/api/log/paste')
    if (response.data && response.data.success && response.data.url) {
      pasteUrl.value = response.data.url
    } else {
      console.error('Paste upload failed:', response.data)
      alert(t('log.pasteFailed', 'Upload failed. Please try again.'))
    }
  } catch (error) {
    console.error('Paste upload error:', error)
    alert(t('log.pasteFailed', 'Upload failed. Please try again.'))
  } finally {
    pasting.value = false
  }
}

const copyPasteUrl = async () => {
  try {
    await navigator.clipboard.writeText(pasteUrl.value)
    copied.value = true
    setTimeout(() => { copied.value = false }, 2000)
  } catch {
    const textArea = document.createElement('textarea')
    textArea.value = pasteUrl.value
    document.body.appendChild(textArea)
    textArea.select()
    document.execCommand('copy')
    document.body.removeChild(textArea)
    copied.value = true
    setTimeout(() => { copied.value = false }, 2000)
  }
}

watch(autoRefresh, (val) => {
    if (val) {
        fetchLogDelta()
        refreshInterval = setInterval(fetchLogDelta, 2000)
    } else {
        if (refreshInterval) clearInterval(refreshInterval)
    }
})

onMounted(() => {
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
