<template>
  <header class="header-shell">
    <Transition name="slide-down">
      <div v-if="updateStore.shouldShowUpdateBadge && showBanner" class="update-banner glass-panel">
        <div class="update-banner-copy">
          <span class="hero-eyebrow">
            <AppIcon name="firmware" />
            {{ t('update.available') }}
          </span>
          <strong>v{{ updateStore.latestVersion }}</strong>
          <span>{{ t('update.updateNow') }}</span>
        </div>
        <div class="update-banner-actions">
          <BButton size="sm" variant="primary" to="/firmware" @click="mobileMenuOpen = false">
            <AppIcon name="download" />
            {{ t('update.updateNow') }}
          </BButton>
          <button class="icon-button" @click="dismissUpdate">
            <AppIcon name="close" />
          </button>
        </div>
      </div>
    </Transition>

    <nav class="header-nav glass-panel">
      <router-link to="/" class="brand" @click="closeMobileMenu">
        <span class="brand-mark">
          <AppIcon name="router" />
        </span>
        <span class="brand-copy">
          <strong>HB-RF-ETH-ng</strong>
          <small>{{ deviceName }}</small>
        </span>
      </router-link>

      <div class="desktop-nav" :aria-label="t('nav.mainMenu')">
        <div
          v-for="group in visibleNavGroups"
          :key="group.id"
          class="desktop-nav-group"
        >
          <span class="desktop-nav-group-label">{{ group.label }}</span>
          <router-link
            v-for="item in group.items"
            :key="item.to"
            :to="item.to"
            class="nav-item"
            active-class="active"
            exact-active-class="exact-active"
          >
            <AppIcon :name="item.icon" />
            <span>{{ item.label }}</span>
            <span v-if="item.to === '/firmware' && updateStore.shouldShowUpdateBadge" class="mini-dot"></span>
          </router-link>
        </div>
      </div>

      <div class="header-actions">
        <button v-if="updateStore.shouldShowUpdateBadge && !showBanner" class="icon-button" @click="showBanner = true">
          <AppIcon name="firmware" />
        </button>

        <div class="locale-picker desktop-only">
          <button class="icon-button locale-button" @click="localeOpen = !localeOpen">
            <AppIcon name="language" />
            <span>{{ currentLocale.toUpperCase() }}</span>
          </button>
          <Transition name="dropdown-fade">
            <div v-if="localeOpen" class="locale-menu glass-panel">
              <button
                v-for="loc in availableLocales"
                :key="loc.code"
                class="locale-menu-item"
                :class="{ active: loc.code === currentLocale }"
                @click="changeLocale(loc.code)"
              >
                <span>{{ loc.flag }}</span>
                <span>{{ loc.name }}</span>
              </button>
            </div>
          </Transition>
        </div>

        <button class="icon-button" :title="t('nav.toggleTheme')" @click="themeStore.toggleTheme">
          <AppIcon :name="themeStore.theme === 'light' ? 'moon' : 'sun'" />
        </button>

        <router-link v-if="!loginStore.isLoggedIn" to="/login" class="auth-button">{{ t('nav.login') }}</router-link>
        <template v-else>
          <button class="icon-button restart-button desktop-only" :title="t('firmware.restart')" :disabled="isRestarting" @click="showRestartModal = true">
            <AppIcon name="restart" />
          </button>
          <button class="icon-button logout-button desktop-only" :title="t('nav.logout')" @click="logout">
            <AppIcon name="logout" />
          </button>
        </template>

        <button class="icon-button mobile-menu-toggle mobile-only" @click="mobileMenuOpen = !mobileMenuOpen">
          <AppIcon :name="mobileMenuOpen ? 'close' : 'menu'" />
        </button>
      </div>
    </nav>

    <Transition name="mobile-menu">
      <div v-if="mobileMenuOpen" class="mobile-overlay" @click.self="closeMobileMenu">
        <div class="mobile-panel glass-panel">
          <div class="mobile-panel-top">
            <div class="mobile-device-title">
              <strong>HB-RF-ETH-ng</strong>
              <small>{{ deviceName }}</small>
            </div>
            <button class="icon-button" @click="closeMobileMenu">
              <AppIcon name="close" />
            </button>
          </div>

          <div class="mobile-links">
            <div
              v-for="group in visibleNavGroups"
              :key="group.id"
              class="mobile-link-group"
            >
              <div class="mobile-section-title">{{ group.label }}</div>
              <router-link
                v-for="item in group.items"
                :key="item.to"
                :to="item.to"
                class="mobile-link"
                @click="closeMobileMenu"
              >
                <AppIcon :name="item.icon" />
                {{ item.label }}
              </router-link>
            </div>
          </div>

          <div class="mobile-section">
            <div class="mobile-section-title">{{ t('nav.language') }}</div>
            <div class="mobile-locale-grid">
              <button
                v-for="loc in availableLocales"
                :key="loc.code"
                class="mobile-locale"
                :class="{ active: loc.code === currentLocale }"
                @click="changeLocale(loc.code)"
              >
                <span>{{ loc.flag }}</span>
                <span>{{ loc.code.toUpperCase() }}</span>
              </button>
            </div>
          </div>

          <template v-if="loginStore.isLoggedIn">
            <button class="mobile-restart" :disabled="isRestarting" @click="showRestartModal = true">
              <AppIcon name="restart" />
              {{ t('firmware.restart') }}
            </button>
            <button class="mobile-logout" @click="logout">
              <AppIcon name="logout" />
              {{ t('nav.logout') }}
            </button>
          </template>
          <router-link v-else to="/login" class="auth-button mobile-auth" @click="closeMobileMenu">
            {{ t('nav.login') }}
          </router-link>
        </div>
      </div>
    </Transition>

    <!-- Restart confirmation modal -->
    <BModal
      v-model="showRestartModal"
      :title="t('firmware.restart')"
      centered
      no-fade
      :ok-title="t('firmware.restart')"
      ok-variant="danger"
      :cancel-title="t('common.cancel')"
      @ok.prevent="performRestart"
      :ok-disabled="isRestarting"
      :cancel-disabled="isRestarting"
      no-close-on-backdrop
      no-close-on-esc
    >
      <p>{{ t('firmware.restartConfirm') }}</p>
      <BAlert
        v-if="settingsStore.flashPause"
        variant="info"
        :model-value="true"
        fade
        class="mt-2 mb-0"
      >
        {{ t('firmware.restartFlashPauseHint') }}
      </BAlert>
    </BModal>
  </header>
</template>

<script setup>
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import axios from 'axios'
import { useLoginStore, useThemeStore, useUpdateStore, useSysInfoStore, useUiStore, useSettingsStore, useRestartUiStore } from './stores.js'
import { availableLocales } from './locales/index.js'
import { useHeaderNavigation } from './composables/useHeaderNavigation.js'

const { t, locale } = useI18n()
const router = useRouter()
const loginStore = useLoginStore()
const themeStore = useThemeStore()
const updateStore = useUpdateStore()
const sysInfoStore = useSysInfoStore()
const uiStore = useUiStore()
const settingsStore = useSettingsStore()
const restartUiStore = useRestartUiStore()

const showBanner = ref(true)
const localeOpen = ref(false)
const mobileMenuOpen = ref(false)
const showRestartModal = ref(false)
const isRestarting = ref(false)
let updateCheckTimer = null

const currentLocale = computed(() => locale.value)
const deviceName = computed(() => sysInfoStore.hostname || 'HB-RF-ETH-ng')
const { visibleNavGroups } = useHeaderNavigation(t, loginStore)

watch(mobileMenuOpen, (isOpen) => {
  document.body.style.overflow = isOpen ? 'hidden' : ''
})

const closeMobileMenu = () => {
  mobileMenuOpen.value = false
}

const changeLocale = (newLocale) => {
  locale.value = newLocale
  localeOpen.value = false
  localStorage.setItem('locale', newLocale)
}

const logout = () => {
  closeMobileMenu()
  loginStore.logout()
  router.push('/login')
}

// Restart flow shared with the settings page: confirm, POST /api/restart,
// toast, then reload once the device has had time to reboot. The restart
// button lives next to logout in the header — but unlike logout it always
// asks for confirmation first so users don't reboot by accident.
const performRestart = async () => {
  if (isRestarting.value) return
  isRestarting.value = true
  try {
    await axios.post('/api/restart')
    showRestartModal.value = false
    closeMobileMenu()
    uiStore.pushToast({ type: 'info', title: t('common.success'), message: t('firmware.restartingText'), duration: 1200 })
    restartUiStore.start({ includeFlashPause: settingsStore.flashPause })
  } catch (e) {
    console.error('Restart request failed', e)
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('settings.restartError') })
    isRestarting.value = false
  }
}

const dismissUpdate = () => {
  showBanner.value = false
  localStorage.setItem('dismissedUpdate', updateStore.latestVersion)
}

onMounted(async () => {
  try {
    if (!sysInfoStore.currentVersion) {
      await sysInfoStore.update()
    }
    // /api/check_update requires auth - calling it logged out triggers a 401
    // that force-redirects visitors away from the public pages (/, /about).
    if (loginStore.isLoggedIn) {
      await updateStore.checkForUpdate(sysInfoStore.currentVersion, { cached: true })
    }
  } catch (e) {
    console.error('Failed to load sys info for update check', e)
  }

  if (loginStore.isLoggedIn) {
    try {
      await settingsStore.load()
    } catch (e) {
      console.error('Failed to load settings for restart sync state', e)
    }
  }

  if (localStorage.getItem('dismissedUpdate') === updateStore.latestVersion) {
    showBanner.value = false
  }

  updateCheckTimer = setInterval(() => {
    if (loginStore.isLoggedIn && sysInfoStore.currentVersion) {
      updateStore.checkForUpdate(sysInfoStore.currentVersion, { cached: true })
    }
  }, 24 * 60 * 60 * 1000)
})

onUnmounted(() => {
  document.body.style.overflow = ''
  if (updateCheckTimer) {
    clearInterval(updateCheckTimer)
  }
})
</script>

<style scoped>
.header-shell {
  position: relative;
  z-index: 1000;
  margin-bottom: var(--spacing-lg);
  display: flex;
  flex-direction: column;
  gap: var(--spacing-md);
}

.update-banner {
  position: relative;
  z-index: 1001;
  padding: 14px 18px;
  border-radius: 24px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 16px;
  box-shadow: var(--shadow-lg);
}

.header-nav {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 14px;
  padding: 12px 16px;
  border-radius: 28px;
  /* Prevent long hostnames from blowing out the nav and pushing menu items
     off-screen. The brand truncates at 180px, but without min-width:0 on
     the flex container itself, some browsers let the brand grow to its
     intrinsic content width before flex distribution kicks in. */
  min-width: 0;
  max-width: 100%;
  overflow: visible;
}

.brand {
  display: flex;
  align-items: center;
  gap: 12px;
  color: var(--color-text);
  text-decoration: none;
  min-width: 0;
}

.brand-mark {
  width: 46px;
  height: 46px;
  border-radius: 16px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  color: white;
  background: linear-gradient(135deg, var(--color-primary), var(--color-primary-strong));
  box-shadow: var(--shadow-sm);
}

.brand-copy {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.brand-copy strong {
  font-size: 1rem;
}

.brand-copy small {
  color: var(--color-text-secondary);
  font-size: 0.78rem;
  max-width: 180px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.desktop-nav {
  display: flex;
  align-items: center;
  justify-content: flex-start;
  gap: 6px;
  flex: 1;
  min-width: 0;
  flex-wrap: wrap;
  overflow: visible;
  padding: 5px;
  border: 1px solid var(--color-border);
  border-radius: 20px;
  background: rgba(255, 255, 255, 0.32);
  scrollbar-width: none;
}

.desktop-nav-group {
  display: inline-flex;
  align-items: center;
  gap: 3px;
  min-width: 0;
  padding: 3px;
  border-radius: 16px;
  background: rgba(255, 255, 255, 0.28);
}

.desktop-nav-group + .desktop-nav-group {
  border-left: 1px solid var(--color-border);
  padding-left: 8px;
}

.desktop-nav-group-label {
  padding: 0 7px;
  color: var(--color-text-muted);
  font-size: 0.66rem;
  font-weight: 800;
  letter-spacing: 0.08em;
  line-height: 1;
  text-transform: uppercase;
  white-space: nowrap;
}

.desktop-nav-group .nav-item {
  flex: 0 1 auto;
}

.desktop-nav::-webkit-scrollbar {
  display: none;
}

[data-bs-theme="dark"] .desktop-nav {
  background: rgba(255, 255, 255, 0.04);
}

[data-bs-theme="dark"] .desktop-nav-group {
  background: rgba(255, 255, 255, 0.035);
}

.nav-item {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 7px;
  min-height: 38px;
  padding: 8px 10px;
  border: 1px solid transparent;
  border-radius: 13px;
  color: var(--color-text-secondary);
  text-decoration: none;
  font-weight: 700;
  font-size: 0.82rem;
  line-height: 1;
  white-space: nowrap;
  transition: color var(--transition-fast), background var(--transition-fast), border-color var(--transition-fast), transform var(--transition-fast);
}

.nav-item .app-icon {
  width: 15px;
  height: 15px;
  flex: 0 0 auto;
}

.nav-item:hover {
  color: var(--color-text);
  background: rgba(255, 255, 255, 0.58);
  border-color: var(--color-border);
}

[data-bs-theme="dark"] .nav-item:hover {
  background: rgba(255, 255, 255, 0.08);
}

.nav-item.active,
.nav-item.exact-active {
  color: var(--color-primary-strong);
  background: linear-gradient(180deg, rgba(242, 106, 61, 0.18), rgba(242, 106, 61, 0.09));
  border-color: rgba(242, 106, 61, 0.22);
}

.nav-item.exact-active {
  box-shadow: inset 0 -2px 0 rgba(242, 106, 61, 0.32);
}

.mini-dot {
  width: 6px;
  height: 6px;
  border-radius: 999px;
  background: var(--color-warning);
}

.header-actions {
  display: flex;
  align-items: center;
  gap: 8px;
  min-width: 0;
}

.icon-button {
  width: 42px;
  height: 42px;
  border: none;
  border-radius: 14px;
  background: rgba(255, 255, 255, 0.42);
  color: var(--color-text);
  display: inline-flex;
  align-items: center;
  justify-content: center;
}

[data-bs-theme="dark"] .icon-button {
  background: rgba(255, 255, 255, 0.04);
}

.icon-button:hover {
  background: rgba(255, 255, 255, 0.65);
}

.auth-button,
.mobile-logout {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 10px;
  min-height: 44px;
  padding: 0 18px;
  border-radius: var(--radius-pill);
  text-decoration: none;
  font-weight: 700;
  border: none;
  max-width: 100%;
  min-width: 0;
  white-space: normal;
}

.auth-button {
  background: linear-gradient(135deg, var(--color-primary), var(--color-primary-strong));
  color: white;
}

.logout-button {
  color: var(--color-danger);
}

/* Restart icon-button uses the brand primary colour so users immediately see
 * it as "reboot" rather than the red "power off" it replaced. The red danger
 * colour stays reserved for the actual logout button next to it. */
.restart-button {
  color: var(--color-primary);
}

.restart-button:disabled {
  opacity: 0.55;
  cursor: not-allowed;
}

.locale-picker {
  position: relative;
}

.locale-button {
  width: auto;
  padding: 0 12px;
  gap: 8px;
}

.locale-menu {
  position: absolute;
  top: calc(100% + 10px);
  right: 0;
  width: min(220px, calc(100vw - 24px));
  padding: 10px;
  border-radius: 22px;
  max-height: min(420px, calc(100vh - 96px));
  overflow-y: auto;
  z-index: 1200;
}

.locale-menu-item {
  width: 100%;
  border: none;
  background: transparent;
  color: var(--color-text);
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 10px 12px;
  border-radius: 14px;
  text-align: left;
  min-width: 0;
  overflow-wrap: anywhere;
}

.locale-menu-item.active,
.locale-menu-item:hover {
  background: rgba(255, 255, 255, 0.5);
}

.update-banner-copy {
  display: flex;
  align-items: center;
  gap: 12px;
  flex-wrap: wrap;
  min-width: 0;
}

.update-banner-copy strong {
  font-size: 1rem;
}

.update-banner-copy span:last-child {
  color: var(--color-text-secondary);
}

.update-banner-actions {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
}

.mobile-only {
  display: none;
}

.mobile-overlay {
  position: fixed;
  inset: 0;
  z-index: 1200;
  background: rgba(10, 14, 20, 0.38);
  backdrop-filter: blur(8px);
  display: flex;
  justify-content: flex-end;
  padding: 12px;
}

.mobile-panel {
  width: min(340px, 100%);
  height: 100%;
  max-height: calc(100vh - 24px);
  min-height: 0;
  border-radius: 28px;
  padding: 16px;
  display: flex;
  flex-direction: column;
  gap: 18px;
  overflow-y: auto;
  overflow-x: hidden;
  overscroll-behavior: contain;
}

.mobile-panel-top {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
}

.mobile-device-title {
  min-width: 0;
  display: flex;
  flex-direction: column;
}

.mobile-device-title small {
  color: var(--color-text-secondary);
  font-size: 0.82rem;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.mobile-links,
.mobile-link-group,
.mobile-section {
  display: flex;
  flex-direction: column;
}

.mobile-links {
  gap: 14px;
}

.mobile-link-group,
.mobile-section {
  gap: 8px;
}

.mobile-link {
  display: flex;
  align-items: center;
  gap: 12px;
  min-height: 46px;
  padding: 0 14px;
  border-radius: 16px;
  color: var(--color-text);
  text-decoration: none;
  font-weight: 700;
  min-width: 0;
  overflow-wrap: anywhere;
}

.mobile-link.router-link-active {
  background: var(--color-primary-soft);
  color: var(--color-primary-strong);
}

.mobile-section-title {
  color: var(--color-text-secondary);
  font-size: 0.8rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  padding: 0 4px;
}

.mobile-locale-grid {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 8px;
}

.mobile-locale {
  min-height: 52px;
  border: none;
  border-radius: 16px;
  background: rgba(255, 255, 255, 0.34);
  color: var(--color-text);
  display: flex;
  flex-direction: column;
  justify-content: center;
  gap: 4px;
  font-weight: 700;
  min-width: 0;
  overflow-wrap: anywhere;
}

.mobile-locale.active {
  background: var(--color-primary-soft);
  color: var(--color-primary-strong);
}

.mobile-auth,
.mobile-logout,
.mobile-restart {
  width: 100%;
}

.mobile-logout {
  background: var(--color-danger-soft);
  color: var(--color-danger);
}

/* Mobile restart mirrors the logout styling but uses the primary colour so
 * the two actions stay visually distinct. */
.mobile-restart {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 10px;
  min-height: 44px;
  padding: 0 18px;
  border-radius: var(--radius-pill);
  border: none;
  font-weight: 700;
  background: var(--color-primary-soft);
  color: var(--color-primary-strong);
  white-space: normal;
}

.mobile-restart:disabled {
  opacity: 0.55;
  cursor: not-allowed;
}

.dropdown-fade-enter-active,
.dropdown-fade-leave-active,
.slide-down-enter-active,
.slide-down-leave-active,
.mobile-menu-enter-active,
.mobile-menu-leave-active {
  transition: all 0.22s ease;
}

.dropdown-fade-enter-from,
.dropdown-fade-leave-to,
.slide-down-enter-from,
.slide-down-leave-to {
  opacity: 0;
  transform: translateY(-8px);
}

.mobile-menu-enter-from,
.mobile-menu-leave-to {
  opacity: 0;
}

@media (max-width: 1200px) {
  .desktop-nav,
  .desktop-only {
    display: none;
  }

  .mobile-only {
    display: inline-flex;
  }

  .header-nav {
    padding: 10px 12px;
    border-radius: 24px;
  }

  .brand-copy small {
    display: none;
  }

  .update-banner {
    flex-direction: column;
    align-items: flex-start;
  }

  .update-banner-actions {
    width: 100%;
  }
}

</style>
