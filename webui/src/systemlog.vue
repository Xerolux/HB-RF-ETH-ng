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
          <button class="btn btn-sm btn-outline-secondary me-2" @click="toggleAutoScroll" :title="t('systemlog.autoScroll')">
            {{ autoScroll ? '⏸' : '▶' }}
          </button>
          <button class="btn btn-sm btn-outline-secondary me-2" @click="clearLog" :title="t('systemlog.clear')">
            🗑
          </button>
          <a class="btn btn-sm btn-outline-primary" :href="downloadUrl" :title="t('systemlog.download')">
            ⬇ {{ t('systemlog.download') }}
          </a>
        </div>
      </div>

      <div class="card-body p-0">
        <div ref="logContainer" class="log-container" @scroll="onScroll">
          <pre class="log-content">{{ logContent || t('systemlog.empty') }}</pre>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, nextTick, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import axios from 'axios'

const { t } = useI18n()

const logContent = ref('')
const logContainer = ref(null)
const autoScroll = ref(true)
const offset = ref(0)
let pollTimer = null

const downloadUrl = computed(() => '/api/log/download')

const fetchLog = async () => {
  try {
    const response = await axios.get('/api/log', {
      params: { offset: offset.value }
    })
    if (response.data) {
      logContent.value += response.data
      offset.value += response.data.length
      if (autoScroll.value) {
        await nextTick()
        scrollToBottom()
      }
    }
  } catch (error) {
    // Silently ignore poll errors
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
  fetchLog()
  pollTimer = setInterval(fetchLog, 3000)
})

onUnmounted(() => {
  if (pollTimer) clearInterval(pollTimer)
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
  background: var(--color-bg, #1a1a2e);
  border-top: 1px solid var(--color-border-light);
}

.log-content {
  margin: 0;
  padding: var(--spacing-md);
  font-family: 'Courier New', monospace;
  font-size: 0.8125rem;
  line-height: 1.5;
  color: #a8b2d1;
  white-space: pre-wrap;
  word-break: break-all;
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
