<template>
  <div class="page-shell dashboard">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow">
          <AppIcon name="dashboard" />
          {{ t('sysinfo.dashboardTitle') }}
        </span>
        <h1 class="hero-title">{{ sysInfoStore.hostname || 'HB-RF-ETH-ng' }}</h1>
        <p class="hero-subtitle">{{ t('sysinfo.system') }} · {{ t('sysinfo.network') }} · {{ t('sysinfo.radioModule') }}</p>
      </div>

      <div class="hero-meta">
        <span class="status-chip" :class="sysInfoStore.ethernetConnected ? 'success' : 'danger'">
          <AppIcon :name="sysInfoStore.ethernetConnected ? 'ethernet' : 'alert'" />
          {{ sysInfoStore.ethernetConnected ? t('sysinfo.online') : t('sysinfo.offline') }}
        </span>
        <span class="meta-chip">
          <AppIcon name="time" />
          {{ uptimeFormatted }}
        </span>
        <router-link
          class="meta-chip supporter-hero-chip"
          :class="{ 'is-active': sysInfoStore.supporterActive }"
          to="/settings"
          :title="sysInfoStore.supporterActive
            ? t('supporter.chipTooltip', { date: sysInfoStore.supporterExpiresAt || '—' })
            : t('supporter.chipInactiveTooltip')"
        >
          <AppIcon name="support" />
          {{ sysInfoStore.supporterActive ? t('supporter.chipActive') : t('supporter.chipInactive') }}
        </router-link>
      </div>
    </section>

    <div v-if="isLoading" class="stats-grid compact">
      <div v-for="index in 6" :key="index" class="card-glass skeleton-block skeleton"></div>
    </div>

    <template v-else>
      <section class="quick-actions">
        <router-link class="quick-action card-glass card-hover" to="/firmware">
          <span class="icon-badge warning"><AppIcon name="firmware" /></span>
          <div>
            <strong>{{ t('nav.firmware') }}</strong>
            <p>{{ sysInfoStore.latestVersion && sysInfoStore.latestVersion !== 'n/a' && updateStore.compareVersions(sysInfoStore.currentVersion, sysInfoStore.latestVersion) < 0 ? t('sysinfo.updateAvailableShort', { version: sysInfoStore.latestVersion }) : t('sysinfo.updatesRecovery') }}</p>
          </div>
        </router-link>

        <router-link class="quick-action card-glass card-hover" to="/systemlog">
          <span class="icon-badge soft"><AppIcon name="logs" /></span>
          <div>
            <strong>{{ t('nav.systemlog') }}</strong>
            <p>{{ t('sysinfo.liveStreamExport') }}</p>
          </div>
        </router-link>

        <router-link class="quick-action card-glass card-hover" to="/monitoring">
          <span class="icon-badge info"><AppIcon name="monitoring" /></span>
          <div>
            <strong>{{ t('nav.monitoring') }}</strong>
            <p>{{ t('sysinfo.monitoringSummary') }}</p>
          </div>
        </router-link>

        <router-link class="quick-action card-glass card-hover" to="/settings">
          <span class="icon-badge success"><AppIcon name="settings" /></span>
          <div>
            <strong>{{ t('nav.settings') }}</strong>
            <p>{{ t('sysinfo.networkTimeSetup') }}</p>
          </div>
        </router-link>
      </section>

      <section class="stats-grid">
        <article class="card-glass metric-card card-hover">
          <div class="metric-top">
            <div>
              <div class="metric-label">{{ t('sysinfo.cpuUsage') }}</div>
              <div class="metric-value">{{ safePercent(sysInfoStore.cpuUsage) }}%</div>
            </div>
            <span class="icon-badge soft"><AppIcon name="cpu" /></span>
          </div>
          <div class="metric-subtext">{{ t('sysinfo.system') }}</div>
          <div class="metric-progress info">
            <span :style="{ width: safePercent(sysInfoStore.cpuUsage) + '%' }"></span>
          </div>
        </article>

        <article class="card-glass metric-card card-hover">
          <div class="metric-top">
            <div>
              <div class="metric-label">{{ t('sysinfo.memoryUsage') }}</div>
              <div class="metric-value">{{ safePercent(sysInfoStore.memoryUsage) }}%</div>
            </div>
            <span class="icon-badge success"><AppIcon name="memory" /></span>
          </div>
          <div class="metric-subtext">{{ t('sysinfo.system') }}</div>
          <div class="metric-progress success">
            <span :style="{ width: safePercent(sysInfoStore.memoryUsage) + '%' }"></span>
          </div>
        </article>

        <article class="card-glass metric-card card-hover">
          <div class="metric-top">
            <div>
              <div class="metric-label">{{ t('sysinfo.ethernetStatus') }}</div>
              <div class="metric-value">{{ ethernetSummary }}</div>
            </div>
            <span class="icon-badge" :class="sysInfoStore.ethernetConnected ? 'info' : 'danger'">
              <AppIcon :name="sysInfoStore.ethernetConnected ? 'ethernet' : 'alert'" />
            </span>
          </div>
          <div class="metric-subtext">{{ ethernetStatus }}</div>
          <div class="status-chip" :class="sysInfoStore.ethernetConnected ? 'success' : 'danger'">
            <AppIcon :name="sysInfoStore.ethernetConnected ? 'check' : 'close'" />
            {{ sysInfoStore.ethernetConnected ? t('sysinfo.connected') : t('sysinfo.disconnected') }}
          </div>
        </article>

        <article v-if="loginStore.isLoggedIn" class="card-glass metric-card card-hover">
          <div class="metric-top">
            <div>
              <div class="metric-label">{{ t('sysinfo.radioModule') }}</div>
              <div class="metric-value smaller">{{ sysInfoStore.radioModuleType || '-' }}</div>
            </div>
            <span class="icon-badge warning"><AppIcon name="board" /></span>
          </div>
          <div class="metric-subtext">{{ sysInfoStore.radioModuleFirmwareVersion || '-' }}</div>
          <span class="pill-chip">{{ sysInfoStore.radioModuleSerial || '-' }}</span>
        </article>
      </section>

      <section class="info-grid">
        <article class="card-glass card-hover">
          <div class="card-section-title">
            <span class="icon-badge soft"><AppIcon name="board" /></span>
            <div>
              <h3>{{ t('sysinfo.system') }}</h3>
              <p>{{ t('sysinfo.serial') }} · {{ t('sysinfo.resetReason') }}</p>
            </div>
          </div>
          <div class="kv-list">
            <div class="kv-row">
              <span class="kv-label">{{ t('sysinfo.hostname') }}</span>
              <button class="copy-value" @click="copyValue(sysInfoStore.hostname, t('sysinfo.hostname'))">
                <span class="kv-value mono">{{ sysInfoStore.hostname || '-' }}</span>
                <AppIcon name="copy" />
              </button>
            </div>
            <div class="kv-row">
              <span class="kv-label">{{ t('sysinfo.serial') }}</span>
              <button class="copy-value" @click="copyValue(sysInfoStore.serial, t('sysinfo.serial'))">
                <span class="kv-value mono">{{ sysInfoStore.serial || '-' }}</span>
                <AppIcon name="copy" />
              </button>
            </div>
            <div class="kv-row">
              <span class="kv-label">{{ t('sysinfo.boardRevision') }}</span>
              <span class="kv-value">
                {{ sysInfoStore.boardRevision || '-' }}
                <span
                  v-if="sysInfoStore.boardRevision && sysInfoStore.boardRevision.startsWith('Unknown')"
                  class="board-sense-hint"
                  :title="t('sysinfo.boardSenseHint')"
                >
                  {{ sysInfoStore.boardSenseVoltage }} mV
                </span>
              </span>
            </div>
            <div class="kv-row">
              <span class="kv-label">{{ t('sysinfo.uptime') }}</span>
              <span class="kv-value">{{ uptimeFormatted }}</span>
            </div>
            <div class="kv-row">
              <span class="kv-label">{{ t('sysinfo.resetReason') }}</span>
              <span class="kv-value">{{ sysInfoStore.resetReason || '-' }}</span>
            </div>
          </div>
        </article>

        <article v-if="loginStore.isLoggedIn" class="card-glass card-hover">
          <div class="card-section-title">
            <span class="icon-badge info"><AppIcon name="network" /></span>
            <div>
              <h3>{{ t('sysinfo.network') }}</h3>
              <p>{{ t('sysinfo.ethernet') }} · {{ t('sysinfo.rawUartRemoteAddress') }}</p>
            </div>
          </div>
          <div class="kv-list">
            <div class="kv-row">
              <span class="kv-label">{{ t('sysinfo.ethernetStatus') }}</span>
              <span class="status-chip" :class="sysInfoStore.ethernetConnected ? 'success' : 'danger'">
                <AppIcon :name="sysInfoStore.ethernetConnected ? 'ethernet' : 'close'" />
                {{ ethernetStatus }}
              </span>
            </div>
            <div v-if="sysInfoStore.localIP" class="kv-row">
              <span class="kv-label">{{ t('sysinfo.localIP') }}</span>
              <button class="copy-value" @click="copyValue(sysInfoStore.localIP, t('sysinfo.localIP'))">
                <span class="kv-value mono">{{ sysInfoStore.localIP }}</span>
                <AppIcon name="copy" />
              </button>
            </div>
            <div v-if="sysInfoStore.netmask" class="kv-row">
              <span class="kv-label">{{ t('sysinfo.netmask') }}</span>
              <span class="kv-value mono">{{ sysInfoStore.netmask }}</span>
            </div>
            <div v-if="sysInfoStore.gateway" class="kv-row">
              <span class="kv-label">{{ t('sysinfo.gateway') }}</span>
              <button class="copy-value" @click="copyValue(sysInfoStore.gateway, t('sysinfo.gateway'))">
                <span class="kv-value mono">{{ sysInfoStore.gateway }}</span>
                <AppIcon name="copy" />
              </button>
            </div>
            <div v-if="sysInfoStore.dns1 || sysInfoStore.dns2" class="kv-row">
              <span class="kv-label">{{ t('sysinfo.dns') }}</span>
              <span class="kv-value mono">
                {{ sysInfoStore.dns1 || '-' }}<span v-if="sysInfoStore.dns2"> · {{ sysInfoStore.dns2 }}</span>
              </span>
            </div>
            <div v-if="sysInfoStore.ipv6Addresses && sysInfoStore.ipv6Addresses.length" class="kv-row ipv6-row">
              <span class="kv-label">{{ t('sysinfo.ipv6Addresses') }}</span>
              <div class="ipv6-list">
                <button v-for="addr in sysInfoStore.ipv6Addresses" :key="addr" class="copy-value" @click="copyValue(addr, t('sysinfo.ipv6Addresses'))">
                  <span class="kv-value mono">{{ addr }}</span>
                  <AppIcon name="copy" />
                </button>
              </div>
            </div>
            <div class="kv-row">
              <span class="kv-label">{{ t('sysinfo.rawUartRemoteAddress') }}</span>
              <button class="copy-value" :disabled="!sysInfoStore.rawUartRemoteAddress" @click="copyValue(sysInfoStore.rawUartRemoteAddress, t('sysinfo.rawUartRemoteAddress'))">
                <span class="kv-value mono">{{ sysInfoStore.rawUartRemoteAddress || '-' }}</span>
                <AppIcon name="copy" />
              </button>
            </div>
          </div>
        </article>

        <article v-if="loginStore.isLoggedIn" class="card-glass card-hover radio-card">
          <div class="card-section-title">
            <span class="icon-badge warning"><AppIcon name="firmware" /></span>
            <div>
              <h3>{{ t('sysinfo.radioModule') }}</h3>
              <p>{{ t('sysinfo.radioModuleFirmware') }} · {{ t('sysinfo.radioModuleSGTIN') }}</p>
            </div>
          </div>

          <div class="radio-grid">
            <div class="radio-stat">
              <span class="kv-label">{{ t('sysinfo.radioModuleType') }}</span>
              <strong>{{ sysInfoStore.radioModuleType || '-' }}</strong>
            </div>
            <div class="radio-stat">
              <span class="kv-label">{{ t('sysinfo.radioModuleFirmware') }}</span>
              <strong>{{ sysInfoStore.radioModuleFirmwareVersion || '-' }}</strong>
            </div>
            <div class="radio-stat wide">
              <span class="kv-label">{{ t('sysinfo.radioModuleSerial') }}</span>
              <button class="copy-value" :disabled="!sysInfoStore.radioModuleSerial" @click="copyValue(sysInfoStore.radioModuleSerial, t('sysinfo.radioModuleSerial'))">
                <span class="kv-value mono">{{ sysInfoStore.radioModuleSerial || '-' }}</span>
                <AppIcon name="copy" />
              </button>
            </div>
            <div class="radio-stat wide">
              <span class="kv-label">{{ t('sysinfo.radioModuleSGTIN') }}</span>
              <button class="copy-value" :disabled="!sysInfoStore.radioModuleSGTIN" @click="copyValue(sysInfoStore.radioModuleSGTIN, t('sysinfo.radioModuleSGTIN'))">
                <span class="kv-value mono">{{ sysInfoStore.radioModuleSGTIN || '-' }}</span>
                <AppIcon name="copy" />
              </button>
            </div>
            <div class="radio-stat wide">
              <span class="kv-label">{{ t('sysinfo.radioModuleBidCosRadioMAC') }}</span>
              <button class="copy-value" :disabled="!sysInfoStore.radioModuleBidCosRadioMAC" @click="copyValue(sysInfoStore.radioModuleBidCosRadioMAC, t('sysinfo.radioModuleBidCosRadioMAC'))">
                <span class="kv-value mono">{{ sysInfoStore.radioModuleBidCosRadioMAC || '-' }}</span>
                <AppIcon name="copy" />
              </button>
            </div>
            <div class="radio-stat wide">
              <span class="kv-label">{{ t('sysinfo.radioModuleHmIPRadioMAC') }}</span>
              <button class="copy-value" :disabled="!sysInfoStore.radioModuleHmIPRadioMAC" @click="copyValue(sysInfoStore.radioModuleHmIPRadioMAC, t('sysinfo.radioModuleHmIPRadioMAC'))">
                <span class="kv-value mono">{{ sysInfoStore.radioModuleHmIPRadioMAC || '-' }}</span>
                <AppIcon name="copy" />
              </button>
            </div>
          </div>
        </article>
      </section>
    </template>
  </div>
</template>

<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSysInfoStore, useUpdateStore, useUiStore, useLoginStore } from './stores.js'

const { t } = useI18n()
const sysInfoStore = useSysInfoStore()
const updateStore = useUpdateStore()
const uiStore = useUiStore()
const loginStore = useLoginStore()
const isLoading = ref(true)

const ethernetStatus = computed(() => {
  if (!sysInfoStore.ethernetConnected) return t('sysinfo.disconnected')
  return `${sysInfoStore.ethernetSpeed} ${t('sysinfo.mbits')} · ${sysInfoStore.ethernetDuplex}`
})

const ethernetSummary = computed(() => {
  if (!sysInfoStore.ethernetConnected) return t('sysinfo.down')
  return `${sysInfoStore.ethernetSpeed}M`
})

const uptimeFormatted = computed(() => {
  const seconds = sysInfoStore.uptimeSeconds || 0
  const days = Math.floor(seconds / 86400)
  const hours = Math.floor((seconds % 86400) / 3600)
  const minutes = Math.floor((seconds % 3600) / 60)
  const secs = seconds % 60

  if (days > 0) return `${days}d ${hours}h ${minutes}m`
  if (hours > 0) return `${hours}h ${minutes}m ${secs}s`
  return `${minutes}m ${secs}s`
})

const safePercent = (value) => Math.max(0, Math.min(100, Number(value || 0).toFixed(0)))

const copyValue = async (value, label) => {
  if (!value) return
  // execCommand runs synchronously within the click handler, so it still has
  // the user-gesture activation. Awaiting the async Clipboard API first can
  // consume that activation before falling back, breaking copy on HTTP pages.
  const textarea = document.createElement('textarea')
  textarea.value = value
  textarea.style.position = 'fixed'
  textarea.style.opacity = '0'
  document.body.appendChild(textarea)
  textarea.select()
  let ok = false
  try {
    ok = document.execCommand('copy')
  } catch {
    ok = false
  }
  document.body.removeChild(textarea)
  if (!ok) {
    if (navigator.clipboard) {
      await navigator.clipboard.writeText(value)
    } else {
      throw new Error(t('common.copyFailed'))
    }
  }
  uiStore.pushToast({
    type: 'success',
    title: t('common.success'),
    message: t('sysinfo.copied', { label }),
    duration: 2200
  })
}

let updateTimer = null

const startPolling = async () => {
  if (updateTimer) return
  try {
    await sysInfoStore.update()
  } catch (e) {
    console.warn('System info poll failed:', e.response?.status || e.message)
  } finally {
    isLoading.value = false
  }
  updateTimer = setInterval(() => {
    sysInfoStore.update().catch(() => { /* logged in the store; avoid unhandled rejection per tick */ })
  }, 5000)
}

const stopPolling = () => {
  if (updateTimer) {
    clearInterval(updateTimer)
    updateTimer = null
  }
}

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
  gap: var(--spacing-xl);
}

/* Inline badge showing the raw board-revision ADC voltage. Only renders when
 * boardRevision starts with "Unknown" so a clean detection stays visually
 * quiet. */
.board-sense-hint {
  display: inline-block;
  margin-left: 8px;
  padding: 1px 8px;
  border-radius: var(--radius-pill, 999px);
  background: var(--color-warning-soft, #fff3cd);
  color: var(--color-warning, #856404);
  font-size: 0.75rem;
  font-weight: 600;
  font-family: var(--font-mono, monospace);
  cursor: help;
}

.quick-actions {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 14px;
}

.quick-action {
  display: flex;
  align-items: center;
  gap: 14px;
  text-decoration: none;
  color: inherit;
  padding: 16px 18px;
}

.quick-action strong {
  display: block;
  font-size: 0.95rem;
}

.quick-action p {
  margin: 4px 0 0;
  color: var(--color-text-secondary);
  font-size: 0.82rem;
}

.metric-value.smaller {
  font-size: 1.2rem;
  line-height: 1.2;
}

.copy-value {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  border: none;
  background: transparent;
  color: var(--color-text);
  padding: 0;
}

.copy-value:disabled {
  opacity: 0.6;
}

.ipv6-row {
  flex-direction: column;
  align-items: flex-start;
  gap: 4px;
}

.ipv6-list {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.radio-card {
  grid-column: 1 / -1;
}

.radio-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  gap: 12px;
}

.radio-stat {
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding: 14px;
  border-radius: 18px;
  background: rgba(255, 255, 255, 0.44);
  border: 1px solid rgba(125, 139, 167, 0.12);
}

[data-bs-theme="dark"] .radio-stat {
  background: rgba(255, 255, 255, 0.03);
}

.radio-stat.wide {
  grid-column: span 2;
}

.radio-stat strong {
  font-size: 1rem;
}

@media (max-width: 768px) {
  .quick-actions {
    grid-template-columns: 1fr;
  }

  .radio-grid {
    grid-template-columns: 1fr;
  }

  .radio-stat.wide {
    grid-column: auto;
  }
}

/* Supporter status chip in the dashboard hero. Links to the settings page
   where the key can be entered. Two states: filled/warm when active, plain
   outline otherwise (acts as a call-to-action). */
.supporter-hero-chip {
  text-decoration: none;
  cursor: pointer;
  transition: background 0.2s, border-color 0.2s, color 0.2s;
}

.supporter-hero-chip .app-icon {
  color: var(--color-text-secondary);
}

.supporter-hero-chip.is-active {
  color: var(--color-primary-strong);
  background: var(--color-primary-soft);
  border-color: rgba(242, 106, 61, 0.32);
}

.supporter-hero-chip.is-active .app-icon {
  color: var(--color-primary-strong);
}

.supporter-hero-chip:hover {
  border-color: rgba(242, 106, 61, 0.45);
}

.supporter-hero-chip:not(.is-active):hover {
  color: var(--color-primary-strong);
  background: var(--color-primary-soft);
}
</style>
