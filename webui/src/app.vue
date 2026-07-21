<template>
  <div id="app" class="newdesign-shell" :class="{ 'bare-layout': isBareLayout }">
    <!-- Bare layout: full-screen pages (login) render standalone, no shell. -->
    <main v-if="isBareLayout" class="bare-main">
      <RouterView v-slot="{ Component }">
        <Transition name="page" mode="out-in">
          <component :is="Component" />
        </Transition>
      </RouterView>
    </main>

    <!-- Full app shell: sidebar header + content + footer. -->
    <div v-else class="app-container">
      <NewDesignHeader />
      <main class="main-content">
        <RouterView v-slot="{ Component }">
          <Transition name="page" mode="out-in">
            <component :is="Component" />
          </Transition>
        </RouterView>
      </main>
      <footer class="app-footer">
        <div class="footer-content">
          <small class="text-muted">Firmware v{{ sysInfoStore.currentVersion || '—' }} · WebUI v{{ webUiVersion }} © 2025-2026 Xerolux</small>
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

    <!-- Restart countdown overlay (from the restart UI store) + global overlays
         live OUTSIDE the layout branch so they work in both layouts: an OTA-
         success can land on the login page after reboot, toasts and the
         supporter prompt can surface anywhere, and the restart countdown may
         fire regardless of which page is active. -->
    <div v-if="restartUiStore.visible" class="countdown-overlay restart-countdown-overlay">
      <div class="countdown-card">
        <div class="spinner-container">
          <div class="spinner-ring"></div>
          <div class="spinner-icon"><AppIcon name="refresh" /></div>
        </div>
        <div v-if="restartUiStore.phaseTotal > 1" class="countdown-phase">
          <span>{{ restartUiStore.phaseIndex }}/{{ restartUiStore.phaseTotal }}</span>
        </div>
        <h2 class="countdown-title">{{ restartCountdownTitle }}</h2>
        <div class="countdown-value">{{ restartUiStore.countdown }}</div>
        <p class="countdown-text">{{ restartCountdownText }}</p>
        <div class="progress-track">
          <div class="progress-fill" :style="{ width: restartCountdownProgress + '%' }"></div>
        </div>
      </div>
    </div>

    <BModal
      v-model="showSupporterExpiredPrompt"
      :title="t('supporter.expiredPromptTitle')"
      :ok-title="t('supporter.expiredPromptSupport')"
      :cancel-title="t('supporter.expiredPromptLater')"
      ok-variant="primary"
      no-close-on-backdrop
      @ok="showSponsorModal = true"
      @cancel="showSupporterExpiredPrompt = false"
    >
      <div class="expired-prompt-body">
        <div class="expired-prompt-icon"><AppIcon name="heart" /></div>
        <p class="expired-prompt-text">{{ t('supporter.expiredPromptBody') }}</p>
      </div>
    </BModal>

    <AppToastContainer />
    <SponsorModal v-model="showSponsorModal" />

    <BModal
      v-model="showUpdateSuccess"
      :title="t('updateSuccess.title')"
      :ok-title="t('common.close')"
      ok-only
      @ok="showUpdateSuccess = false"
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
import { useRoute } from 'vue-router'
import { useI18n } from 'vue-i18n'
import axios from 'axios'
import { useLoginStore, useRestartUiStore, useSysInfoStore } from './stores.js'
import { safeLocal, safeSession } from './composables/useSafeStorage'
import NewDesignHeader from './components/NewDesignHeader.vue'
import SponsorModal from './components/SponsorModal.vue'
import AppToastContainer from './components/AppToastContainer.vue'

const { t } = useI18n()
const route = useRoute()
const loginStore = useLoginStore()
const sysInfoStore = useSysInfoStore()
const restartUiStore = useRestartUiStore()
const showSponsorModal = ref(false)
const showUpdateSuccess = ref(false)
const showSupporterExpiredPrompt = ref(false)
const otaUpdateVersion = ref('')
const webUiVersion = ref(typeof __WEBUI_VERSION__ !== 'undefined' ? __WEBUI_VERSION__ : 'unbekannt')
let updateSuccessTimer = null
const pageTitle = computed(() => `${sysInfoStore.hostname || 'HB-RF-ETH-ng'} - HB-RF-ETH-ng`)
// Routes flagged meta.bareLayout (currently /login) render without the app
// shell — standalone full-screen page instead of sidebar + content + footer.
// (Replaces the older isLoginScreen approach: meta.bareLayout is generic and
// declared on the route, not hard-coded to /login here.)
const isBareLayout = computed(() => !!route.meta.bareLayout)
// Restart-countdown overlay computeds (driven by the restart UI store). The
// overlay is rendered below for every layout.
const restartCountdownTitle = computed(() => (
  restartUiStore.phase === 'sync'
    ? t('settings.flashPause')
    : t('firmware.restarting')
))
const restartCountdownText = computed(() => (
  restartUiStore.phase === 'sync'
    ? t('firmware.restartFlashPauseHint')
    : t('firmware.restartingText')
))
const restartCountdownProgress = computed(() => {
  const duration = Math.max(restartUiStore.phaseDuration, 1)
  return Math.min(100, Math.max(0, ((duration - restartUiStore.countdown) / duration) * 100))
})

// Remind a returning supporter whose key has expired to renew — shown once
// per browser session, right after login / first sysinfo load. A gentle
// nudge toward re-supporting rather than a hard gate (no functionality is
// ever locked). Dismissing or opening the sponsor modal sets a sessionStorage
// flag so it won't pester again until the next real login session.
watch(() => [sysInfoStore.supporterExpired, loginStore.isLoggedIn], ([expired, loggedIn]) => {
  if (!expired || !loggedIn) return
  if (sysInfoStore.supporterActive) return
  if (safeSession.get('supporterPromptShown') === '1') return
  safeSession.set('supporterPromptShown', '1')
  showSupporterExpiredPrompt.value = true
})

// Idle timeout is handled globally in main.js via the login store's
// activity tracking (5-minute timeout with cross-tab sync via localStorage).

watch(pageTitle, (title) => {
  document.title = title
}, { immediate: true })

// Note: the `newdesign-active` body class is owned exclusively by the
// experimental store (useExperimentalStore.applyDesignClass) + an inline
// script in index.html for FOUC prevention. Do NOT toggle it here — a
// duplicate watcher previously disagreed with the store on which element
// holds the class (<html> vs <body>) and caused the toggle to feel flaky.

onMounted(() => {
  sysInfoStore.update().catch((error) => {
    console.warn('Failed to load system info on app mount:', error)
  })

  axios.get('/api/webui/status', { timeout: 5000, silent: true })
    .then(response => { webUiVersion.value = response.data?.effectiveVersion || response.data?.version || webUiVersion.value })
    .catch(() => {})

  // Check if we just came back from an OTA update
  const pendingVersion = safeLocal.get('otaUpdateVersion')
  if (pendingVersion) {
    otaUpdateVersion.value = pendingVersion
    showUpdateSuccess.value = true
    safeLocal.remove('otaUpdateVersion')
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
  /* dvh accounts for the iOS Safari dynamic toolbar; vh is the fallback. */
  min-height: 100vh;
  min-height: 100dvh;
  display: flex;
  flex-direction: column;
}

/* Bare layout (login): full-viewport standalone page, no shell. */
.bare-main {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-height: 100vh;
  min-height: 100dvh;
  width: 100%;
  min-width: 0;
}

.app-container {
  max-width: min(1080px, 94vw);
  width: 100%;
  margin: 0 auto;
  padding: var(--spacing-md);
  flex: 1;
  display: flex;
  flex-direction: column;
  min-width: 0;
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
  min-width: 0;
}

.newdesign-shell .main-content {
  min-height: 100vh;
  min-height: 100dvh;
  margin-bottom: 0;
  padding: var(--newdesign-content-top) var(--newdesign-content-gap) var(--newdesign-content-gap) var(--newdesign-sidebar-width);
}

@media (max-width: 991px) {
  .newdesign-shell .main-content {
    padding: var(--newdesign-content-top-mobile) 8px 24px;
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
  padding: 8px var(--newdesign-content-gap) 18px var(--newdesign-sidebar-width);
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
  min-width: 0;
  text-align: center;
}

.sponsor-btn {
  background: var(--color-surface);
  border: 1px solid var(--color-border-light);
  padding: 8px 20px;
  border-radius: var(--radius-full);
  font-size: var(--fs-xs);
  font-weight: 600;
  color: var(--color-text-secondary);
  cursor: pointer;
  transition: all 0.2s;
  display: flex;
  align-items: center;
  gap: 8px;
  box-shadow: var(--shadow-sm);
  max-width: 100%;
  white-space: normal;
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
  max-width: 100%;
  min-width: 0;
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
  font-size: var(--fs-xs);
  font-weight: 600;
  text-decoration: none;
  cursor: pointer;
  transition: all 0.2s;
  box-shadow: var(--shadow-sm);
  max-width: 100%;
  white-space: normal;
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
  font-size: var(--fs-md);
  color: var(--color-text);
  overflow-wrap: anywhere;
}

.expired-prompt-body {
  display: flex;
  align-items: center;
  gap: 16px;
  padding: 8px 0;
  min-width: 0;
}

.expired-prompt-icon {
  width: 52px;
  height: 52px;
  flex: 0 0 auto;
  border-radius: 50%;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  background: linear-gradient(135deg, #f26a3d, #ec4899);
  color: #fff;
}

.expired-prompt-text {
  margin: 0;
  color: var(--color-text-secondary);
  font-size: var(--fs-sm);
  line-height: 1.5;
  min-width: 0;
  overflow-wrap: anywhere;
}

.countdown-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0, 0, 0, 0.8);
  backdrop-filter: blur(8px);
  z-index: 9999;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: var(--spacing-md);
}

.countdown-card {
  text-align: center;
  color: white;
  width: min(92vw, 420px);
  padding: var(--spacing-xl);
  border-radius: var(--radius-lg);
  background: rgba(15, 23, 42, 0.42);
}

.spinner-container {
  position: relative;
  width: 80px;
  height: 80px;
  margin: 0 auto var(--spacing-lg);
}

.spinner-ring {
  position: absolute;
  inset: 0;
  border: 4px solid rgba(255, 255, 255, 0.1);
  border-top-color: white;
  border-radius: 50%;
  animation: spin 1s linear infinite;
}

.spinner-icon {
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  font-size: var(--fs-3xl);
  color: white;
  display: inline-flex;
}

.countdown-value {
  font-size: var(--fs-3xl);
  line-height: 1;
  font-weight: 800;
  margin: var(--spacing-md) 0;
}

.countdown-phase {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-width: 44px;
  height: 28px;
  padding: 0 10px;
  border-radius: var(--radius-full);
  background: rgba(255, 255, 255, 0.16);
  color: white;
  font-weight: 700;
  margin-bottom: var(--spacing-sm);
}

.countdown-title {
  margin: 0;
  color: white;
}

.countdown-text {
  color: rgba(255, 255, 255, 0.82);
  margin-bottom: var(--spacing-lg);
}

.progress-track {
  width: 100%;
  height: 6px;
  border-radius: var(--radius-full);
  background: rgba(255, 255, 255, 0.16);
  overflow: hidden;
}

.progress-fill {
  height: 100%;
  border-radius: inherit;
  background: white;
  transition: width 0.3s ease;
}

@keyframes spin {
  to {
    transform: rotate(360deg);
  }
}

.newdesign-shell .restart-countdown-overlay .spinner-container {
  border: none !important;
  background: transparent !important;
  box-shadow: none !important;
}

@media (max-width: 480px) {
  .expired-prompt-body {
    align-items: flex-start;
    flex-direction: column;
  }

  .footer-actions,
  .follow-x-btn,
  .sponsor-btn {
    width: 100%;
    justify-content: center;
  }
}
</style>
