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
        <span class="meta-chip"><AppIcon name="activity" /> {{ !logEnabled ? t('systemlog.disabled') : (paused ? t('systemlog.paused') : t('systemlog.live')) }}</span>
        <span class="meta-chip"><AppIcon name="search" /> {{ filteredEntries.length }} {{ t('systemlog.lines') }}</span>
        <span v-if="newEntriesCount > 0" class="meta-chip">{{ newEntriesCount }} {{ t('systemlog.newEntries') }}</span>
      </div>
    </section>

    <div class="settings-card card-glass">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-info-light text-info"><AppIcon name="logs" /></div>
          <div>
            <h3>{{ t('systemlog.liveLog') }}</h3>
            <p class="section-subtitle">{{ t('systemlog.subtitle') }}</p>
          </div>
        </div>
        <div class="log-actions">
          <label class="toggle-chip">
            <input type="checkbox" :disabled="logToggleBusy" v-model="logEnabled">
            <span>{{ logEnabled ? t('systemlog.enabled') : t('systemlog.disabled') }}</span>
          </label>
          <button class="tool-btn" type="button" :disabled="!logEnabled" @click="refreshLog">
            <AppIcon name="refresh" />
            {{ t('systemlog.refresh') }}
          </button>
          <button class="tool-btn" type="button" :disabled="!logEnabled" @click="togglePaused">
            <AppIcon name="activity" />
            {{ paused ? t('systemlog.resume') : t('systemlog.pause') }}
          </button>
          <button class="tool-btn" type="button" :disabled="!logEnabled" @click="toggleAutoScroll">
            <AppIcon name="chevronDown" />
            {{ autoScroll ? t('systemlog.autoScroll') : t('systemlog.manualScroll') }}
          </button>
          <button class="tool-btn" type="button" :disabled="!filteredEntries.length" @click="copyVisibleLog">
            <AppIcon name="copy" />
            {{ t('systemlog.copy') }}
          </button>
          <button class="tool-btn" type="button" @click="clearLog">
            <AppIcon name="close" />
            {{ t('systemlog.clear') }}
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
          <input v-model.trim="searchQuery" type="search" :placeholder="t('systemlog.searchPlaceholder')">
        </div>
        <select v-model="levelFilter" class="level-filter">
          <option value="all">{{ t('systemlog.allLevels') }}</option>
          <option value="E">{{ t('systemlog.levelError') }}</option>
          <option value="W">{{ t('systemlog.levelWarning') }}</option>
          <option value="I">{{ t('systemlog.levelInfo') }}</option>
          <option value="D">{{ t('systemlog.levelDebug') }}</option>
        </select>
      </div>

      <div class="card-body p-0">
        <div v-if="logEnabled" ref="logContainer" class="log-container" @scroll="onScroll">
          <div v-if="filteredEntries.length" class="log-list">
            <div
              v-for="(entry, index) in filteredEntries"
              :key="`${index}-${entry.raw}`"
              class="log-line"
              :class="levelClass(entry)"
            >
              <code>{{ entry.raw }}</code>
              <button class="line-copy" type="button" @click="copyLine(entry.raw)">
                <AppIcon name="copy" />
              </button>
            </div>
          </div>
          <div v-else class="log-empty">
            {{ searchQuery || levelFilter !== 'all' ? t('systemlog.noMatches') : t('systemlog.empty') }}
          </div>
        </div>
        <div v-else class="log-disabled">
          <p>{{ t('systemlog.disabledMessage') }}</p>
        </div>
      </div>
    </div>

    <!--
      Crash recovery snapshot. When the device restarted due to a watchdog
      or panic, a tail of the in-memory log was saved to NVS right before
      the restart. The snapshot is delivered once (the backend clears it
      after the first read), so this modal auto-opens exactly once after a
      real crash — making "the log was wiped" debuggable for the first time.
    -->
    <BModal
      v-model="showCrashModal"
      :title="t('systemlog.crashTitle')"
      :ok-title="t('common.close')"
      ok-only
      size="lg"
      scrollable
    >
      <div class="alert alert-warning" role="alert">
        {{ t('systemlog.crashHint') }}
      </div>
      <pre class="crash-tail-pre">{{ crashTail }}</pre>
    </BModal>
  </div>
</template>

<script setup>
import { ref, watch, onMounted, onUnmounted, nextTick, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import axios from 'axios'
import { useUiStore } from './stores.js'
import { copyToClipboard } from './composables/useClipboard'

const { t } = useI18n()
const uiStore = useUiStore()

const logEntries = ref([])
const logContainer = ref(null)
const logEnabled = ref(false)
const logToggleBusy = ref(false)
const autoScroll = ref(true)
const paused = ref(false)
const offset = ref(0)
const searchQuery = ref('')
const levelFilter = ref('all')
const newEntriesCount = ref(0)
// Last-crash tail recovered from NVS after a watchdog/panic restart.
// Empty when no snapshot was stored (normal reboot, first boot, etc.).
const crashTail = ref('')
const showCrashModal = ref(false)
let pollTimer = null
// Suppresses the logEnabled watcher while we sync the toggle from the backend
// status on mount (so the initial state does not trigger a redundant enable
// POST).
let syncingFromBackend = false

const MAX_LOG_LINES = 2500
const MAX_COPY_LINES = 500

// copyToClipboard is provided by ./composables/useClipboard.js (iOS-Safari-safe).

const stripAnsi = value => value.replace(/\u001b\[[0-?]*[ -/]*[@-~]/g, '')

const getEntryLevel = (line) => {
  const normalized = stripAnsi(line)
  if (/(?:^|\s)E\s*\(/.test(normalized) || /\bERROR\b/i.test(normalized)) return 'E'
  if (/(?:^|\s)W\s*\(/.test(normalized) || /\bWARN\b/i.test(normalized)) return 'W'
  if (/(?:^|\s)I\s*\(/.test(normalized) || /\bINFO\b/i.test(normalized)) return 'I'
  if (/(?:^|\s)D\s*\(/.test(normalized) || /\bDEBUG\b/i.test(normalized)) return 'D'
  return 'N'
}

const levelClass = (entry) => {
  const level = entry.level
  if (level === 'E') return 'error'
  if (level === 'W') return 'warning'
  if (level === 'I') return 'info'
  return ''
}

const filteredEntries = computed(() => {
  const query = searchQuery.value.toLowerCase()
  const filter = levelFilter.value
  return logEntries.value.filter((entry) => {
    const matchesQuery = !query || entry.lowerRaw.includes(query)
    const matchesLevel = filter === 'all' || entry.level === filter
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
    logEntries.value.push({
      raw: line,
      level: getEntryLevel(line),
      lowerRaw: line.toLowerCase()
    })
  }

  if (paused.value || !autoScroll.value) {
    newEntriesCount.value += lines.length
  }
}

// ---- WebSocket live stream (Phase E) -------------------------------------
// The browser cannot attach Authorization headers to a WebSocket upgrade, so
// the token is passed via ?token=. The backend validates it before upgrading.
const WS_SNAPSHOT_PREFIX = 'stream snapshot '
const WS_BACKLOG_PREFIX = 'stream backlog '
const WS_READY_PREFIX = 'stream connected '
const WS_DATA_PREFIX = 'stream data '
const WS_HANDSHAKE_TIMEOUT_MS = 5000
const WS_RECONNECT_DELAY_MS = 2000
let ws = null
let wsReconnectTimer = null
let wsHandshakeTimer = null
let wsSnapshotOffset = null
let wsBacklogBytes = null
const wsConnected = ref(false)

const clearWsHandshakeTimer = () => {
  if (wsHandshakeTimer) {
    clearTimeout(wsHandshakeTimer)
    wsHandshakeTimer = null
  }
}

const scheduleNextPoll = () => {
  if (pollTimer) clearTimeout(pollTimer)
  pollTimer = null
  // The acknowledged WebSocket snapshot/live offsets are authoritative.
  // Poll only as a fallback while no application-level stream is ready.
  if (!logEnabled.value || wsConnected.value) return

  pollTimer = setTimeout(async () => {
    pollTimer = null
    await fetchLog()
    scheduleNextPoll()
  }, wsConnected.value ? 30000 : 5000)
}

const scheduleWsReconnect = () => {
  if (!logEnabled.value || wsReconnectTimer) return
  wsReconnectTimer = setTimeout(() => {
    wsReconnectTimer = null
    if (logEnabled.value) openLogStream()
  }, WS_RECONNECT_DELAY_MS)
}

const openLogStream = () => {
  closeLogStream()
  try {
    const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
    const token = sessionStorage.getItem('hb-rf-eth-ng-pw') || ''
    const url = `${proto}//${window.location.host}/api/log/stream?token=${encodeURIComponent(token)}&offset=${offset.value}`
    const socket = new WebSocket(url)
    ws = socket
    wsSnapshotOffset = null
    wsBacklogBytes = null
    socket.binaryType = 'arraybuffer'

    // A successful protocol upgrade is not enough: wait for the server's
    // post-handshake acknowledgement before treating the stream as live.
    socket.onopen = () => {
      if (ws !== socket) return
      clearWsHandshakeTimer()
      wsHandshakeTimer = setTimeout(() => {
        if (ws === socket && !wsConnected.value) socket.close()
      }, WS_HANDSHAKE_TIMEOUT_MS)
    }
    socket.onclose = async () => {
      // Ignore a late close event from a socket that closeLogStream() already
      // replaced. Otherwise it can null the new socket and start two loops.
      if (ws !== socket) return
      clearWsHandshakeTimer()
      wsConnected.value = false
      ws = null
      wsSnapshotOffset = null
      wsBacklogBytes = null
      await fetchLog()
      scheduleNextPoll()
      scheduleWsReconnect()
    }
    socket.onerror = () => { /* close handler will reconnect */ }
    socket.onmessage = (ev) => {
      if (ws !== socket) return
      // ev.data is a string text frame from the device.
      if (typeof ev.data !== 'string') return

      if (!wsConnected.value) {
        if (ev.data.startsWith(WS_SNAPSHOT_PREFIX)) {
          const value = Number(ev.data.slice(WS_SNAPSHOT_PREFIX.length).trim())
          if (!Number.isSafeInteger(value) || value < 0) return socket.close()
          wsSnapshotOffset = value
          wsBacklogBytes = null
          return
        }

        if (ev.data.startsWith(WS_BACKLOG_PREFIX) && wsSnapshotOffset !== null) {
          const value = Number(ev.data.slice(WS_BACKLOG_PREFIX.length).trim())
          if (!Number.isSafeInteger(value) || value < 0) return socket.close()
          wsBacklogBytes = value
          return
        }

        if (wsBacklogBytes > 0) {
          const actualBytes = new TextEncoder().encode(ev.data).length
          if (actualBytes !== wsBacklogBytes) return socket.close()
          appendChunk(ev.data)
          wsBacklogBytes = 0
          return
        }

        if (ev.data.startsWith(WS_READY_PREFIX) &&
            wsSnapshotOffset !== null && wsBacklogBytes === 0) {
          const value = Number(ev.data.slice(WS_READY_PREFIX.length).trim())
          if (!Number.isSafeInteger(value) || value !== wsSnapshotOffset) return socket.close()
          offset.value = value
          clearWsHandshakeTimer()
          wsConnected.value = true
          wsSnapshotOffset = null
          wsBacklogBytes = null
          scheduleNextPoll()
          if (autoScroll.value) {
            newEntriesCount.value = 0
            nextTick(() => scrollToBottom())
          }
        }
        return
      }

      if (!ev.data.startsWith(WS_DATA_PREFIX)) return
      const headerEnd = ev.data.indexOf('\n')
      if (headerEnd < 0) return socket.close()

      const endOffset = Number(ev.data.slice(WS_DATA_PREFIX.length, headerEnd).trim())
      const payload = ev.data.slice(headerEnd + 1)
      const encodedPayload = new TextEncoder().encode(payload)
      if (!Number.isSafeInteger(endOffset) || endOffset < encodedPayload.length) return socket.close()

      const startOffset = endOffset - encodedPayload.length
      if (endOffset <= offset.value) return // stale queued frame from before the snapshot
      if (startOffset > offset.value) return socket.close() // queue gap; reconnect for a snapshot

      const overlap = Math.max(0, offset.value - startOffset)
      const newPayload = overlap > 0
        ? new TextDecoder().decode(encodedPayload.slice(overlap))
        : payload
      offset.value = endOffset
      if (newPayload) {
        appendChunk(newPayload)
        if (autoScroll.value) {
          newEntriesCount.value = 0
          nextTick(() => scrollToBottom())
        }
      }
    }
  } catch (e) {
    console.warn('WebSocket log stream unavailable, falling back to polling', e)
    scheduleWsReconnect()
  }
}

const closeLogStream = () => {
  if (wsReconnectTimer) {
    clearTimeout(wsReconnectTimer)
    wsReconnectTimer = null
  }
  clearWsHandshakeTimer()
  const socket = ws
  // Clear identity before close so its asynchronous onclose cannot schedule
  // a reconnect after polling was intentionally stopped.
  ws = null
  wsConnected.value = false
  wsSnapshotOffset = null
  wsBacklogBytes = null
  if (socket) {
    try { socket.close() } catch (e) {}
  }
}

const startPolling = async () => {
  stopPolling()
  await fetchLog()
  if (!logEnabled.value) return
  openLogStream()
  scheduleNextPoll()
}

const stopPolling = () => {
  if (pollTimer) {
    clearTimeout(pollTimer)
    pollTimer = null
  }
  closeLogStream()
}

watch(logEnabled, async (enabled) => {
  if (syncingFromBackend) return
  if (enabled) {
    logToggleBusy.value = true
    try {
      const response = await axios.post('/api/log/enable', {}, { timeout: 5000, silent: true })
      if (response.data?.enabled !== true) {
        throw new Error('Device could not allocate the log buffer')
      }
    } catch (e) {
      // Backend could not allocate the buffer - revert the toggle.
      logEnabled.value = false
      uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('systemlog.enableFailed') })
      logToggleBusy.value = false
      return
    }
    logToggleBusy.value = false
    offset.value = 0
    newEntriesCount.value = 0
    logEntries.value = []
    startPolling()
  } else {
    stopPolling()
    try {
      await axios.post('/api/log/disable', {}, { timeout: 5000, silent: true })
    } catch (e) {
      // Non-fatal: polling already stopped; backend keeps its current state.
    }
  }
})

const fetchLog = async () => {
  if (paused.value) return

  try {
    const response = await axios.get('/api/log', {
      params: { offset: offset.value },
      timeout: 5000,
      silent: true
    })
    if (response.data) {
      appendChunk(response.data)
      const totalWritten = parseInt(response.headers['x-log-total'])
      if (!isNaN(totalWritten)) {
        offset.value = totalWritten
      } else {
        // The device-side offset counts bytes - measure the chunk in UTF-8
        // bytes, not JS string characters, or multi-byte log content
        // desyncs the poll window.
        offset.value += new TextEncoder().encode(response.data).length
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
  const content = filteredEntries.value.slice(-MAX_COPY_LINES).map(e => e.raw).join('\n')
  if (!content) return
  try {
    await copyToClipboard(content)
    uiStore.pushToast({ type: 'success', title: t('common.success'), message: t('systemlog.copiedVisible'), duration: 1800 })
  } catch (error) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('systemlog.copyVisibleFailed') })
  }
}

const copyLine = async (line) => {
  try {
    await copyToClipboard(line)
    uiStore.pushToast({ type: 'success', title: t('common.success'), message: t('systemlog.copiedLine'), duration: 1400 })
  } catch (error) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('systemlog.copyLineFailed') })
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

onMounted(async () => {
  // Ask the device whether the in-memory log buffer is active. Once enabled,
  // the device persists that choice and restores capture after a reboot.
  try {
    const response = await axios.get('/api/log/status', { silent: true })
    syncingFromBackend = true
    logEnabled.value = !!response.data.enabled
    // Vue batches watchers until the next tick. Keep the guard set until the
    // watcher has observed this backend-driven assignment, otherwise both the
    // watcher and the explicit start below create a WebSocket.
    await nextTick()
    syncingFromBackend = false
  } catch (e) {
    syncingFromBackend = false
    logEnabled.value = false
  }
  if (logEnabled.value) {
    await startPolling()
  }

  // Check whether the device stored a log tail right before the last
  // watchdog/panic restart. The endpoint returns {available, tail} and
  // clears the snapshot after the first successful read, so this only
  // surfaces once after a real crash — not on every page load.
  try {
    const r = await axios.get('/api/crash_log', { silent: true })
    if (r.data && r.data.available && r.data.tail) {
      crashTail.value = r.data.tail
      showCrashModal.value = true
    }
  } catch (e) {
    // ignore — older firmware versions do not expose this endpoint
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
  min-width: 0;
}

.card-header {
  padding: var(--spacing-lg);
  background: transparent;
  border: none;
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: var(--spacing-md);
  min-width: 0;
}

.header-content {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  min-width: 0;
}

.header-content p {
  margin: 4px 0 0;
  color: var(--color-text-secondary);
  font-size: var(--fs-xs);
  overflow-wrap: anywhere;
}

.header-icon {
  width: 40px;
  height: 40px;
  border-radius: var(--radius-lg);
  display: flex;
  align-items: center;
  justify-content: center;
}

.card-header h3 {
  margin: 0;
  font-size: var(--fs-lg);
  font-weight: var(--font-weight-semibold);
  overflow-wrap: anywhere;
}

.log-actions {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  flex-wrap: wrap;
  gap: 10px;
  min-width: 0;
}

.toggle-chip {
  display: inline-flex;
  align-items: center;
  gap: var(--space-2);
  padding: var(--space-2) var(--space-3);
  border-radius: var(--radius-full);
  background: rgba(255, 255, 255, 0.52);
  border: 1px solid var(--color-border-light);
  max-width: 100%;
  overflow-wrap: anywhere;
}

.tool-btn {
  display: inline-flex;
  align-items: center;
  gap: var(--space-2);
  border: 1px solid var(--color-border-light);
  background: rgba(255, 255, 255, 0.6);
  border-radius: var(--radius-full);
  padding: var(--space-2) var(--space-3);
  color: var(--color-text);
  max-width: 100%;
  white-space: normal;
}

.tool-btn:disabled {
  opacity: 0.55;
}

.toolbar {
  padding: 0 var(--spacing-lg) var(--spacing-md);
  display: flex;
  gap: var(--space-3);
  flex-wrap: wrap;
  min-width: 0;
}

.search-field {
  flex: 1 1 280px;
  min-width: min(240px, 100%);
  display: flex;
  align-items: center;
  gap: 10px;
  padding: var(--space-3) 14px;
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
  padding: var(--space-3) 14px;
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
  min-width: 0;
}

.log-list {
  padding: 10px;
}

.log-line {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 10px;
  padding: var(--space-2) 10px;
  border-radius: var(--radius-lg);
  color: var(--color-text);
  min-width: 0;
}

.log-line:hover {
  background: var(--color-border-light);
}

.log-line code {
  color: inherit;
  white-space: pre-wrap;
  word-break: break-word;
  overflow-wrap: anywhere;
  font-family: var(--font-mono);
  font-size: var(--fs-xs);
  min-width: 0;
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
  flex: 0 0 auto;
}

.log-empty {
  padding: var(--space-5);
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
  font-size: var(--fs-sm);
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

  .log-actions > *,
  .tool-btn,
  .toggle-chip {
    width: 100%;
    justify-content: center;
  }

  .tool-btn {
    padding: 6px 10px;
    font-size: var(--fs-xs);
  }

  .log-line code {
    font-size: var(--fs-2xs);
  }
}

.crash-tail-pre {
  max-height: 60vh;
  overflow: auto;
  background: var(--color-bg-secondary);
  color: var(--color-text-inverse);
  padding: var(--space-3);
  border-radius: var(--radius-sm);
  font-family: var(--font-mono);
  font-size: var(--fs-xs);
  line-height: 1.35;
  white-space: pre-wrap;
  word-break: break-word;
  margin: 0;
}
</style>
