<template>
  <div id="app">
    <div class="app-container">
      <Header />
      <main class="main-content">
        <RouterView v-slot="{ Component }">
          <Transition name="page" mode="out-in">
            <component :is="Component" />
          </Transition>
        </RouterView>
      </main>
      <footer class="app-footer">
        <div class="footer-content">
          <small class="text-muted">HB-RF-ETH-ng {{ sysInfoStore.currentVersion ? 'v' + sysInfoStore.currentVersion : '' }} &copy; 2025-2026 Xerolux</small>
          <div class="footer-actions">
            <button class="sponsor-btn" @click="showSponsorModal = true">
              <AppIcon name="support" /> Sponsor
            </button>
            <a class="footer-social" href="https://x.com/Xerolux" target="_blank" rel="noopener noreferrer" aria-label="X">
              <AppIcon name="xSocial" />
            </a>
            <a class="footer-social" href="https://wa.me/" target="_blank" rel="noopener noreferrer" aria-label="WhatsApp">
              <AppIcon name="whatsapp" />
            </a>
          </div>
        </div>
      </footer>
    </div>

    <AppToastContainer />
    <SponsorModal v-model="showSponsorModal" />

    <BModal
      v-model="showUpdateSuccess"
      :title="t('updateSuccess.title')"
      :ok-title="t('common.ok')"
      cancel-title-class="d-none"
      @ok="showUpdateSuccess = false"
      @cancel="showUpdateSuccess = false"
      no-close-on-backdrop
      no-close-on-esc
      hide-header-close
    >
      <div class="update-success-body">
        <div class="update-success-icon"><AppIcon name="check" /></div>
        <p class="update-success-text">
          {{ t('updateSuccess.message', { version: otaUpdateVersion }) }}
        </p>
      </div>
    </BModal>
  </div>
</template>

<script setup>
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useLoginStore, useSysInfoStore } from './stores.js'
import Header from './header.vue'
import SponsorModal from './components/SponsorModal.vue'
import AppToastContainer from './components/AppToastContainer.vue'

const { t } = useI18n()
const loginStore = useLoginStore()
const sysInfoStore = useSysInfoStore()
const showSponsorModal = ref(false)
const showUpdateSuccess = ref(false)
const otaUpdateVersion = ref('')
let updateSuccessTimer = null
const pageTitle = computed(() => `${sysInfoStore.hostname || 'HB-RF-ETH-ng'} - HB-RF-ETH-ng`)

// Idle timeout is handled globally in main.js via the login store's
// activity tracking (5-minute timeout with cross-tab sync via localStorage).

watch(pageTitle, (title) => {
  document.title = title
}, { immediate: true })

onMounted(() => {
  sysInfoStore.update().catch((error) => {
    console.warn('Failed to load system info on app mount:', error)
  })

  // Check if we just came back from an OTA update
  const pendingVersion = localStorage.getItem('otaUpdateVersion')
  if (pendingVersion) {
    otaUpdateVersion.value = pendingVersion
    showUpdateSuccess.value = true
    localStorage.removeItem('otaUpdateVersion')
    // Auto-close after 10 seconds
    updateSuccessTimer = setTimeout(() => {
      showUpdateSuccess.value = false
    }, 10000)
  }
})

onUnmounted(() => {
  if (updateSuccessTimer) {
    clearTimeout(updateSuccessTimer)
  }
})
</script>

<style scoped>
#app {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
}

.app-container {
  max-width: none;
  width: 100%;
  margin: 0;
  padding: 0;
  flex: 1;
  display: flex;
  flex-direction: column;
}

@media (min-width: 768px) {
  .app-container {
    padding: 0;
  }
}

@media (max-width: 480px) {
  .app-container {
    padding: 0;
  }
}

/* Safe area padding for notch devices */
@supports (padding: env(safe-area-inset-left)) {
  .app-container {
    padding-left: 0;
    padding-right: 0;
  }
}

.main-content {
  flex: 1;
  min-height: 100vh;
  padding: 112px 24px 24px 384px;
  overflow-x: hidden;
}

@media (max-width: 991px) {
  .main-content {
    padding: 96px 8px 24px;
  }
}

.app-footer {
  padding: 8px 24px 18px 384px;
  margin-top: 0;
}

@media (max-width: 991px) {
  .app-footer {
    padding: 8px 8px 18px;
  }
}

.footer-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 10px;
}

.footer-actions {
  display: flex;
  align-items: center;
  gap: 14px;
}

.sponsor-btn {
  background: transparent;
  border: 1px solid var(--color-border-strong);
  padding: 8px 16px;
  border-radius: var(--radius-sm);
  font-size: 0.875rem;
  font-weight: 600;
  color: var(--color-text-secondary);
  cursor: pointer;
  transition: all 0.2s;
  display: flex;
  align-items: center;
  gap: 8px;
}

.sponsor-btn:hover {
  background: var(--color-primary-soft);
  color: var(--color-primary-strong);
  border-color: var(--color-primary);
}

.footer-social {
  width: 34px;
  height: 34px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  color: var(--color-text-muted);
  text-decoration: none;
  border-radius: var(--radius-sm);
  transition: color var(--transition-fast), background var(--transition-fast);
}

.footer-social:hover {
  color: var(--color-primary);
  background: var(--color-primary-soft);
}

.update-success-body {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: var(--spacing-md);
  padding: var(--spacing-md) 0;
  text-align: center;
}

.update-success-icon {
  width: 56px;
  height: 56px;
  border-radius: 50%;
  background: var(--color-success-soft, #d4edda);
  color: var(--color-success, #28a745);
  display: flex;
  align-items: center;
  justify-content: center;
}

.update-success-icon .app-icon {
  width: 28px;
  height: 28px;
}

.update-success-text {
  margin: 0;
  font-size: 1rem;
  color: var(--color-text);
}
</style>
