<template>
  <div class="dashboard">
    <!-- Status Cards Row -->
    <div class="status-cards">
      <BCard class="status-card status-card-primary">
        <div class="status-icon">⚡</div>
        <div class="status-content">
          <span class="status-label">{{ t('sysinfo.cpuUsage') }}</span>
          <span class="status-value">{{ sysInfoStore.cpuUsage.toFixed(1) }}%</span>
        </div>
        <div class="status-progress">
          <div class="progress-bar-bg" :style="{ width: sysInfoStore.cpuUsage + '%' }"></div>
        </div>
      </BCard>

      <BCard class="status-card status-card-success">
        <div class="status-icon">💾</div>
        <div class="status-content">
          <span class="status-label">{{ t('sysinfo.memoryUsage') }}</span>
          <span class="status-value">{{ sysInfoStore.memoryUsage.toFixed(1) }}%</span>
        </div>
        <div class="status-progress">
          <div class="progress-bar-bg progress-bar-success" :style="{ width: sysInfoStore.memoryUsage + '%' }"></div>
        </div>
      </BCard>

      <BCard class="status-card" :class="ethernetCardClass">
        <div class="status-icon">{{ ethernetIcon }}</div>
        <div class="status-content">
          <span class="status-label">{{ t('sysinfo.ethernet') || 'Ethernet' }}</span>
          <span class="status-value small">{{ ethernetStatusShort }}</span>
        </div>
      </BCard>
    </div>

    <!-- Main Info Cards -->
    <div class="info-grid">
      <!-- System Information -->
      <BCard class="info-card">
        <template #header>
          <div class="card-header-custom">
            <span class="card-icon">🖥️</span>
            <span>{{ t('sysinfo.system') || 'System' }}</span>
          </div>
        </template>
        <div class="info-list">
          <div class="info-item">
            <span class="info-label">{{ t('sysinfo.serial') }}</span>
            <span class="info-value">{{ sysInfoStore.serial }}</span>
          </div>
          <div class="info-item">
            <span class="info-label">{{ t('sysinfo.boardRevision') }}</span>
            <span class="info-value">{{ sysInfoStore.boardRevision }}</span>
          </div>
          <div class="info-item">
            <span class="info-label">{{ t('sysinfo.uptime') }}</span>
            <span class="info-value info-value-highlight">{{ uptimeFormatted }}</span>
          </div>
          <div class="info-item">
            <span class="info-label">{{ t('sysinfo.resetReason') }}</span>
            <span class="info-value">{{ sysInfoStore.resetReason }}</span>
          </div>
        </div>
      </BCard>

      <!-- Network Information -->
      <BCard class="info-card">
        <template #header>
          <div class="card-header-custom">
            <span class="card-icon">🌐</span>
            <span>{{ t('sysinfo.network') || 'Network' }}</span>
          </div>
        </template>
        <div class="info-list">
          <div class="info-item">
            <span class="info-label">{{ t('sysinfo.ethernetStatus') }}</span>
            <span class="info-value" :class="ethernetValueClass">
              <span class="status-dot" :class="ethernetDotClass"></span>
              {{ ethernetStatus }}
            </span>
          </div>
          <div class="info-item">
            <span class="info-label">{{ t('sysinfo.rawUartRemoteAddress') }}</span>
            <span class="info-value">{{ sysInfoStore.rawUartRemoteAddress || '-' }}</span>
          </div>
        </div>
      </BCard>

      <!-- Radio Module Information -->
      <BCard class="info-card info-card-full">
        <template #header>
          <div class="card-header-custom">
            <span class="card-icon">📻</span>
            <span>{{ t('sysinfo.radioModule') || 'Radio Module' }}</span>
          </div>
        </template>
        <div class="info-list info-list-grid">
          <div class="info-item">
            <span class="info-label">{{ t('sysinfo.radioModuleType') }}</span>
            <span class="info-value">{{ sysInfoStore.radioModuleType || '-' }}</span>
          </div>
          <div class="info-item">
            <span class="info-label">{{ t('sysinfo.radioModuleSerial') }}</span>
            <span class="info-value">{{ sysInfoStore.radioModuleSerial || '-' }}</span>
          </div>
          <div class="info-item">
            <span class="info-label">{{ t('sysinfo.radioModuleFirmware') }}</span>
            <span class="info-value">{{ sysInfoStore.radioModuleFirmwareVersion || '-' }}</span>
          </div>
          <div class="info-item">
            <span class="info-label">{{ t('sysinfo.radioModuleBidCosRadioMAC') }}</span>
            <span class="info-value">{{ sysInfoStore.radioModuleBidCosRadioMAC || '-' }}</span>
          </div>
          <div class="info-item">
            <span class="info-label">{{ t('sysinfo.radioModuleHmIPRadioMAC') }}</span>
            <span class="info-value">{{ sysInfoStore.radioModuleHmIPRadioMAC || '-' }}</span>
          </div>
          <div class="info-item">
            <span class="info-label">{{ t('sysinfo.radioModuleSGTIN') }}</span>
            <span class="info-value">{{ sysInfoStore.radioModuleSGTIN || '-' }}</span>
          </div>
        </div>
      </BCard>
    </div>
  </div>
</template>

<script setup>
import { onMounted, onBeforeUnmount, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSysInfoStore } from './stores.js'

const { t } = useI18n()
const sysInfoStore = useSysInfoStore()

// Ethernet status
const ethernetStatus = computed(() => {
  if (!sysInfoStore.ethernetConnected) return 'Disconnected'
  return `${sysInfoStore.ethernetSpeed} Mbit/s (${sysInfoStore.ethernetDuplex})`
})

const ethernetStatusShort = computed(() => {
  if (!sysInfoStore.ethernetConnected) return 'Down'
  return `${sysInfoStore.ethernetSpeed}M`
})

const ethernetIcon = computed(() => {
  return sysInfoStore.ethernetConnected ? '🔗' : '❌'
})

const ethernetCardClass = computed(() => {
  return sysInfoStore.ethernetConnected ? 'status-card-success' : 'status-card-danger'
})

const ethernetValueClass = computed(() => {
  return sysInfoStore.ethernetConnected ? 'text-success' : 'text-danger'
})

const ethernetDotClass = computed(() => {
  return sysInfoStore.ethernetConnected ? 'dot-success' : 'dot-danger'
})

// Uptime formatting
const uptimeFormatted = computed(() => {
  const seconds = sysInfoStore.uptimeSeconds
  const days = Math.floor(seconds / 86400)
  const hours = Math.floor((seconds % 86400) / 3600)
  const minutes = Math.floor((seconds % 3600) / 60)
  const secs = seconds % 60

  if (days > 0) {
    return `${days}d ${hours}h ${minutes}m`
  } else if (hours > 0) {
    return `${hours}h ${minutes}m ${secs}s`
  } else {
    return `${minutes}m ${secs}s`
  }
})

let updateTimer = null

const startPolling = () => {
  if (!updateTimer) {
    sysInfoStore.update()
    updateTimer = setInterval(() => {
      sysInfoStore.update()
    }, 1000)
  }
}

const stopPolling = () => {
  if (updateTimer) {
    clearInterval(updateTimer)
    updateTimer = null
  }
}

// Optimize performance by pausing polling when tab is hidden
const handleVisibilityChange = () => {
  if (document.hidden) {
    stopPolling()
  } else {
    startPolling()
  }
}

onMounted(() => {
  startPolling()
  document.addEventListener('visibilitychange', handleVisibilityChange)
})

onBeforeUnmount(() => {
  stopPolling()
  document.removeEventListener('visibilitychange', handleVisibilityChange)
})
</script>

<style scoped>
.dashboard {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-lg);
}

/* Status Cards */
.status-cards {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: var(--spacing-md);
}

.status-card {
  position: relative;
  border: none;
  box-shadow: var(--shadow-md);
  border-radius: var(--radius-lg);
  overflow: hidden;
  padding: var(--spacing-md);
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  transition: all var(--transition-normal);
}

.status-card:hover {
  transform: translateY(-2px);
  box-shadow: var(--shadow-lg);
}

.status-card :deep(.card-body) {
  padding: 0;
  display: contents;
}

.status-icon {
  font-size: 2rem;
  filter: drop-shadow(0 2px 4px rgba(0, 0, 0, 0.1));
}

.status-content {
  display: flex;
  flex-direction: column;
  flex: 1;
}

.status-label {
  font-size: 0.75rem;
  font-weight: 600;
  color: var(--color-text-secondary);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

.status-value {
  font-size: 1.5rem;
  font-weight: 700;
  color: var(--color-text);
}

.status-value.small {
  font-size: 1.125rem;
}

.status-progress {
  position: absolute;
  bottom: 0;
  left: 0;
  right: 0;
  height: 4px;
  background: var(--color-bg);
}

.progress-bar-bg {
  height: 100%;
  background: linear-gradient(90deg, var(--color-primary) 0%, var(--color-primary-dark) 100%);
  transition: width var(--transition-normal);
}

.progress-bar-success {
  background: linear-gradient(90deg, var(--color-success) 0%, #38a169 100%);
}

/* Info Grid */
.info-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: var(--spacing-md);
}

.info-card {
  border: 1px solid var(--color-border);
  border-radius: var(--radius-lg);
  box-shadow: var(--shadow-md);
  transition: box-shadow var(--transition-normal);
}

.info-card:hover {
  box-shadow: var(--shadow-lg);
}

.info-card :deep(.card-header) {
  background: var(--color-bg);
  border-bottom: 1px solid var(--color-border);
  padding: var(--spacing-md) var(--spacing-lg);
}

.info-card :deep(.card-body) {
  padding: var(--spacing-lg);
}

.card-header-custom {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
  font-weight: 600;
  color: var(--color-text);
}

.card-icon {
  font-size: 1.25rem;
}

.info-list {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-md);
}

.info-list-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: var(--spacing-md);
}

.info-item {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-xs);
}

.info-label {
  font-size: 0.8125rem;
  font-weight: 500;
  color: var(--color-text-secondary);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

.info-value {
  font-size: 0.9375rem;
  font-weight: 500;
  color: var(--color-text);
  word-break: break-all;
}

.info-value-highlight {
  color: var(--color-primary);
  font-weight: 600;
}

.text-success {
  color: var(--color-success) !important;
}

.text-danger {
  color: var(--color-danger) !important;
}

.status-dot {
  display: inline-block;
  width: 8px;
  height: 8px;
  border-radius: 50%;
  margin-right: var(--spacing-xs);
}

.dot-success {
  background-color: var(--color-success);
  box-shadow: 0 0 0 3px rgba(72, 187, 120, 0.2);
}

.dot-danger {
  background-color: var(--color-danger);
  box-shadow: 0 0 0 3px rgba(245, 101, 101, 0.2);
}

.info-card-full {
  grid-column: 1 / -1;
}

/* Mobile adjustments */
@media (max-width: 576px) {
  .status-cards {
    grid-template-columns: 1fr;
  }

  .info-grid {
    grid-template-columns: 1fr;
  }

  .info-list-grid {
    grid-template-columns: 1fr;
  }

  .status-value {
    font-size: 1.25rem;
  }
}
</style>
