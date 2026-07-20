<template>
  <div class="page-shell system-overview-page">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow"><AppIcon name="cpu" /> {{ copy.eyebrow }}</span>
        <h1 class="hero-title">{{ copy.title }}</h1>
        <p class="hero-subtitle">{{ copy.subtitle }}</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="firmware" /> {{ data.idfVersion || '—' }}</span>
        <span class="meta-chip"><AppIcon name="board" /> {{ data.target || 'ESP32' }}</span>
        <span class="meta-chip" :class="data.webui?.valid ? 'success-chip' : 'warning-chip'">
          <AppIcon :name="data.webui?.valid ? 'check' : 'alert'" />
          {{ data.webui?.source || 'embedded' }}
        </span>
      </div>
    </section>

    <div v-if="loading" class="overview-grid">
      <div v-for="index in 6" :key="index" class="overview-card skeleton"></div>
    </div>

    <template v-else>
      <BAlert v-if="error" variant="danger" :model-value="true">{{ error }}</BAlert>

      <section class="overview-grid">
        <article class="overview-card">
          <span class="overview-label">{{ copy.freeHeap }}</span>
          <strong>{{ formatBytes(data.freeHeap) }}</strong>
          <small>{{ copy.currentAvailable }}</small>
        </article>
        <article class="overview-card">
          <span class="overview-label">{{ copy.minimumHeap }}</span>
          <strong>{{ formatBytes(data.minimumFreeHeap) }}</strong>
          <small>{{ copy.sinceBoot }}</small>
        </article>
        <article class="overview-card">
          <span class="overview-label">{{ copy.largestBlock }}</span>
          <strong>{{ formatBytes(data.largestFreeBlock) }}</strong>
          <small>{{ copy.contiguous }}</small>
        </article>
        <article class="overview-card">
          <span class="overview-label">{{ copy.flash }}</span>
          <strong>{{ formatBytes(data.flashBytes) }}</strong>
          <small>{{ copy.physicalFlash }}</small>
        </article>
      </section>

      <section class="detail-grid">
        <article class="detail-card">
          <div class="detail-heading">
            <span class="icon-badge soft"><AppIcon name="cpu" /></span>
            <div><h2>{{ copy.runtime }}</h2><p>{{ copy.runtimeHint }}</p></div>
          </div>
          <div class="kv-list">
            <div class="kv-row"><span>{{ copy.idf }}</span><strong class="mono">{{ data.idfVersion || '—' }}</strong></div>
            <div class="kv-row"><span>{{ copy.target }}</span><strong>{{ data.target || '—' }}</strong></div>
            <div class="kv-row"><span>{{ copy.cores }}</span><strong>{{ data.chipCores ?? '—' }}</strong></div>
            <div class="kv-row"><span>{{ copy.chipRevision }}</span><strong>{{ data.chipRevision ?? '—' }}</strong></div>
            <div class="kv-row"><span>{{ copy.internalHeap }}</span><strong>{{ formatBytes(data.freeInternalHeap) }}</strong></div>
          </div>
        </article>

        <article class="detail-card">
          <div class="detail-heading">
            <span class="icon-badge warning"><AppIcon name="firmware" /></span>
            <div><h2>{{ copy.partitions }}</h2><p>{{ copy.partitionsHint }}</p></div>
          </div>
          <div class="kv-list">
            <div class="kv-row"><span>{{ copy.runningPartition }}</span><strong class="mono">{{ data.runningPartition || '—' }}</strong></div>
            <div class="kv-row"><span>{{ copy.runningSize }}</span><strong>{{ formatBytes(data.runningPartitionSize) }}</strong></div>
            <div class="kv-row"><span>{{ copy.nextPartition }}</span><strong class="mono">{{ data.nextUpdatePartition || '—' }}</strong></div>
            <div class="kv-row"><span>{{ copy.nextSize }}</span><strong>{{ formatBytes(data.nextUpdatePartitionSize) }}</strong></div>
          </div>
        </article>

        <article class="detail-card">
          <div class="detail-heading">
            <span class="icon-badge info"><AppIcon name="globe" /></span>
            <div><h2>WebUI</h2><p>{{ copy.webuiHint }}</p></div>
          </div>
          <div class="kv-list">
            <div class="kv-row"><span>{{ copy.source }}</span><strong>{{ data.webui?.source || 'embedded' }}</strong></div>
            <div class="kv-row"><span>{{ copy.version }}</span><strong class="mono">{{ data.webui?.version || 'embedded' }}</strong></div>
            <div class="kv-row"><span>{{ copy.partitionSize }}</span><strong>{{ formatBytes(data.webui?.partitionBytes) }}</strong></div>
            <div class="kv-row"><span>{{ copy.used }}</span><strong>{{ formatBytes(data.webui?.usedBytes) }}</strong></div>
          </div>
        </article>

        <article class="detail-card">
          <div class="detail-heading">
            <span class="icon-badge success"><AppIcon name="logs" /></span>
            <div><h2>{{ copy.logs }}</h2><p>{{ copy.logsHint }}</p></div>
          </div>
          <div class="kv-list">
            <div class="kv-row"><span>{{ copy.capture }}</span><strong>{{ data.logs?.enabled ? copy.active : copy.inactive }}</strong></div>
            <div class="kv-row"><span>{{ copy.bufferSize }}</span><strong>{{ formatBytes(data.logs?.bufferBytes) }}</strong></div>
            <div class="kv-row"><span>{{ copy.available }}</span><strong>{{ formatBytes(data.logs?.availableBytes) }}</strong></div>
            <div class="kv-row"><span>{{ copy.totalStream }}</span><strong>{{ formatBytes(data.logs?.totalWritten) }}</strong></div>
            <div class="kv-row"><span>{{ copy.subscribers }}</span><strong>{{ data.logs?.subscribers ?? 0 }}</strong></div>
          </div>
        </article>
      </section>

      <div class="action-row">
        <BButton variant="outline-primary" :disabled="loading" @click="loadOverview">
          <AppIcon name="refresh" /> {{ copy.refresh }}
        </BButton>
      </div>
    </template>
  </div>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import axios from 'axios'

const { locale } = useI18n()
const loading = ref(true)
const error = ref('')
const data = ref({ webui: {}, logs: {} })

const translations = {
  de: {
    eyebrow: 'Diagnose', title: 'Systemübersicht',
    subtitle: 'Speicher, Flash, Partitionen, Logs und aktive Weboberfläche auf einen Blick.',
    freeHeap: 'Freier Heap', minimumHeap: 'Heap-Minimum', largestBlock: 'Größter Block', flash: 'Flash',
    currentAvailable: 'Aktuell verfügbar', sinceBoot: 'Niedrigster Wert seit Start', contiguous: 'Größte zusammenhängende Allokation', physicalFlash: 'Erkannter Flash-Speicher',
    runtime: 'Laufzeitumgebung', runtimeHint: 'ESP-IDF und Hardwaredaten', idf: 'ESP-IDF', target: 'Zielplattform', cores: 'CPU-Kerne', chipRevision: 'Chip-Revision', internalHeap: 'Interner Heap',
    partitions: 'OTA-Partitionen', partitionsHint: 'Aktive und nächste Firmware-Partition', runningPartition: 'Aktiv', runningSize: 'Aktive Größe', nextPartition: 'Nächstes Update', nextSize: 'Update-Größe',
    webuiHint: 'Quelle und Speicher des New Designs', source: 'Quelle', version: 'Version', partitionSize: 'Partitionsgröße', used: 'Belegt',
    logs: 'Log-Puffer', logsHint: 'Speicher und aktive Live-Streams', capture: 'Aufzeichnung', active: 'Aktiv', inactive: 'Inaktiv', bufferSize: 'Puffergröße', available: 'Verfügbar', totalStream: 'Gesamt geschrieben', subscribers: 'Subscriber', refresh: 'Aktualisieren'
  },
  en: {
    eyebrow: 'Diagnostics', title: 'System Overview',
    subtitle: 'Memory, flash, partitions, logs and the active web interface at a glance.',
    freeHeap: 'Free heap', minimumHeap: 'Minimum heap', largestBlock: 'Largest block', flash: 'Flash',
    currentAvailable: 'Currently available', sinceBoot: 'Lowest value since boot', contiguous: 'Largest contiguous allocation', physicalFlash: 'Detected flash storage',
    runtime: 'Runtime', runtimeHint: 'ESP-IDF and hardware information', idf: 'ESP-IDF', target: 'Target', cores: 'CPU cores', chipRevision: 'Chip revision', internalHeap: 'Internal heap',
    partitions: 'OTA partitions', partitionsHint: 'Running and next firmware partition', runningPartition: 'Running', runningSize: 'Running size', nextPartition: 'Next update', nextSize: 'Update size',
    webuiHint: 'New Design source and storage', source: 'Source', version: 'Version', partitionSize: 'Partition size', used: 'Used',
    logs: 'Log buffer', logsHint: 'Memory and active live streams', capture: 'Capture', active: 'Active', inactive: 'Inactive', bufferSize: 'Buffer size', available: 'Available', totalStream: 'Total written', subscribers: 'Subscribers', refresh: 'Refresh'
  }
}

const copy = computed(() => translations[locale.value] || translations.en)

const formatBytes = (value) => {
  const bytes = Number(value) || 0
  if (!bytes) return '—'
  if (bytes < 1024) return `${bytes} B`
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
  return `${(bytes / 1024 / 1024).toFixed(2)} MB`
}

const loadOverview = async () => {
  loading.value = true
  error.value = ''
  try {
    const response = await axios.get('/api/system/overview', { timeout: 8000 })
    data.value = response.data || { webui: {}, logs: {} }
  } catch (requestError) {
    error.value = requestError.response?.data || requestError.message || 'System overview unavailable'
  } finally {
    loading.value = false
  }
}

onMounted(loadOverview)
</script>

<style scoped>
.system-overview-page { display: flex; flex-direction: column; gap: 20px; }
.success-chip { color: var(--color-success); }
.warning-chip { color: var(--color-warning); }
.overview-grid { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 16px; }
.overview-card, .detail-card { border: 1px solid var(--color-border-light); background: var(--color-surface); box-shadow: var(--shadow-sm); }
.overview-card { min-height: 135px; border-radius: var(--radius-lg); padding: 20px; display: flex; flex-direction: column; gap: 8px; }
.overview-card strong { font-size: 1.5rem; }
.overview-card small, .overview-label { color: var(--color-text-secondary); }
.overview-label { font-size: .76rem; text-transform: uppercase; letter-spacing: .08em; font-weight: 800; }
.detail-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 16px; }
.detail-card { border-radius: var(--radius-xl); padding: 22px; min-width: 0; }
.detail-heading { display: flex; gap: 12px; align-items: flex-start; margin-bottom: 18px; }
.detail-heading h2 { margin: 0; font-size: 1.2rem; }
.detail-heading p { margin: 3px 0 0; color: var(--color-text-secondary); font-size: .88rem; }
.kv-list { display: flex; flex-direction: column; }
.kv-row { display: flex; justify-content: space-between; gap: 14px; padding: 11px 0; border-bottom: 1px solid var(--color-border-light); }
.kv-row:last-child { border-bottom: 0; }
.kv-row span { color: var(--color-text-secondary); }
.kv-row strong { text-align: right; overflow-wrap: anywhere; }
.mono { font-family: var(--font-mono, monospace); }
.action-row { display: flex; justify-content: flex-end; }
.skeleton { min-height: 135px; animation: pulse 1.4s ease-in-out infinite; }
@keyframes pulse { 0%,100% { opacity: .45; } 50% { opacity: .8; } }
@media (max-width: 1000px) { .overview-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); } .detail-grid { grid-template-columns: 1fr; } }
@media (max-width: 560px) { .overview-grid { grid-template-columns: 1fr; } .kv-row { flex-direction: column; gap: 4px; } .kv-row strong { text-align: left; } }
</style>
