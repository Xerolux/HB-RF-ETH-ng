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
  sessionActive: false,
  lastRxAgeMs: -1
})
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

const queueNearFull = computed(() => {
  const cap = Number(relay.value.queueCapacity) || 0
  return cap > 0 && Number(relay.value.queueDepthMax) >= cap * 0.75
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
    } else {
      // Older firmware does not report these counters. Showing the initial
      // zeros would read as "everything is fine", which is the opposite of
      // what this page is for.
      unsupported.value = true
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
