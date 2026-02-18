<template>
  <div class="log-page">
    <div class="page-header">
      <h3>{{ t('systemlog.title') }}</h3>
      <p class="text-secondary">{{ t('systemlog.description') }}</p>
    </div>

    <div class="settings-card">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-info-light text-info">📋</div>
          <h3>{{ t('systemlog.liveLog') }}</h3>
        </div>
        <div class="log-actions">
          <div class="form-check form-switch me-3">
            <input class="form-check-input" type="checkbox" id="logEnabled" v-model="logEnabled">
            <label class="form-check-label" for="logEnabled">{{ t('systemlog.enabled') }}</label>
          </div>
          <button class="btn btn-sm btn-outline-secondary me-2" @click="refreshLog" :disabled="!logEnabled" :title="t('systemlog.refresh') || 'Refresh'">
            🔄
          </button>
          <button class="btn btn-sm btn-outline-secondary me-2" @click="toggleAutoScroll" :disabled="!logEnabled" :title="t('systemlog.autoScroll')">
            {{ autoScroll ? '⏸' : '▶' }}
          </button>
          <button class="btn btn-sm btn-outline-secondary me-2" @click="clearLog" :title="t('systemlog.clear')">
            🗑
          </button>
          <button class="btn btn-sm btn-outline-primary" @click="downloadLog" :title="t('systemlog.download')">
            ⬇ {{ t('systemlog.download') }}
          </button>
        </div>
      </div>

      <div class="card-body p-0">
        <div v-if="logEnabled" ref="logContainer" class="log-container" @scroll="onScroll">
          <pre class="log-content">{{ logContent || t('systemlog.empty') }}</pre>
        </div>
        <div v-else class="log-disabled">
          <p>{{ t('systemlog.disabledMessage') }}</p>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, watch, onMounted, onUnmounted, nextTick, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import axios from 'axios'

const { t } = useI18n()

const logContent = ref('')
const logContainer = ref(null)
const logEnabled = ref(true)
const autoScroll = ref(true)
const offset = ref(0)
let pollTimer = null

const startPolling = () => {
  stopPolling()
  fetchLog()
  pollTimer = setInterval(fetchLog, 3000)
}

const stopPolling = () => {
  if (pollTimer) {
    clearInterval(pollTimer)
    pollTimer = null
  }
}

watch(logEnabled, (enabled) => {
  if (enabled) {
    startPolling()
  } else {
    stopPolling()
  }
})

const fetchLog = async () => {
  try {
    const response = await axios.get('/api/log', {
      params: { offset: offset.value },
      timeout: 5000
    })
    if (response.data) {
      logContent.value += response.data
      // Use X-Log-Total header for accurate offset (handles ring buffer overflow)
      const totalWritten = parseInt(response.headers['x-log-total'])
      if (!isNaN(totalWritten)) {
        offset.value = totalWritten
      } else {
        offset.value += response.data.length
      }
      if (autoScroll.value) {
        await nextTick()
        scrollToBottom()
      }
    }
  } catch (error) {
    console.warn("Log poll failed:", error.response?.status || error.message)
  }
}

const refreshLog = () => {
  offset.value = 0
  logContent.value = ''
  fetchLog()
}

const downloadLog = async () => {
  try {
    const response = await axios.get('/api/log/download', {
      responseType: 'blob'
    })
    const url = window.URL.createObjectURL(new Blob([response.data]))
    const link = document.createElement('a')
    link.href = url
    link.setAttribute('download', 'hb-rf-eth-log.txt')
    document.body.appendChild(link)
    link.click()
    document.body.removeChild(link)
    setTimeout(() => window.URL.revokeObjectURL(url), 100)
  } catch (error) {
    console.error('Log download failed:', error)
    alert(t('common.error') + ': ' + (error.response?.status || error.message))
  }
}

const scrollToBottom = () => {
  if (logContainer.value) {
    logContainer.value.scrollTop = logContainer.value.scrollHeight
  }
}

const onScroll = () => {
  if (!logContainer.value) return
  const el = logContainer.value
  const atBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 50
  autoScroll.value = atBottom
}

const toggleAutoScroll = () => {
  autoScroll.value = !autoScroll.value
  if (autoScroll.value) scrollToBottom()
}

const clearLog = () => {
  logContent.value = ''
}

onMounted(() => {
  if (logEnabled.value) {
    startPolling()
  }
})

onUnmounted(() => {
  stopPolling()
})
</script>

<style scoped>
.log-page {
  max-width: 1000px;
  margin: 0 auto;
}

.page-header {
  margin-bottom: var(--spacing-xl);
  text-align: center;
}

.page-header h3 {
  font-size: 1.5rem;
  margin-bottom: var(--spacing-xs);
}

.settings-card {
  background: var(--color-surface);
  border-radius: var(--radius-xl);
  box-shadow: var(--shadow-md);
  margin-bottom: var(--spacing-lg);
  overflow: hidden;
}

.card-header {
  padding: var(--spacing-lg);
  background: transparent;
  border: none;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.header-content {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
}

.header-icon {
  width: 40px;
  height: 40px;
  border-radius: 10px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 1.25rem;
}

.card-header h3 {
  margin: 0;
  font-size: 1.125rem;
  font-weight: 600;
}

.log-actions {
  display: flex;
  align-items: center;
}

.log-container {
  height: 500px;
  overflow-y: auto;
  background: var(--color-surface);
  border: 2px solid var(--color-primary);
  border-radius: var(--radius-lg);
  margin: var(--spacing-sm);
}

.log-content {
  margin: 0;
  padding: var(--spacing-md);
  font-family: 'Courier New', monospace;
  font-size: 0.9rem;
  line-height: 1.6;
  color: var(--color-text);
  font-weight: 500;
  white-space: pre-wrap;
  word-break: break-all;
}

.log-disabled {
  height: 200px;
  display: flex;
  align-items: center;
  justify-content: center;
  background: var(--color-bg);
  border: 1px solid var(--color-border-light);
  color: var(--color-text-secondary);
  font-size: 0.9rem;
}

/* Utility Colors */
.bg-info-light { background-color: var(--color-info-light); }
.text-info { color: var(--color-info); }

/* Mobile */
@media (max-width: 768px) {
  .log-page {
    padding-bottom: var(--spacing-lg);
  }

  .page-header h3 {
    font-size: 1.25rem;
  }

  .settings-card {
    border-radius: var(--radius-lg);
  }

  .card-header {
    padding: var(--spacing-md);
    flex-direction: column;
    gap: var(--spacing-sm);
  }

  .log-container {
    height: 400px;
  }

  .log-content {
    font-size: 0.75rem;
  }
}
</style>
