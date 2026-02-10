<template>
  <div class="dashboard-container">
    <!-- Header Section -->
    <div class="dashboard-header animate-entry">
      <div class="header-text">
        <h1>{{ t('sysinfo.dashboardTitle') }}</h1>
      </div>
      <div class="status-indicator" :class="{ 'online': sysInfoStore.ethernetConnected }">
        <span class="indicator-dot"></span>
        <span class="indicator-text">{{ sysInfoStore.ethernetConnected ? t('sysinfo.online') : t('sysinfo.offline') }}</span>
      </div>
    </div>

    <!-- Status Widgets Row -->
    <div class="widgets-row">
      <!-- CPU Widget -->
      <div class="widget animate-entry" style="animation-delay: 0.1s">
        <div class="widget-top">
          <div class="widget-icon-bg primary">
            <span class="widget-icon">⚡</span>
          </div>
          <span class="widget-trend">{{ sysInfoStore.cpuUsage.toFixed(0) }}%</span>
        </div>
        <div class="widget-bottom">
          <span class="widget-label">{{ t('sysinfo.cpuUsage') }}</span>
          <div class="widget-progress">
            <div class="progress-fill primary" :style="{ width: sysInfoStore.cpuUsage + '%' }"></div>
          </div>
        </div>
      </div>

      <!-- Memory Widget -->
      <div class="widget animate-entry" style="animation-delay: 0.2s">
        <div class="widget-top">
          <div class="widget-icon-bg success">
            <span class="widget-icon">💾</span>
          </div>
          <span class="widget-trend">{{ sysInfoStore.memoryUsage.toFixed(0) }}%</span>
        </div>
        <div class="widget-bottom">
          <span class="widget-label">{{ t('sysinfo.memoryUsage') }}</span>
          <div class="widget-progress">
            <div class="progress-fill success" :style="{ width: sysInfoStore.memoryUsage + '%' }"></div>
          </div>
        </div>
      </div>

      <!-- Ethernet Widget -->
      <div class="widget animate-entry" style="animation-delay: 0.3s">
        <div class="widget-top">
          <div class="widget-icon-bg" :class="sysInfoStore.ethernetConnected ? 'info' : 'danger'">
            <span class="widget-icon">{{ sysInfoStore.ethernetConnected ? '🌐' : '❌' }}</span>
          </div>
          <span class="widget-trend small">{{ ethernetStatusShort }}</span>
        </div>
        <div class="widget-bottom">
          <span class="widget-label">{{ t('sysinfo.ethernet') }}</span>
          <div class="widget-status-text" :class="sysInfoStore.ethernetConnected ? 'text-success' : 'text-danger'">
            {{ sysInfoStore.ethernetConnected ? t('sysinfo.connected') : t('sysinfo.disconnected') }}
          </div>
        </div>
      </div>
    </div>

    <!-- Main Info Grid -->
    <div class="info-masonry">
      <!-- System Information -->
      <div class="info-card animate-entry" style="animation-delay: 0.4s">
        <div class="card-header-clean">
          <h3>{{ t('sysinfo.system') }}</h3>
          <div class="header-line"></div>
        </div>
        <div class="card-content">
          <div class="info-row">
            <span class="label">{{ t('sysinfo.serial') }}</span>
            <span class="value">{{ sysInfoStore.serial }}</span>
          </div>
          <div class="info-row">
            <span class="label">{{ t('sysinfo.boardRevision') }}</span>
            <span class="value">{{ sysInfoStore.boardRevision }}</span>
          </div>
          <div class="info-row">
            <span class="label">{{ t('sysinfo.uptime') }}</span>
            <span class="value highlight">{{ uptimeFormatted }}</span>
          </div>
          <div class="info-row">
            <span class="label">{{ t('sysinfo.resetReason') }}</span>
            <span class="value muted">{{ sysInfoStore.resetReason }}</span>
          </div>
        </div>
      </div>

      <!-- Network Information -->
      <div class="info-card animate-entry" style="animation-delay: 0.5s">
        <div class="card-header-clean">
          <h3>{{ t('sysinfo.network') }}</h3>
          <div class="header-line"></div>
        </div>
        <div class="card-content">
          <div class="info-row">
            <span class="label">{{ t('sysinfo.ethernetStatus') }}</span>
            <span class="value" :class="sysInfoStore.ethernetConnected ? 'text-success' : 'text-danger'">
              {{ ethernetStatus }}
            </span>
          </div>
          <div class="info-row">
            <span class="label">{{ t('sysinfo.rawUartRemoteAddress') }}</span>
            <span class="value">{{ sysInfoStore.rawUartRemoteAddress || '-' }}</span>
          </div>
        </div>
      </div>

      <!-- Radio Module Information -->
      <div class="info-card wide animate-entry" style="animation-delay: 0.6s">
        <div class="card-header-clean">
          <h3>{{ t('sysinfo.radioModule') }}</h3>
          <div class="header-line"></div>
        </div>
        <div class="card-content grid-3">
          <div class="info-block box">
            <span class="label">{{ t('sysinfo.radioModuleType') }}</span>
            <span class="value large">{{ sysInfoStore.radioModuleType || '-' }}</span>
          </div>
          <div class="info-block box">
            <span class="label">{{ t('sysinfo.radioModuleSerial') }}</span>
            <span class="value large">{{ sysInfoStore.radioModuleSerial || '-' }}</span>
          </div>
          <div class="info-block box">
            <span class="label">{{ t('sysinfo.radioModuleFirmware') }}</span>
            <span class="value large">{{ sysInfoStore.radioModuleFirmwareVersion || '-' }}</span>
          </div>
           <div class="info-block">
            <span class="label">{{ t('sysinfo.radioModuleSGTIN') }}</span>
            <span class="value monospace">{{ sysInfoStore.radioModuleSGTIN || '-' }}</span>
          </div>
          <div class="info-block">
            <span class="label">{{ t('sysinfo.radioModuleBidCosRadioMAC') }}</span>
            <span class="value monospace">{{ sysInfoStore.radioModuleBidCosRadioMAC || '-' }}</span>
          </div>
          <div class="info-block">
            <span class="label">{{ t('sysinfo.radioModuleHmIPRadioMAC') }}</span>
            <span class="value monospace">{{ sysInfoStore.radioModuleHmIPRadioMAC || '-' }}</span>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { onMounted, onBeforeUnmount, computed, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSysInfoStore } from './stores.js'

const { t } = useI18n()
const sysInfoStore = useSysInfoStore()

// Ethernet status
const ethernetStatus = computed(() => {
  if (!sysInfoStore.ethernetConnected) return t('sysinfo.disconnected')
  return `${sysInfoStore.ethernetSpeed} ${t('sysinfo.mbits')} (${sysInfoStore.ethernetDuplex})`
})

const ethernetStatusShort = computed(() => {
  if (!sysInfoStore.ethernetConnected) return t('sysinfo.down')
  return `${sysInfoStore.ethernetSpeed}M`
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
.dashboard-container {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-xl);
  max-width: 1200px;
  margin: 0 auto;
}

/* Animations */
@keyframes fadeInUp {
  from {
    opacity: 0;
    transform: translateY(20px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

.animate-entry {
  opacity: 0;
  animation: fadeInUp 0.6s cubic-bezier(0.16, 1, 0.3, 1) forwards;
}

/* Header */
.dashboard-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0 var(--spacing-sm);
}

.header-text h1 {
  font-size: 2rem;
  font-weight: 800;
  margin: 0;
  background: linear-gradient(135deg, var(--color-text) 0%, var(--color-text-secondary) 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}

.header-text p {
  margin: 0;
  color: var(--color-text-secondary);
  font-weight: 500;
}

.status-indicator {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 16px;
  background: var(--color-surface);
  border-radius: var(--radius-full);
  box-shadow: var(--shadow-sm);
  font-weight: 600;
  font-size: 0.875rem;
  color: var(--color-text-secondary);
}

.status-indicator.online {
  color: var(--color-success);
}

.indicator-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background-color: currentColor;
  box-shadow: 0 0 0 2px rgba(currentColor, 0.2);
}

/* Widgets Row */
.widgets-row {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
  gap: var(--spacing-lg);
}

.widget {
  background: var(--color-surface);
  border-radius: var(--radius-xl);
  padding: 24px;
  box-shadow: var(--shadow-md);
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  height: 160px;
  transition: transform 0.3s ease, box-shadow 0.3s ease;
  cursor: default;
}

.widget:hover {
  transform: translateY(-4px) scale(1.01);
  box-shadow: var(--shadow-lg);
}

.widget-top {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
}

.widget-icon-bg {
  width: 48px;
  height: 48px;
  border-radius: 16px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 1.5rem;
  box-shadow: var(--shadow-sm);
}

.widget-icon-bg.primary { background: linear-gradient(135deg, var(--color-primary-light), var(--color-surface)); color: var(--color-primary); }
.widget-icon-bg.success { background: linear-gradient(135deg, var(--color-success-light), var(--color-surface)); color: var(--color-success); }
.widget-icon-bg.info { background: linear-gradient(135deg, var(--color-info-light), var(--color-surface)); color: var(--color-info); }
.widget-icon-bg.danger { background: linear-gradient(135deg, var(--color-danger-light), var(--color-surface)); color: var(--color-danger); }

.widget-trend {
  font-size: 1.5rem;
  font-weight: 700;
  color: var(--color-text);
}

.widget-trend.small {
  font-size: 1.25rem;
}

.widget-bottom {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.widget-label {
  font-size: 0.8125rem;
  font-weight: 600;
  color: var(--color-text-secondary);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

.widget-progress {
  height: 6px;
  background-color: var(--color-bg);
  border-radius: var(--radius-full);
  overflow: hidden;
}

.progress-fill {
  height: 100%;
  border-radius: var(--radius-full);
  transition: width 0.5s ease;
}

.progress-fill.primary { background-image: linear-gradient(90deg, var(--color-primary), #ff8a5c); }
.progress-fill.success { background-image: linear-gradient(90deg, var(--color-success), #66d985); }

.widget-status-text {
  font-size: 0.9375rem;
  font-weight: 600;
}

/* Info Masonry */
.info-masonry {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(400px, 1fr));
  gap: var(--spacing-lg);
}

.info-card {
  background: var(--color-surface);
  border-radius: var(--radius-xl);
  box-shadow: var(--shadow-md);
  overflow: hidden;
  display: flex;
  flex-direction: column;
}

.info-card.wide {
  grid-column: 1 / -1;
}

.card-header-clean {
  padding: 24px 24px 16px;
}

.card-header-clean h3 {
  font-size: 1.25rem;
  font-weight: 700;
  margin: 0 0 8px 0;
  color: var(--color-text);
}

.header-line {
  width: 40px;
  height: 4px;
  background-color: var(--color-primary);
  border-radius: 2px;
}

.card-content {
  padding: 0 24px 24px;
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.info-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding-bottom: 12px;
  border-bottom: 1px solid var(--color-border-light);
}

.info-row:last-child {
  border-bottom: none;
  padding-bottom: 0;
}

.label {
  font-size: 0.9375rem;
  color: var(--color-text-secondary);
}

.value {
  font-weight: 600;
  color: var(--color-text);
  font-size: 1rem;
}

.value.highlight {
  color: var(--color-primary);
}

.value.muted {
  color: var(--color-text-muted);
  font-size: 0.875rem;
}

/* Grid for Radio Module */
.grid-3 {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 16px;
}

.info-block {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.info-block.box {
  background-color: var(--color-bg);
  padding: 16px;
  border-radius: var(--radius-lg);
}

.value.large {
  font-size: 1.25rem;
}

.value.monospace {
  font-family: 'SF Mono', 'Roboto Mono', monospace;
  font-size: 0.9375rem;
  background-color: var(--color-bg);
  padding: 4px 8px;
  border-radius: 6px;
  width: fit-content;
}

/* Responsive */
@media (max-width: 768px) {
  .dashboard-container {
    gap: var(--spacing-lg);
  }

  .dashboard-header {
    flex-direction: column;
    align-items: flex-start;
    gap: 12px;
    padding: 0;
  }

  .header-text h1 {
    font-size: 1.5rem;
  }

  .status-indicator {
    width: 100%;
    justify-content: center;
  }

  .widgets-row {
    grid-template-columns: 1fr 1fr;
    gap: var(--spacing-md);
  }

  .widget {
    padding: 16px;
    height: 130px;
  }

  .widget-icon-bg {
    width: 36px;
    height: 36px;
    border-radius: 10px;
    font-size: 1.125rem;
  }

  .widget-trend {
    font-size: 1.25rem;
  }

  .widget-trend.small {
    font-size: 1rem;
  }

  .widget-label {
    font-size: 0.75rem;
  }

  .info-masonry {
    grid-template-columns: 1fr;
    gap: var(--spacing-md);
  }

  .card-header-clean {
    padding: 16px 16px 12px;
  }

  .card-header-clean h3 {
    font-size: 1.125rem;
  }

  .card-content {
    padding: 0 16px 16px;
    gap: 12px;
  }

  .info-row {
    flex-direction: column;
    align-items: flex-start;
    gap: 4px;
    padding-bottom: 10px;
  }

  .grid-3 {
    grid-template-columns: 1fr;
    gap: 10px;
  }

  .info-block.box {
    padding: 12px;
  }

  .value.large {
    font-size: 1.125rem;
  }

  .value.monospace {
    font-size: 0.8125rem;
    word-break: break-all;
  }
}

@media (max-width: 600px) {
  .widgets-row {
    display: flex;
    overflow-x: auto;
    scroll-snap-type: x mandatory;
    padding-bottom: 1rem;
    margin: 0 -1rem;
    padding: 0 1rem 1rem 1rem;
    gap: var(--spacing-md);
  }

  .widget {
    min-width: 85%;
    scroll-snap-align: center;
    height: 140px;
    margin-bottom: 0;
    flex-direction: column;
    align-items: stretch;
  }

  .widget-top {
    flex-direction: row;
    align-items: flex-start;
  }
}
</style>
