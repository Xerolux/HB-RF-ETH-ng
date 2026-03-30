<template>
  <div class="page-shell monitoring-page">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow">
          <AppIcon name="monitoring" />
          {{ t('monitoring.title') }}
        </span>
        <h1 class="hero-title">{{ t('monitoring.title') }}</h1>
        <p class="hero-subtitle">{{ t('monitoring.description') }}</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="activity" /> MQTT</span>
        <span class="meta-chip"><AppIcon name="logs" /> CheckMK</span>
      </div>
    </section>

    <section class="diagnostics-grid">
      <article v-for="item in diagnosticCards" :key="item.key" class="diagnostic-card card-glass">
        <div class="diagnostic-copy">
          <span class="icon-badge" :class="item.tone"><AppIcon :name="item.icon" /></span>
          <div>
            <h3>{{ item.title }}</h3>
            <p>{{ diagnosticState(item.key).message }}</p>
          </div>
        </div>
        <button class="tool-btn" type="button" :disabled="diagnosticBusy[item.key]" @click="runDiagnostic(item.key)">
          <span v-if="diagnosticBusy[item.key]" class="spinner-border spinner-border-sm me-2"></span>
          <AppIcon v-else name="refresh" />
          Test
        </button>
      </article>
    </section>

    <div class="settings-card card-glass">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-danger-light text-danger"><AppIcon name="search" /></div>
          <h3>{{ t('monitoring.checkmk.title') }}</h3>
        </div>
        <div class="form-check form-switch">
          <input class="form-check-input" type="checkbox" v-model="checkmkConfig.enabled">
        </div>
      </div>

      <Transition name="expand">
        <div v-if="checkmkConfig.enabled" class="card-body">
          <div class="row g-3">
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.checkmk.port') }}</label>
              <BFormInput v-model.number="checkmkConfig.port" type="number" min="1" max="65535" />
              <div class="form-text">{{ t('monitoring.checkmk.portHelp') }}</div>
            </div>
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.checkmk.allowedHosts') }}</label>
              <BFormInput v-model="checkmkConfig.allowedHosts" />
              <div class="form-text">{{ t('monitoring.checkmk.allowedHostsHelp') }}</div>
            </div>
          </div>
        </div>
      </Transition>
    </div>

    <div class="settings-card card-glass">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-success-light text-success"><AppIcon name="activity" /></div>
          <h3>{{ t('monitoring.mqtt.title') }}</h3>
        </div>
        <div class="form-check form-switch">
          <input class="form-check-input" type="checkbox" v-model="mqttConfig.enabled">
        </div>
      </div>

      <Transition name="expand">
        <div v-if="mqttConfig.enabled" class="card-body">
          <div class="row g-3">
            <div class="col-md-8">
              <label class="form-label">{{ t('monitoring.mqtt.server') }}</label>
              <BFormInput v-model="mqttConfig.server" required />
              <div class="form-text">{{ t('monitoring.mqtt.serverHelp') }}</div>
            </div>
            <div class="col-md-4">
              <label class="form-label">{{ t('monitoring.mqtt.port') }}</label>
              <BFormInput v-model.number="mqttConfig.port" type="number" min="1" max="65535" />
              <div class="form-text">{{ t('monitoring.mqtt.portHelp') }}</div>
            </div>
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.mqtt.user') }}</label>
              <BFormInput v-model="mqttConfig.user" />
              <div class="form-text">{{ t('monitoring.mqtt.userHelp') }}</div>
            </div>
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.mqtt.password') }}</label>
              <BFormInput v-model="mqttConfig.password" type="password" />
              <div class="form-text">{{ t('monitoring.mqtt.passwordHelp') }}</div>
            </div>
            <div class="col-12">
              <label class="form-label">{{ t('monitoring.mqtt.topicPrefix') }}</label>
              <BFormInput v-model="mqttConfig.topicPrefix" />
              <div class="form-text">{{ t('monitoring.mqtt.topicPrefixHelp') }}</div>
            </div>

            <div class="col-12 mt-4">
              <div class="d-flex justify-content-between align-items-center mb-2">
                <label class="form-label mb-0">{{ t('monitoring.mqtt.haDiscoveryEnabled') }}</label>
                <div class="form-check form-switch">
                  <input class="form-check-input" type="checkbox" v-model="mqttConfig.haDiscoveryEnabled">
                </div>
              </div>

              <Transition name="expand">
                <div v-if="mqttConfig.haDiscoveryEnabled" class="mt-2">
                  <label class="form-label">{{ t('monitoring.mqtt.haDiscoveryPrefix') }}</label>
                  <BFormInput v-model="mqttConfig.haDiscoveryPrefix" />
                  <div class="form-text">{{ t('monitoring.mqtt.haDiscoveryPrefixHelp') }}</div>
                </div>
              </Transition>
            </div>
          </div>
        </div>
      </Transition>
    </div>

    <Transition name="slide-up">
      <div class="floating-footer">
        <div class="footer-container">
          <BButton
            variant="primary"
            size="lg"
            @click="saveConfig"
            :disabled="saving"
            class="save-btn"
          >
            <span v-if="saving" class="spinner-border spinner-border-sm me-2"></span>
            {{ saving ? t('monitoring.saving') : t('monitoring.save') }}
          </BButton>
        </div>
      </div>
    </Transition>
  </div>
</template>

<script setup>
import { ref, onMounted, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { useMonitoringStore, useUiStore } from './stores.js'
import { storeToRefs } from 'pinia'

const { t } = useI18n()

const monitoringStore = useMonitoringStore()
const uiStore = useUiStore()
const { checkmk: checkmkConfig, mqtt: mqttConfig } = storeToRefs(monitoringStore)

const saving = ref(false)
const diagnosticBusy = ref({ checkmk: false, mqtt: false })

const diagnosticCards = computed(() => [
  { key: 'checkmk', title: 'CheckMK', icon: 'logs', tone: 'warning' },
  { key: 'mqtt', title: 'MQTT', icon: 'activity', tone: 'success' }
])

const diagnosticState = (target) => {
  const result = monitoringStore.diagnostics[target]
  if (!result) {
    return { ok: null, message: 'Run a quick self-test to verify the current configuration.' }
  }
  return result
}

onMounted(async () => {
  try {
    await monitoringStore.load()
  } catch (error) {
    // Error is logged in store
  }
})

const saveConfig = async () => {
  if (mqttConfig.value.enabled && !mqttConfig.value.server) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('monitoring.mqtt.serverRequired') })
    return
  }

  saving.value = true

  try {
    await monitoringStore.save({
      checkmk: checkmkConfig.value,
      mqtt: mqttConfig.value
    })

    uiStore.pushToast({
      type: 'success',
      title: t('common.success'),
      message: t('monitoring.saveSuccess')
    })
  } catch (error) {
    uiStore.pushToast({
      type: 'error',
      title: t('common.error'),
      message: error.response?.data?.error || t('monitoring.saveError')
    })
  } finally {
    saving.value = false
  }
}

const runDiagnostic = async (target) => {
  diagnosticBusy.value[target] = true
  try {
    const result = await monitoringStore.test(target)
    uiStore.pushToast({
      type: result.ok ? 'success' : 'warning',
      title: result.ok ? t('common.success') : 'Check finished',
      message: result.message,
      duration: 2800
    })
  } catch (error) {
    uiStore.pushToast({
      type: 'error',
      title: t('common.error'),
      message: error.response?.data?.error || 'Diagnostic request failed'
    })
  } finally {
    diagnosticBusy.value[target] = false
  }
}
</script>

<style scoped>
.monitoring-page {
  padding-bottom: 80px;
  max-width: 800px;
  margin: 0 auto;
}

.diagnostics-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  gap: var(--spacing-md);
  margin-bottom: var(--spacing-lg);
}

.diagnostic-card {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 16px;
  padding: 18px;
}

.diagnostic-copy {
  display: flex;
  gap: 12px;
  align-items: center;
}

.diagnostic-copy h3 {
  margin: 0;
  font-size: 1rem;
}

.diagnostic-copy p {
  margin: 4px 0 0;
  color: var(--color-text-secondary);
  font-size: 0.82rem;
}

.tool-btn {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  border: 1px solid var(--color-border-light);
  background: rgba(255, 255, 255, 0.68);
  border-radius: var(--radius-full);
  padding: 10px 14px;
  color: var(--color-text);
}

.settings-card {
  border-radius: var(--radius-xl);
  margin-bottom: var(--spacing-lg);
  overflow: hidden;
  transition: transform 0.2s, box-shadow 0.2s;
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

.card-body {
  padding: 0 var(--spacing-lg) var(--spacing-lg);
  border-top: 1px solid var(--color-border-light);
  margin-top: var(--spacing-xs);
  padding-top: var(--spacing-lg);
}

.floating-footer {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  padding: var(--spacing-md);
  background: linear-gradient(to top, var(--color-surface) 80%, transparent);
  display: flex;
  justify-content: center;
  z-index: 100;
  pointer-events: none;
}

.footer-container {
  width: 100%;
  max-width: 600px;
  pointer-events: auto;
}

.save-btn {
  width: 100%;
  box-shadow: var(--shadow-lg);
  border-radius: var(--radius-full);
  padding: 1rem;
  font-size: 1.1rem;
}

.expand-enter-active,
.expand-leave-active {
  transition: all 0.3s ease;
  max-height: 1000px;
  opacity: 1;
}

.expand-enter-from,
.expand-leave-to {
  max-height: 0;
  opacity: 0;
  margin: 0;
  padding-top: 0;
  padding-bottom: 0;
  overflow: hidden;
}

.slide-up-enter-active,
.slide-up-leave-active {
  transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);
}

.slide-up-enter-from,
.slide-up-leave-to {
  transform: translateY(100%);
}

.bg-danger-light { background-color: var(--color-danger-light); }
.bg-success-light { background-color: var(--color-success-light); }
.text-danger { color: var(--color-danger); }
.text-success { color: var(--color-success); }

.form-text {
  font-size: 0.8125rem;
  color: var(--color-text-secondary);
  margin-top: 4px;
}

@media (max-width: 768px) {
  .monitoring-page {
    padding-bottom: 100px;
  }

  .settings-card {
    border-radius: var(--radius-lg);
    margin-bottom: var(--spacing-md);
  }

  .diagnostics-grid {
    grid-template-columns: 1fr;
  }

  .diagnostic-card {
    flex-direction: column;
    align-items: stretch;
  }

  .card-header {
    padding: var(--spacing-md);
  }

  .header-icon {
    width: 36px;
    height: 36px;
    border-radius: 8px;
    font-size: 1rem;
  }

  .card-header h3 {
    font-size: 1rem;
  }

  .card-body {
    padding: 0 var(--spacing-md) var(--spacing-md);
    padding-top: var(--spacing-md);
  }

  .floating-footer {
    padding: var(--spacing-sm) var(--spacing-md);
  }

  .save-btn {
    font-size: 1rem;
    padding: 0.875rem;
  }
}
</style>
