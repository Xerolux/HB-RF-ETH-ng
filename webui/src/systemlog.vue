<template>
  <div class="page-shell log-page">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow">
          <AppIcon name="logs" />
          {{ t('systemlog.title') }}
        </span>
        <h1 class="hero-title">{{ t('systemlog.title') }}</h1>
        <p class="hero-subtitle">{{ t('systemlog.description') }}</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="activity" /> {{ paused ? 'Paused' : 'Live' }}</span>
        <span class="meta-chip"><AppIcon name="search" /> {{ filteredEntries.length }} lines</span>
        <span v-if="newEntriesCount > 0" class="meta-chip">{{ newEntriesCount }} new</span>
      </div>
    </section>

    <div class="settings-card card-glass">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-info-light text-info"><AppIcon name="logs" /></div>
          <div>
            <h3>{{ t('systemlog.liveLog') }}</h3>
            <p class="section-subtitle">Filter, pause and export the live device log.</p>
          </div>
        </div>
        <div class="log-actions">
          <label class="toggle-chip">
            <input type="checkbox" v-model="logEnabled">
            <span>{{ t('systemlog.enabled') }}</span>
          </label>
          <button class="tool-btn" type="button" :disabled="!logEnabled" @click="refreshLog">
            <AppIcon name="refresh" />
            Refresh
          </button>
          <button class="tool-btn" type="button" :disabled="!logEnabled" @click="togglePaused">
            <AppIcon name="activity" />
            {{ paused ? 'Resume' : 'Pause' }}
          </button>
          <button class="tool-btn" type="button" :disabled="!logEnabled" @click="toggleAutoScroll">
            <AppIcon name="download" />
            {{ autoScroll ? 'Auto scroll' : 'Manual scroll' }}
          </button>
          <button class="tool-btn" type="button" :disabled="!filteredEntries.length" @click="copyVisibleLog">
            <AppIcon name="copy" />
            Copy
          </button>
          <button class="tool-btn" type="button" @click="clearLog">
            <AppIcon name="close" />
            Clear
          </button>
          <button class="btn btn-primary btn-sm" type="button" @click="downloadLog">
            <AppIcon name="download" />
            {{ t('systemlog.download') }}
          </button>
        </div>
      </div>

      <div class="toolbar">
        <div class="search-field">
          <AppIcon name="search" />
          <input v-model.trim="searchQuery" type="search" placeholder="Search log output">
        </div>
        <select v-model="levelFilter" class="level-filter">
          <option value="all">All levels</option>
          <option value="E">Error</option>
          <option value="W">Warning</option>
          <option value="I">Info</option>
          <option value="D">Debug</option>
        </select>
      </div>

      <div class="card-body p-0">
        <div v-if="logEnabled" ref="logContainer" class="log-container" @scroll="onScroll">
          <div v-if="filteredEntries.length" class="log-list">
            <div
              v-for="(line, index) in filteredEntries"
              :key="`${index}-${line}`"
              class="log-line"
              :class="levelClass(line)"
            >
              <code>{{ line }}</code>
              <button class="line-copy" type="button" @click="copyLine(line)">
                <AppIcon name="copy" />
              </button>
            </div>
          </div>
          <div v-else class="log-empty">
            {{ searchQuery || levelFilter !== 'all' ? 'No matching log lines' : t('systemlog.empty') }}
          </div>
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
import { useUiStore } from './stores.js'

const { t } = useI18n()
const uiStore = useUiStore()

const logEntries = ref([])
const logContainer = ref(null)
const logEnabled = ref(true)
const autoScroll = ref(true)
const paused = ref(false)
const offset = ref(0)
const searchQuery = ref('')
const levelFilter = ref('all')
const newEntriesCount = ref(0)
let pollTimer = null

const MAX_LOG_LINES = 2500
const MAX_COPY_LINES = 500

const getEntryLevel = (line) => {
  if (/\bE\s*\(/.test(line) || /\bERROR\b/i.test(line)) return 'E'
  if (/\bW\s*\(/.test(line) || /\bWARN\b/i.test(line)) return 'W'
  if (/\bI\s*\(/.test(line) || /\bINFO\b/i.test(line)) return 'I'
  if (/\bD\s*\(/.test(line) || /\bDEBUG\b/i.test(line)) return 'D'
  return 'N'
}

const levelClass = (line) => {
  const level = getEntryLevel(line)
  if (level === 'E') return 'error'
  if (level === 'W') return 'warning'
  if (level === 'I') return 'info'
  return ''
}

const filteredEntries = computed(() => {
  const query = searchQuery.value.toLowerCase()
  return logEntries.value.filter((line) => {
    const matchesQuery = !query || line.toLowerCase().includes(query)
    const matchesLevel = levelFilter.value === 'all' || getEntryLevel(line) === levelFilter.value
    return matchesQuery && matchesLevel
  })
})

const appendChunk = (chunk) => {
  const lines = chunk
    .split(/\r?\n/)
    .map((line) => line.trimEnd())
    .filter((line) => line.length > 0)

  if (!lines.length) return

  for (const line of lines) {
    if (logEntries.value.length >= MAX_LOG_LINES) {
      logEntries.value.shift()
    }
    logEntries.value.push(line)
  }

  if (paused.value || !autoScroll.value) {
    newEntriesCount.value += lines.length
  }
}

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
  if (paused.value) return

  try {
    const response = await axios.get('/api/log', {
      params: { offset: offset.value },
      timeout: 5000
    })
    if (response.data) {
      appendChunk(response.data)
      const totalWritten = parseInt(response.headers['x-log-total'])
      if (!isNaN(totalWritten)) {
        offset.value = totalWritten
      } else {
        offset.value += response.data.length
      }
      if (autoScroll.value) {
        newEntriesCount.value = 0
        await nextTick()
        scrollToBottom()
      }
    }
  } catch (error) {
    console.warn('Log poll failed:', error.response?.status || error.message)
  }
}

const refreshLog = () => {
  offset.value = 0
  newEntriesCount.value = 0
  logEntries.value = []
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
    uiStore.pushToast({
      type: 'error',
      title: t('common.error'),
      message: String(error.response?.status || error.message)
    })
  }
}

const copyVisibleLog = async () => {
  const content = filteredEntries.value.slice(-MAX_COPY_LINES).join('\n')
  if (!content) return
  try {
    await navigator.clipboard.writeText(content)
    uiStore.pushToast({ type: 'success', title: t('common.success'), message: 'Visible log copied', duration: 1800 })
  } catch (error) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: 'Could not copy log output' })
  }
}

const copyLine = async (line) => {
  try {
    await navigator.clipboard.writeText(line)
    uiStore.pushToast({ type: 'success', title: t('common.success'), message: 'Log line copied', duration: 1400 })
  } catch (error) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: 'Could not copy log line' })
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
  if (atBottom) {
    newEntriesCount.value = 0
  }
}

const toggleAutoScroll = () => {
  autoScroll.value = !autoScroll.value
  if (autoScroll.value) {
    newEntriesCount.value = 0
    scrollToBottom()
  }
}

const togglePaused = () => {
  paused.value = !paused.value
  if (!paused.value) {
    newEntriesCount.value = 0
    fetchLog()
  }
}

const clearLog = () => {
  logEntries.value = []
  newEntriesCount.value = 0
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
}

.settings-card {
  border-radius: var(--radius-xl);
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
  gap: var(--spacing-md);
}

.header-content {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
}

.header-content p {
  margin: 4px 0 0;
  color: var(--color-text-secondary);
  font-size: 0.85rem;
}

.header-icon {
  width: 40px;
  height: 40px;
  border-radius: 10px;
  display: flex;
  align-items: center;
  justify-content: center;
}

.card-header h3 {
  margin: 0;
  font-size: 1.125rem;
  font-weight: 600;
}

.log-actions {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  flex-wrap: wrap;
  gap: 10px;
}

.toggle-chip {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  padding: 8px 12px;
  border-radius: var(--radius-full);
  background: rgba(255, 255, 255, 0.52);
  border: 1px solid var(--color-border-light);
}

.tool-btn {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  border: 1px solid var(--color-border-light);
  background: rgba(255, 255, 255, 0.6);
  border-radius: var(--radius-full);
  padding: 8px 12px;
  color: var(--color-text);
}

.tool-btn:disabled {
  opacity: 0.55;
}

.toolbar {
  padding: 0 var(--spacing-lg) var(--spacing-md);
  display: flex;
  gap: 12px;
  flex-wrap: wrap;
}

.search-field {
  flex: 1 1 280px;
  min-width: 240px;
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 12px 14px;
  border-radius: var(--radius-full);
  background: rgba(255, 255, 255, 0.64);
  border: 1px solid var(--color-border-light);
}

.search-field input,
.level-filter {
  border: none;
  background: transparent;
  width: 100%;
  color: var(--color-text);
  outline: none;
}

.level-filter {
  width: auto;
  min-width: 150px;
  padding: 12px 14px;
  border-radius: var(--radius-full);
  border: 1px solid var(--color-border-light);
  background: rgba(255, 255, 255, 0.64);
}

.log-container {
  height: clamp(300px, 55vh, 500px);
  overflow-y: auto;
  background: var(--color-surface);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-lg);
  margin: var(--spacing-sm);
}

.log-list {
  padding: 10px;
}

.log-line {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 10px;
  padding: 8px 10px;
  border-radius: 12px;
  color: var(--color-text);
}

.log-line:hover {
  background: rgba(127, 127, 127, 0.1);
}

.log-line code {
  color: inherit;
  white-space: pre-wrap;
  word-break: break-word;
  font-family: "Cascadia Code", "Courier New", monospace;
  font-size: 0.86rem;
}

.log-line.info {
  color: var(--color-text);
}

.log-line.warning {
  color: var(--color-warning);
}

.log-line.error {
  color: var(--color-danger);
}

.line-copy {
  border: none;
  background: transparent;
  color: var(--color-text-secondary);
  padding: 2px;
}

.log-empty {
  padding: 24px;
  color: var(--color-text-secondary);
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

.bg-info-light { background-color: var(--color-info-light); }
.text-info { color: var(--color-info); }

@media (max-width: 768px) {
  .settings-card {
    border-radius: var(--radius-lg);
  }

  .card-header {
    padding: var(--spacing-md);
    flex-direction: column;
    align-items: stretch;
  }

  .toolbar {
    padding: 0 var(--spacing-md) var(--spacing-md);
    flex-direction: column;
  }

  .level-filter {
    width: 100%;
  }

  .log-container {
    height: clamp(250px, 50vh, 400px);
  }

  .log-actions {
    gap: 6px;
  }

  .tool-btn {
    padding: 6px 10px;
    font-size: 0.8125rem;
  }

  .log-line code {
    font-size: 0.78rem;
  }
}
</style>
