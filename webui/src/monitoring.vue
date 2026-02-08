<template>
  <div class="monitoring-page">
    <div class="page-header">
      <h3>{{ t('monitoring.title') }}</h3>
      <p class="text-secondary">{{ t('monitoring.description') }}</p>
    </div>

    <!-- SNMP Configuration -->
    <div class="settings-card">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-info-light text-info">📡</div>
          <h3>{{ t('monitoring.snmp.title') }}</h3>
        </div>
        <div class="form-check form-switch">
          <input class="form-check-input" type="checkbox" v-model="snmpConfig.enabled">
        </div>
      </div>

      <Transition name="expand">
        <div v-if="snmpConfig.enabled" class="card-body">
          <div class="row g-3">
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.snmp.port') }}</label>
              <BFormInput v-model.number="snmpConfig.port" type="number" min="1" max="65535" />
              <div class="form-text">{{ t('monitoring.snmp.portHelp') }}</div>
            </div>
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.snmp.community') }}</label>
              <BFormInput v-model="snmpConfig.community" />
              <div class="form-text">{{ t('monitoring.snmp.communityHelp') }}</div>
            </div>
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.snmp.location') }}</label>
              <BFormInput v-model="snmpConfig.location" />
              <div class="form-text">{{ t('monitoring.snmp.locationHelp') }}</div>
            </div>
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.snmp.contact') }}</label>
              <BFormInput v-model="snmpConfig.contact" />
              <div class="form-text">{{ t('monitoring.snmp.contactHelp') }}</div>
            </div>
          </div>
        </div>
      </Transition>
    </div>

    <!-- CheckMK Configuration -->
    <div class="settings-card">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-danger-light text-danger">🔍</div>
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

    <!-- MQTT Configuration -->
    <div class="settings-card">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-success-light text-success">🔄</div>
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

    <!-- Floating Action Bar for Save -->
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

    <!-- Success/Error Toasts -->
    <Transition name="fade">
      <div v-if="showSuccess" class="toast-overlay success">
        <div class="toast-card">
          <span class="toast-icon">✓</span>
          <span class="toast-message">{{ t('monitoring.saveSuccess') }}</span>
        </div>
      </div>
    </Transition>

    <Transition name="fade">
      <div v-if="showError" class="toast-overlay error">
        <div class="toast-card">
          <span class="toast-icon">⚠️</span>
          <span class="toast-message">{{ t('monitoring.saveError') }}</span>
          <button @click="showError = false" class="toast-close">✕</button>
        </div>
      </div>
    </Transition>

  </div>
</template>

<script setup>
import { ref, onMounted, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { useMonitoringStore } from './stores.js'
import { storeToRefs } from 'pinia'

const { t } = useI18n()

const monitoringStore = useMonitoringStore()
// Use refs to make them reactive but disconnected from store state until save?
// No, storeToRefs keeps them reactive. We modify store state directly.
// Ideally we should clone them to local state and only save on button press,
// but Pinia state is mutable. Let's keep it simple as per original implementation.
const { snmp: snmpConfig, checkmk: checkmkConfig, mqtt: mqttConfig } = storeToRefs(monitoringStore)

const showSuccess = ref(false)
const showError = ref(false)
const saving = ref(false)

onMounted(async () => {
  try {
    await monitoringStore.load()
  } catch (error) {
    // Error is logged in store
  }
})

const saveConfig = async () => {
  saving.value = true
  showSuccess.value = false
  showError.value = false

  try {
    // Force a small delay to show loading state
    await new Promise(resolve => setTimeout(resolve, 500))

    await monitoringStore.save({
      snmp: snmpConfig.value,
      checkmk: checkmkConfig.value,
      mqtt: mqttConfig.value
    })

    showSuccess.value = true
    setTimeout(() => { showSuccess.value = false }, 3000)
  } catch (error) {
    showError.value = true
  } finally {
    saving.value = false
  }
}
</script>

<style scoped>
.monitoring-page {
  padding-bottom: 80px;
  max-width: 800px;
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

/* Floating Footer */
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

/* Toast Messages */
.toast-overlay {
  position: fixed;
  top: var(--spacing-xl);
  left: 0;
  right: 0;
  display: flex;
  justify-content: center;
  z-index: 2000;
  pointer-events: none;
}

.toast-card {
  background: var(--color-surface);
  padding: var(--spacing-md) var(--spacing-xl);
  border-radius: var(--radius-full);
  box-shadow: var(--shadow-xl);
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  pointer-events: auto;
  min-width: 300px;
}

.success .toast-card {
  border-left: 4px solid var(--color-success);
}

.error .toast-card {
  border-left: 4px solid var(--color-danger);
}

.toast-icon {
  font-size: 1.25rem;
}

.success .toast-icon { color: var(--color-success); }
.error .toast-icon { color: var(--color-danger); }

.toast-message {
  font-weight: 600;
  flex: 1;
}

.toast-close {
  background: transparent;
  border: none;
  color: var(--color-text-secondary);
  cursor: pointer;
  font-size: 1rem;
}

/* Transitions */
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

.fade-enter-active,
.fade-leave-active {
  transition: opacity 0.3s ease, transform 0.3s ease;
}

.fade-enter-from,
.fade-leave-to {
  opacity: 0;
  transform: translateY(-20px);
}

/* Utility Colors */
.bg-info-light { background-color: var(--color-info-light); }
.bg-danger-light { background-color: var(--color-danger-light); }
.bg-success-light { background-color: var(--color-success-light); }
.text-info { color: var(--color-info); }
.text-danger { color: var(--color-danger); }
.text-success { color: var(--color-success); }

/* Form Text */
.form-text {
  font-size: 0.8125rem;
  color: var(--color-text-secondary);
  margin-top: 4px;
}

/* ===== Mobile Responsive ===== */
@media (max-width: 768px) {
  .monitoring-page {
    padding-bottom: 100px;
  }

  .page-header {
    margin-bottom: var(--spacing-lg);
  }

  .page-header h3 {
    font-size: 1.25rem;
  }

  .settings-card {
    border-radius: var(--radius-lg);
    margin-bottom: var(--spacing-md);
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

  .toast-card {
    min-width: 0;
    width: 90%;
    max-width: 300px;
    margin: 0 auto;
  }
}
</style>
