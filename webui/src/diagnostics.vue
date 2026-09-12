<template>
  <div class="diagnostics-page page-shell">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow"><AppIcon name="radio" /> {{ t('diagnostics.eyebrow') }}</span>
        <h1 class="hero-title">{{ t('diagnostics.pageTitle') }}</h1>
        <p class="hero-subtitle">{{ t('diagnostics.pageDescription') }}</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip" :class="relay.sessionActive ? 'chip-ok' : 'chip-idle'">
          <AppIcon name="network" />
          {{ relay.sessionActive ? t('diagnostics.sessionActive') : t('diagnostics.sessionIdle') }}
        </span>
      </div>
    </section>

    <BAlert v-if="loadError" variant="danger" :model-value="true">{{ loadError }}</BAlert>
    <BAlert v-else-if="unsupported" variant="warning" :model-value="true">
      {{ t('diagnostics.unsupported') }}
    </BAlert>

    <!-- Throughput first: every failure count below is only readable against
         these totals. A drop count without a denominator says nothing. -->
    <section class="stat-card">
      <div class="card-header">
        <div class="header-icon bg-primary-light text-primary"><AppIcon name="network" /></div>
        <div class="header-text">
          <span class="kicker">{{ t('diagnostics.throughputKicker') }}</span>
          <h2>{{ t('diagnostics.throughputHeading') }}</h2>
          <p>{{ t('diagnostics.throughputHelp') }}</p>
        </div>
      </div>
      <div class="card-body stat-grid">
        <div class="stat">
          <span class="stat-label">{{ t('diagnostics.rxFrames') }}</span>
          <span class="stat-value">{{ num(relay.rxFrames) }}</span>
        </div>
        <div class="stat">
          <span class="stat-label">{{ t('diagnostics.txFrames') }}</span>
          <span class="stat-value">{{ num(relay.txFrames) }}</span>
        </div>
        <div class="stat">
          <span class="stat-label">{{ t('diagnostics.keepalives') }}</span>
          <span class="stat-value">{{ num(relay.keepalives) }}</span>
        </div>
        <div class="stat" :class="{ 'stat-warn': relay.drops > 0 }">
          <span class="stat-label">{{ t('diagnostics.drops') }}</span>
          <span class="stat-value">{{ num(relay.drops) }}<small v-if="dropRate"> ({{ dropRate }})</small></span>
        </div>
        <div class="stat" :class="{ 'stat-warn': txFailed > 0 }">
          <span class="stat-label">{{ t('diagnostics.txFailed') }}</span>
          <span class="stat-value">{{ num(txFailed) }}</span>
        </div>
      </div>
    </section>

    <section class="stat-card">
      <div class="card-header">
        <div class="header-icon bg-primary-light text-primary"><AppIcon name="clock" /></div>
        <div class="header-text">
          <span class="kicker">{{ t('diagnostics.latencyKicker') }}</span>
          <h2>{{ t('diagnostics.latencyHeading') }}</h2>
          <p>{{ t('diagnostics.latencyHelp') }}</p>
        </div>
      </div>
      <div class="card-body">
        <div class="stat-grid">
          <div class="stat" :class="{ 'stat-warn': relay.queueWaitMaxMs >= 100 }">
            <span class="stat-label">{{ t('diagnostics.queueWaitMax') }}</span>
            <span class="stat-value">{{ num(relay.queueWaitMaxMs) }} ms</span>
          </div>
          <div class="stat" :class="{ 'stat-warn': queueNearFull }">
            <span class="stat-label">{{ t('diagnostics.queueDepthMax') }}</span>
            <span class="stat-value">{{ num(relay.queueDepthMax) }} / {{ num(relay.queueCapacity) }}</span>
          </div>
          <div class="stat" :class="{ 'stat-warn': relay.waitOver100ms > 0 }">
            <span class="stat-label">{{ t('diagnostics.waitOver100ms') }}</span>
            <span class="stat-value">{{ num(relay.waitOver100ms) }}</span>
          </div>
          <div class="stat" :class="{ 'stat-warn': relay.waitOver1s > 0 }">
            <span class="stat-label">{{ t('diagnostics.waitOver1s') }}</span>
            <span class="stat-value">{{ num(relay.waitOver1s) }}</span>
          </div>
        </div>

        <div class="stat-grid secondary">
          <div class="stat">
            <span class="stat-label">{{ t('diagnostics.waitOver10ms') }}</span>
            <span class="stat-value">{{ num(relay.waitOver10ms) }}</span>
          </div>
          <div class="stat">
            <span class="stat-label">{{ t('diagnostics.lastRx') }}</span>
            <span class="stat-value">{{ lastRxText }}</span>
          </div>
        </div>

        <!-- The high-water marks never decay, so a peak set days ago would
             otherwise mask a currently healthy device. -->
        <p class="muted-text">{{ t('diagnostics.highWaterNote') }}</p>
        <div class="actions">
          <BButton variant="outline-secondary" :disabled="resetting || unsupported" @click="resetStats">
            <span v-if="resetting" class="spinner-border spinner-border-sm me-2"></span>
            <AppIcon v-else name="refresh" /> {{ t('diagnostics.resetButton') }}
          </BButton>
          <BButton variant="outline-secondary" :disabled="loading" @click="load">
            <AppIcon name="refresh" /> {{ t('diagnostics.reloadButton') }}
          </BButton>
        </div>
      </div>
    </section>

    <!-- The radio-module half of the bridge. Everything above measures the
         CCU side; an overflow here discards frames the module already
         delivered, so the CCU repeats them and the duty cycle climbs without
         a single CCU-side counter moving. -->
    <section class="stat-card">
      <div class="card-header">
        <div class="header-icon bg-primary-light text-primary"><AppIcon name="radio" /></div>
        <div class="header-text">
          <span class="kicker">{{ t('diagnostics.uartKicker') }}</span>
          <h2>{{ t('diagnostics.uartHeading') }}</h2>
          <p>{{ t('diagnostics.uartHelp') }}</p>
        </div>
      </div>
      <div class="card-body">
        <BAlert v-if="uartUnsupported" variant="warning" :model-value="true">
          {{ t('diagnostics.uartUnsupported') }}
        </BAlert>
        <template v-else>
          <div class="stat-grid">
            <div class="stat" :class="{ 'stat-warn': uartOverflows > 0 }">
              <span class="stat-label">{{ t('diagnostics.uartOverflows') }}</span>
              <span class="stat-value">{{ num(uartOverflows) }}</span>
            </div>
            <div class="stat" :class="{ 'stat-warn': uart.flushedBytes > 0 }">
              <span class="stat-label">{{ t('diagnostics.uartLostBytes') }}</span>
              <span class="stat-value">{{ num(uart.flushedBytes) }}</span>
            </div>
            <div class="stat" :class="{ 'stat-warn': uartLineErrors > 0 }">
              <span class="stat-label">{{ t('diagnostics.uartLineErrors') }}</span>
              <span class="stat-value">{{ num(uartLineErrors) }}</span>
            </div>
            <div class="stat" :class="{ 'stat-warn': uart.txErrors > 0 }">
              <span class="stat-label">{{ t('diagnostics.uartTxErrors') }}</span>
              <span class="stat-value">{{ num(uart.txErrors) }}</span>
            </div>
          </div>

          <div class="stat-grid secondary">
            <div class="stat" :class="{ 'stat-warn': uartBacklogNearFull }">
              <span class="stat-label">{{ t('diagnostics.uartBacklogMax') }}</span>
              <span class="stat-value">{{ num(uart.rxBacklogMax) }} / {{ num(uart.rxRingSize) }} B</span>
            </div>
            <div class="stat">
              <span class="stat-label">{{ t('diagnostics.uartRxFrames') }}</span>
              <span class="stat-value">{{ num(uart.rxFrames) }}</span>
            </div>
            <div class="stat">
              <span class="stat-label">{{ t('diagnostics.uartTxFrames') }}</span>
              <span class="stat-value">{{ num(uart.txFrames) }}</span>
            </div>
            <div class="stat">
              <span class="stat-label">{{ t('diagnostics.uartOversize') }}</span>
              <span class="stat-value">{{ num(uart.oversize) }}</span>
            </div>
          </div>

          <!-- Line errors broken down by kind. A break points at the reset
               line or the cable, a framing error at baud rate or timing;
               summed they cannot be told apart. The last column is the
               firmware's own module resets (three per boot are normal) and
               deliberately never highlighted. -->
          <div class="stat-grid secondary uart-line-breakdown">
            <div class="stat" :class="{ 'stat-warn': uart.breaks > 0 }">
              <span class="stat-label">{{ t('diagnostics.uartBreaks') }}</span>
              <span class="stat-value">{{ num(uart.breaks) }}</span>
            </div>
            <div class="stat" :class="{ 'stat-warn': uart.parityErrors > 0 }">
              <span class="stat-label">{{ t('diagnostics.uartParityErrors') }}</span>
              <span class="stat-value">{{ num(uart.parityErrors) }}</span>
            </div>
            <div class="stat" :class="{ 'stat-warn': uart.frameErrors > 0 }">
              <span class="stat-label">{{ t('diagnostics.uartFrameErrors') }}</span>
              <span class="stat-value">{{ num(uart.frameErrors) }}</span>
            </div>
            <div class="stat">
              <span class="stat-label">{{ t('diagnostics.uartResetLineEvents') }}</span>
              <span class="stat-value">{{ num(uart.resetLineEvents) }}</span>
            </div>
          </div>

          <p class="muted-text">{{ t('diagnostics.uartNote') }}</p>
          <p class="muted-text">{{ t('diagnostics.uartResetNote') }}</p>
        </template>
      </div>
    </section>

    <section class="stat-card">
      <div class="card-body">
        <p class="muted-text">{{ t('diagnostics.interpretHint') }}</p>
      </div>
    </section>
  </div>
</template>

<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import axios from 'axios'
import { useI18n } from 'vue-i18n'
import { useUiStore } from './stores.js'

const REFRESH_MS = 5000

const { t } = useI18n()
const uiStore = useUiStore()

const loading = ref(false)
const resetting = ref(false)
const loadError = ref('')
const unsupported = ref(false)
const relay = ref({
  queueWaitMaxMs: 0,
  queueDepthMax: 0,
  queueCapacity: 0,
  waitOver10ms: 0,
  waitOver100ms: 0,
  waitOver1s: 0,
  drops: 0,
  rxFrames: 0,
  txFrames: 0,
  keepalives: 0,
  txAllocFail: 0,
  txSendErrors: 0,
  sessionActive: false,
  lastRxAgeMs: -1
})

// Radio-module UART counters. Reported by firmware that carries the #447
// instrumentation; older builds omit the object entirely, which the
// uartUnsupported flag turns into a notice instead of a wall of zeros.
// resetLineEvents arrived one beta later and simply stays 0 on firmware that
// does not report it. The failure counters are windowed by the firmware: the
// reset button rebases them, the frame totals stay.
const uart = ref({
  fifoOverflows: 0,
  bufferFull: 0,
  oversize: 0,
  flushedBytes: 0,
  breaks: 0,
  parityErrors: 0,
  frameErrors: 0,
  resetLineEvents: 0,
  readTimeouts: 0,
  txErrors: 0,
  rxBacklogMax: 0,
  rxRingSize: 0,
  txRingSize: 0,
  rxFullThreshold: 0,
  rxFrames: 0,
  txFrames: 0,
  rxBytes: 0,
  txBytes: 0
})
const uartUnsupported = ref(false)
let timer = null

const num = value => new Intl.NumberFormat().format(Number(value) || 0)

// A drop count only means something next to the traffic it happened in.
const dropRate = computed(() => {
  const total = Number(relay.value.rxFrames) || 0
  const drops = Number(relay.value.drops) || 0
  if (!total || !drops) return ''
  const percent = (drops / total) * 100
  // Both branches go through the same formatter, so the decimal separator
  // does not switch between them depending on which one is taken.
  const fmt = new Intl.NumberFormat(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })
  return percent >= 0.01 ? `${fmt.format(percent)} %` : `< ${fmt.format(0.01)} %`
})

// Frames towards the CCU that this firmware failed to hand to the network -
// no pbuf, or lwIP refused the datagram. Both were silent before.
const txFailed = computed(
  () => (Number(relay.value.txAllocFail) || 0) + (Number(relay.value.txSendErrors) || 0)
)

const queueNearFull = computed(() => {
  const cap = Number(relay.value.queueCapacity) || 0
  return cap > 0 && Number(relay.value.queueDepthMax) >= cap * 0.75
})

// Both overflow kinds have the same consequence - the driver buffer is
// flushed and received frames are destroyed - so they are one number here.
const uartOverflows = computed(
  () => (Number(uart.value.fifoOverflows) || 0) + (Number(uart.value.bufferFull) || 0)
)

const uartLineErrors = computed(
  () => (Number(uart.value.breaks) || 0)
    + (Number(uart.value.parityErrors) || 0)
    + (Number(uart.value.frameErrors) || 0)
)

const uartBacklogNearFull = computed(() => {
  const cap = Number(uart.value.rxRingSize) || 0
  return cap > 0 && Number(uart.value.rxBacklogMax) >= cap * 0.75
})

const lastRxText = computed(() => {
  const age = Number(relay.value.lastRxAgeMs)
  if (!Number.isFinite(age) || age < 0) return '—'
  if (age < 1000) return `${age} ms`
  if (age < 60000) return `${(age / 1000).toFixed(1)} s`
  return `${Math.round(age / 60000)} min`
})

const load = async () => {
  loading.value = true
  try {
    const response = await axios.get(`/api/system/overview?t=${Date.now()}`, {
      timeout: 8000,
      silent: true
    })
    if (response.data?.ccuRelay) {
      relay.value = { ...relay.value, ...response.data.ccuRelay }
      unsupported.value = false
      loadError.value = ''
      // Firmware that has the CCU counters but not the radio-module ones is a
      // real combination during the beta, so this is checked separately.
      if (response.data?.radioUart) {
        uart.value = { ...uart.value, ...response.data.radioUart }
        uartUnsupported.value = false
      } else {
        uartUnsupported.value = true
      }
    } else {
      // Older firmware does not report these counters. Showing the initial
      // zeros would read as "everything is fine", which is the opposite of
      // what this page is for.
      unsupported.value = true
      uartUnsupported.value = true
      loadError.value = ''
    }
  } catch {
    loadError.value = t('diagnostics.loadError')
  } finally {
    loading.value = false
  }
}

const resetStats = async () => {
  resetting.value = true
  try {
    await axios.post('/api/system/relay-stats/reset', {}, { timeout: 8000 })
    uiStore.pushToast({ type: 'success', message: t('diagnostics.resetDone') })
    await load()
  } catch {
    uiStore.pushToast({ type: 'error', message: t('diagnostics.resetFailed'), duration: 6000 })
  } finally {
    resetting.value = false
  }
}

onMounted(() => {
  load()
  timer = setInterval(load, REFRESH_MS)
})
onBeforeUnmount(() => {
  if (timer) clearInterval(timer)
})
</script>

<style scoped>
.stat-card { background:var(--color-surface); border:1px solid var(--color-border); border-radius:var(--radius-lg); overflow:hidden; margin-bottom:var(--card-padding); }
.card-header { display:flex; gap:14px; align-items:flex-start; padding:var(--card-padding); border-bottom:1px solid var(--color-border); }
.header-icon { width:44px; height:44px; flex:0 0 auto; border-radius:var(--radius-md); display:flex; align-items:center; justify-content:center; }
.header-text { min-width:0; flex:1; }
.header-text h2 { margin:2px 0 0; font-size:var(--fs-lg); font-weight:var(--font-weight-semibold); }
.header-text p { margin:.35rem 0 0; color:var(--color-text-secondary); }
.kicker { color:var(--color-primary-strong); font-size:var(--fs-2xs); font-weight:var(--font-weight-heavy); text-transform:uppercase; letter-spacing:.04em; }
.card-body { padding:var(--card-padding); }
.stat-grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(180px,1fr)); gap:var(--space-4); }
.stat-grid.secondary { margin-top:var(--space-4); }
.stat { display:flex; flex-direction:column; gap:var(--space-1); }
.stat-label { color:var(--color-text-secondary); font-size:var(--fs-xs); }
.stat-value { font-size:var(--fs-xl); font-weight:var(--font-weight-semibold); font-family:var(--font-mono); }
.stat-value small { font-size:var(--fs-xs); font-weight:var(--font-weight-medium); color:var(--color-text-secondary); }
.stat-warn .stat-value { color:var(--color-warning-strong); }
.chip-ok { color:var(--color-success); }
.chip-idle { color:var(--color-text-secondary); }
.actions { display:flex; flex-wrap:wrap; gap:var(--space-3); margin-top:var(--space-4); }
.muted-text { color:var(--color-text-secondary); margin-top:var(--space-4); }
</style>
