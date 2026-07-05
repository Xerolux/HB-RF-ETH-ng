<template>
  <div id="app" :class="{ 'newdesign-shell': experimentalStore.testDesignEnabled }">
    <div class="app-container">
      <component :is="experimentalStore.testDesignEnabled ? NewDesignHeader : Header" />
      <main class="main-content">
        <RouterView v-slot="{ Component }">
          <Transition name="page" mode="out-in">
            <component :is="Component" />
          </Transition>
        </RouterView>
      </main>
      <footer class="app-footer">
        <div class="footer-content">
          <small class="text-muted">{{ t('app.footerCopyright', { version: sysInfoStore.currentVersion ? 'v' + sysInfoStore.currentVersion : '' }) }}</small>
          <div class="footer-actions">
            <a
              class="follow-x-btn"
              href="https://x.com/Xerolux"
              target="_blank"
              rel="noopener noreferrer"
              :aria-label="t('app.followOnX')"
              :title="t('app.followOnX')"
            >
              <AppIcon name="xSocial" />
              <span>{{ t('app.followOnX') }}</span>
            </a>
            <button class="sponsor-btn" @click="showSponsorModal = true">
              <AppIcon name="support" /> {{ t('app.sponsor') }}
            </button>
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
import { useExperimentalStore, useLoginStore, useSysInfoStore } from './stores.js'
import Header from './header.vue'
import NewDesignHeader from './components/NewDesignHeader.vue'
import SponsorModal from './components/SponsorModal.vue'
import AppToastContainer from './components/AppToastContainer.vue'

const { t } = useI18n()
const loginStore = useLoginStore()
const sysInfoStore = useSysInfoStore()
const experimentalStore = useExperimentalStore()
const showSponsorModal = ref(false)
const showUpdateSuccess = ref(false)
const otaUpdateVersion = ref('')
let updateSuccessTimer = null
const pageTitle = computed(() => `${sysInfoStore.hostname || 'HB-RF-ETH-ng'} - HB-RF-ETH-ng`)
const testDesignEnabled = computed(() => experimentalStore.testDesignEnabled)

// Idle timeout is handled globally in main.js via the login store's
// activity tracking (5-minute timeout with cross-tab sync via localStorage).

watch(pageTitle, (title) => {
  document.title = title
}, { immediate: true })

watch(testDesignEnabled, (enabled) => {
  document.body.classList.toggle('newdesign-active', enabled)
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
  document.body.classList.remove('newdesign-active')
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
  max-width: min(1080px, 94vw);
  width: 100%;
  margin: 0 auto;
  padding: var(--spacing-md);
  flex: 1;
  display: flex;
  flex-direction: column;
}

.newdesign-shell .app-container {
  max-width: none;
  margin: 0;
  padding: 0;
}

@media (min-width: 768px) {
  .app-container {
    padding: var(--spacing-lg);
  }
}

@media (max-width: 480px) {
  .app-container {
    padding: var(--spacing-sm);
  }
}

/* Safe area padding for notch devices */
@supports (padding: env(safe-area-inset-left)) {
  .app-container {
    padding-left: max(var(--spacing-sm), env(safe-area-inset-left));
    padding-right: max(var(--spacing-sm), env(safe-area-inset-right));
  }
}

.main-content {
  flex: 1;
  margin-bottom: var(--spacing-lg);
  /* Prevent content from overflowing horizontally on mobile */
  overflow-x: hidden;
}

.newdesign-shell .main-content {
  min-height: 100vh;
  margin-bottom: 0;
  padding: 112px 24px 24px 384px;
}

@media (max-width: 991px) {
  .newdesign-shell .main-content {
    padding: 96px 8px 24px;
  }
}

@media (max-width: 768px) {
  .main-content {
    margin-bottom: var(--spacing-md);
  }
}

.app-footer {
  padding: var(--spacing-lg) 0 var(--spacing-xl);
  /* border-top: 1px solid var(--color-border); */ /* Cleaner look without border */
  margin-top: auto;
}

.newdesign-shell .app-footer {
  padding: 8px 24px 18px 384px;
  margin-top: 0;
}

@media (max-width: 991px) {
  .newdesign-shell .app-footer {
    padding: 8px 8px 18px;
  }
}

@media (max-width: 768px) {
  .app-footer {
    padding: var(--spacing-md) 0 var(--spacing-lg);
  }
}

.footer-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: var(--spacing-md);
}

.sponsor-btn {
  background: var(--color-surface);
  border: 1px solid var(--color-border-light);
  padding: 8px 20px;
  border-radius: var(--radius-full);
  font-size: 0.875rem;
  font-weight: 600;
  color: var(--color-text-secondary);
  cursor: pointer;
  transition: all 0.2s;
  display: flex;
  align-items: center;
  gap: 8px;
  box-shadow: var(--shadow-sm);
}

.sponsor-btn:hover {
  background: var(--color-danger-soft);
  color: var(--color-danger);
  border-color: var(--color-danger);
  transform: translateY(-2px);
  box-shadow: var(--shadow-md);
}

.newdesign-shell .sponsor-btn {
  border-radius: var(--radius-sm);
  background: transparent;
  box-shadow: none;
}

/* Action row: holds the X follow link + sponsor button side-by-side */
.footer-actions {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: center;
  gap: var(--spacing-sm);
}

/* X / Twitter follow button — visually distinct from the sponsor button
 * (dark, neutral chip) so users immediately recognise it as an outgoing
 * social link instead of an in-app action. */
.follow-x-btn {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  padding: 8px 18px;
  border-radius: var(--radius-full);
  background: #000;
  color: #fff;
  border: 1px solid #000;
  font-size: 0.875rem;
  font-weight: 600;
  text-decoration: none;
  cursor: pointer;
  transition: all 0.2s;
  box-shadow: var(--shadow-sm);
}

.follow-x-btn:hover {
  background: #1a1a1a;
  color: #fff;
  transform: translateY(-2px);
  box-shadow: var(--shadow-md);
}

.follow-x-btn .app-icon {
  width: 16px;
  height: 16px;
}

.newdesign-shell .follow-x-btn {
  border-radius: var(--radius-sm);
  background: transparent;
  color: var(--color-text-secondary);
  border-color: var(--color-border-light);
  box-shadow: none;
}

.newdesign-shell .follow-x-btn:hover {
  background: var(--color-bg);
  color: var(--color-text);
  border-color: var(--color-border);
  transform: none;
  box-shadow: none;
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
