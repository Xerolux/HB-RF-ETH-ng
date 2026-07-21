<template>
  <header class="header-shell">
    <Transition name="slide-down">
      <div v-if="updateStore.shouldShowUpdateBadge && showBanner" class="update-banner">
        <div class="update-banner-copy">
          <span class="hero-eyebrow">
            <AppIcon name="firmware" />
            {{ t('update.available') }}
          </span>
          <strong>v{{ updateStore.latestVersion }}</strong>
          <span>{{ t('update.updateNow') }}</span>
        </div>
        <div class="update-banner-actions">
          <BButton size="sm" variant="primary" to="/updates/firmware" @click="mobileMenuOpen = false">
            <AppIcon name="download" />
            {{ t('update.updateNow') }}
          </BButton>
          <button class="icon-button" @click="dismissUpdate">
            <AppIcon name="close" />
          </button>
        </div>
      </div>
    </Transition>

    <aside class="desktop-sidebar">
      <router-link to="/" class="brand" @click="closeMobileMenu">
        <BrandLogo :size="48" />
        <span class="brand-copy">
          <strong>HB-RF-ETH-ng</strong>
          <small>{{ deviceName }}</small>
        </span>
      </router-link>

      <nav class="side-nav" :aria-label="t('nav.mainMenu')">
        <div
          v-for="group in visibleNavGroups"
          :key="group.id"
          class="side-nav-group"
        >
          <div class="side-nav-heading">{{ group.label }}</div>
          <template v-for="item in group.items" :key="item.to">
            <a
              v-if="item.external"
              :href="item.to"
              class="nav-item nav-item-external"
              target="_blank"
              rel="noopener noreferrer"
              @click="closeMobileMenu"
            >
              <AppIcon :name="item.icon" />
              <span>{{ item.label }}</span>
              <AppIcon name="externalLink" class="external-hint" />
            </a>
            <router-link
              v-else
              :to="item.to"
              class="nav-item"
              active-class="active"
              @click="closeMobileMenu"
            >
              <AppIcon :name="item.icon" />
              <span>{{ item.label }}</span>
              <span v-if="item.to === '/updates/firmware' && updateStore.shouldShowUpdateBadge" class="mini-dot"></span>
            </router-link>
          </template>
        </div>
      </nav>

      <div class="sidebar-footer">
        <div class="locale-picker" ref="localePickerRef">
          <button class="utility-row" type="button" @click="localeOpen = !localeOpen">
            <AppIcon name="language" />
            <span>{{ currentLocale.toUpperCase() }}</span>
            <AppIcon name="chevronDown" />
          </button>
          <Transition name="dropdown-fade">
            <div v-if="localeOpen" class="locale-menu">
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
        <router-link v-if="!loginStore.isLoggedIn" to="/login" class="utility-row">{{ t('nav.login') }}</router-link>
        <template v-else>
          <button class="utility-row restart-row" type="button" :disabled="isRestarting" @click="showRestartModal = true">
            <AppIcon name="restart" />
            <span>{{ t('firmware.restart') }}</span>
          </button>
          <button class="utility-row logout-row" type="button" @click="logout">
            <AppIcon name="logout" />
            <span>{{ t('nav.logout') }}</span>
          </button>
        </template>
      </div>
    </aside>

    <nav class="header-nav">
      <router-link to="/" class="mobile-brand" @click="closeMobileMenu">
        <BrandLogo :size="34" />
        <span class="mobile-title">{{ deviceName }}</span>
      </router-link>

      <div class="top-status">
        <span class="connection-label">{{ t('sysinfo.ethernetStatus') }}</span>
        <span class="status-pill" :class="sysInfoStore.ethernetConnected ? 'online' : 'offline'">
          {{ sysInfoStore.ethernetConnected ? t('sysinfo.lan') : t('sysinfo.offline') }}
        </span>
        <span class="top-separator"></span>
        <span>{{ t('sysinfo.uptime') }}: <strong>{{ uptimeShort }}</strong></span>
        <router-link
          v-if="sysInfoStore.supporterActive"
          to="/settings?tab=license"
          class="supporter-chip active"
          :title="t('supporter.chipTooltip', { date: sysInfoStore.supporterExpiresAt || '—' })"
        >
          <AppIcon name="heart" />
          <span>{{ t('supporter.chipActive') }}</span>
        </router-link>
        <router-link
          v-else
          to="/settings?tab=license"
          class="supporter-chip inactive"
          :title="t('supporter.chipInactiveTooltip')"
        >
          <AppIcon name="coffee" />
          <span>{{ t('supporter.chipInactive') }}</span>
        </router-link>
        <span class="clock-chip"><AppIcon name="clock" /> {{ currentTime }}</span>
      </div>

      <div class="header-actions">
        <button v-if="updateStore.shouldShowUpdateBadge && !showBanner" class="icon-button" @click="showBanner = true">
          <AppIcon name="firmware" />
        </button>
        <button class="icon-button" :title="t('nav.toggleTheme')" @click="themeStore.toggleTheme">
          <AppIcon :name="themeStore.theme === 'light' ? 'moon' : 'sun'" />
        </button>
        <button class="icon-button mobile-menu-toggle" @click="mobileMenuOpen = !mobileMenuOpen">
          <AppIcon :name="mobileMenuOpen ? 'close' : 'menu'" />
        </button>
      </div>
    </nav>

    <Transition name="mobile-menu">
      <div v-if="mobileMenuOpen" class="mobile-overlay" @click.self="closeMobileMenu">
        <div class="mobile-panel">
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
              <template v-for="item in group.items" :key="item.to">
                <a
                  v-if="item.external"
                  :href="item.to"
                  class="mobile-link mobile-link-external"
                  target="_blank"
                  rel="noopener noreferrer"
                  @click="closeMobileMenu"
                >
                  <AppIcon :name="item.icon" />
                  <span>{{ item.label }}</span>
                  <AppIcon name="externalLink" class="external-hint" />
                </a>
                <router-link
                  v-else
                  :to="item.to"
                  class="mobile-link"
                  @click="closeMobileMenu"
                >
                  <AppIcon :name="item.icon" />
                  {{ item.label }}
                </router-link>
              </template>
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
import { useLoginStore, useThemeStore, useUpdateStore, useSysInfoStore, useUiStore, useSettingsStore, useRestartUiStore } from '../stores.js'
import { availableLocales } from '../locales/index.js'
import { useHeaderNavigation } from '../composables/useHeaderNavigation.js'
import { safeLocal } from '../composables/useSafeStorage'
import { setBodyScrollLock } from '../composables/useBodyScrollLock'
import { useDismissable } from '../composables/useDismissable'
import BrandLogo from './BrandLogo.vue'

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
const now = ref(new Date())
let updateCheckTimer = null
let clockTimer = null

// Wrap the desktop locale toggle + dropdown for outside-click / Escape dismiss
// (see header.vue for rationale).
const localePickerRef = ref(null)
const { enable: enableLocaleDismiss, disable: disableLocaleDismiss } = useDismissable(localePickerRef, () => (localeOpen.value = false))
watch(localeOpen, (open) => open ? enableLocaleDismiss() : disableLocaleDismiss())

const currentLocale = computed(() => locale.value)
const deviceName = computed(() => sysInfoStore.hostname || 'HB-RF-ETH-ng')
const { visibleNavGroups } = useHeaderNavigation(t, loginStore)
const currentTime = computed(() => now.value.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }))
const uptimeShort = computed(() => {
  const seconds = sysInfoStore.uptimeSeconds || 0
  const days = Math.floor(seconds / 86400)
  const hours = Math.floor((seconds % 86400) / 3600)
  const minutes = Math.floor((seconds % 3600) / 60)
  if (days > 0) return `${days}d ${hours}h ${minutes}m`
  if (hours > 0) return `${hours}h ${minutes}m`
  return `${minutes}m`
})

// iOS Safari ignores `body { overflow: hidden }`; useBodyScrollLock fixes the
// body so the background cannot scroll behind the open mobile menu / modal.
watch(mobileMenuOpen, (isOpen) => setBodyScrollLock(isOpen))
watch(showRestartModal, (isOpen) => setBodyScrollLock(isOpen))

const closeMobileMenu = () => {
  mobileMenuOpen.value = false
}

const changeLocale = (newLocale) => {
  locale.value = newLocale
  localeOpen.value = false
  safeLocal.set('locale', newLocale)
}

const logout = () => {
  closeMobileMenu()
  loginStore.logout()
  router.push('/login')
}

// Same restart contract as header.vue / settings.vue. Defined inline because
// pulling this into a shared composable would require teaching it the modal
// state, the toast store and router — more wiring than the 12 lines are worth.
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
  safeLocal.set('dismissedUpdate', updateStore.latestVersion)
}

onMounted(async () => {
  try {
    if (!sysInfoStore.currentVersion) {
      await sysInfoStore.update()
    }
    // /api/check_update requires auth - calling it logged out triggers a 401
    // that force-redirects visitors away from the public pages (/, /about).
    if (loginStore.isLoggedIn) {
      await updateStore.checkForUpdate(sysInfoStore.currentVersion)
    }
  } catch (e) {
    console.error('Failed to load sys info for update check', e)
  }

  if (loginStore.isLoggedIn) {
    try {
      await settingsStore.ensureLoaded()
    } catch (e) {
      console.error('Failed to load settings for restart sync state', e)
    }
  }

  if (safeLocal.get('dismissedUpdate') === updateStore.latestVersion) {
    showBanner.value = false
  }

  updateCheckTimer = setInterval(() => {
    if (loginStore.isLoggedIn && sysInfoStore.currentVersion) {
      updateStore.checkForUpdate(sysInfoStore.currentVersion)
    }
  }, 24 * 60 * 60 * 1000)

  clockTimer = setInterval(() => {
    now.value = new Date()
  }, 30000)
})

onUnmounted(() => {
  // Release the body scroll lock if a menu/modal was open when navigating away.
  setBodyScrollLock(false)
  disableLocaleDismiss()
  if (updateCheckTimer) {
    clearInterval(updateCheckTimer)
  }
  if (clockTimer) {
    clearInterval(clockTimer)
  }
})
</script>

<style scoped>
.header-shell {
  position: fixed;
  inset: 0 0 auto 0;
  z-index: 1000;
  pointer-events: none;
}

.update-banner {
  position: fixed;
  top: calc(var(--newdesign-header-height) + 12px);
  left: calc(var(--newdesign-sidebar-width) + var(--newdesign-content-gap));
  right: var(--newdesign-content-gap);
  z-index: 1002;
  padding: 10px 14px;
  border-radius: var(--radius-sm);
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
  border: 1px solid var(--color-border-strong);
  background: var(--color-surface);
  box-shadow: var(--shadow-md);
  pointer-events: auto;
}

.desktop-sidebar {
  position: fixed;
  inset: 0 auto 0 0;
  width: var(--newdesign-sidebar-width);
  z-index: 1001;
  padding: 24px 12px 20px;
  background: var(--color-sidebar);
  border-right: 1px solid var(--color-border);
  display: flex;
  flex-direction: column;
  gap: 28px;
  pointer-events: auto;
  /* Allow the sidebar to scroll vertically when the viewport is too short to
     show every nav item plus the pinned footer (restart / logout). Without
     this, items silently overflow below the fold and become unreachable —
     which was the "Vollbildmodus" bug. min-height:0 lets the flex column
     shrink so overflow actually engages. */
  overflow-y: auto;
  overflow-x: hidden;
  min-height: 0;
  scrollbar-width: thin;
}

.desktop-sidebar::-webkit-scrollbar {
  width: 6px;
}

.desktop-sidebar::-webkit-scrollbar-thumb {
  background: var(--color-border);
  border-radius: 3px;
}

.header-nav {
  position: fixed;
  top: 0;
  left: var(--newdesign-sidebar-width);
  right: 0;
  height: var(--newdesign-header-height);
  z-index: 1001;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 18px;
  padding: 0 var(--newdesign-content-gap);
  color: var(--color-text);
  background: var(--newdesign-header, var(--color-surface));
  border-bottom: 1px solid var(--newdesign-border, var(--color-border));
  pointer-events: auto;
  min-width: 0;
}

.brand,
.mobile-brand {
  display: flex;
  align-items: center;
  gap: 14px;
  color: var(--color-text);
  text-decoration: none;
  min-width: 0;
}

.brand-copy {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.brand-copy strong {
  font-size: var(--fs-3xl);
  line-height: var(--line-height-tight);
  letter-spacing: 0;
  font-weight: var(--font-weight-heavy);
  white-space: nowrap;
}

.brand-copy small {
  color: var(--color-text-secondary);
  font-size: var(--fs-sm);
  max-width: 230px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.side-nav {
  display: flex;
  flex-direction: column;
  gap: 18px;
  min-width: 0;
}

.side-nav-group {
  display: flex;
  flex-direction: column;
  gap: 6px;
  min-width: 0;
}

.side-nav-heading {
  padding: 0 14px;
  color: var(--color-text-muted);
  font-size: var(--fs-2xs);
  font-weight: var(--font-weight-heavy);
  letter-spacing: 0.08em;
  line-height: var(--line-height-tight);
  text-transform: uppercase;
  white-space: nowrap;
}

.nav-item {
  min-height: 42px;
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 0 14px;
  border-radius: var(--radius-sm);
  border: 1px solid transparent;
  color: var(--color-text-secondary);
  text-decoration: none;
  font-weight: var(--font-weight-semibold);
  transition: background var(--transition-fast), border-color var(--transition-fast), color var(--transition-fast), transform var(--transition-fast);
  min-width: 0;
  overflow-wrap: anywhere;
}

.nav-item:hover {
  color: var(--color-text);
  background: var(--color-surface);
  border-color: var(--color-border);
}

.nav-item.active {
  color: #fff !important;
  background: var(--color-primary) !important;
  border-color: transparent !important;
  border-radius: 6px;
  box-shadow: none !important;
}

.nav-item.active .app-icon,
.nav-item.active span {
  color: #fff !important;
}

.nav-item .app-icon {
  flex: 0 0 auto;
}

.nav-item span:not(.mini-dot) {
  min-width: 0;
}

/* External-link nav entries (documentation). Same shape as a regular nav
   item so it reads as part of the navigation, but the small external-hint
   icon (top-right of the label) signals "opens in a new tab". */
.nav-item-external {
  text-decoration: none;
}

.nav-item-external .external-hint,
.mobile-link-external .external-hint {
  width: 14px;
  height: 14px;
  margin-left: auto;
  color: var(--color-text-muted);
  opacity: 0.8;
}

.mobile-link-external {
  text-decoration: none;
}

.mobile-link-external span {
  flex: 1;
  min-width: 0;
}

.mini-dot {
  width: 6px;
  height: 6px;
  border-radius: 999px;
  background: var(--color-warning);
}

.sidebar-footer {
  margin-top: auto;
  display: flex;
  flex-direction: column;
  gap: 8px;
  min-width: 0;
}

/* Wrapper around the locale toggle + dropdown so outside-click / Escape can
   dismiss it. It participates in the sidebar column like a direct child. */
.sidebar-footer .locale-picker {
  display: flex;
  flex-direction: column;
  gap: 0;
  width: 100%;
  min-width: 0;
}

.utility-row,
.auth-button,
.mobile-logout,
.mobile-restart {
  min-height: 40px;
  width: 100%;
  border: 1px solid var(--color-border-strong);
  border-radius: var(--radius-sm);
  background: transparent;
  color: var(--color-text-secondary);
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 10px;
  padding: 0 12px;
  text-decoration: none;
  font-weight: 600;
  min-width: 0;
  white-space: normal;
  overflow-wrap: anywhere;
}

.utility-row:hover,
.auth-button:hover,
.mobile-logout:hover,
.mobile-restart:hover {
  color: var(--color-text);
  background: var(--color-surface);
}

/* Restart button wears the brand primary colour so it doesn't get
 * confused with the red logout button next to it. */
.utility-row.restart-row,
.mobile-restart {
  color: var(--color-primary);
  border-color: var(--color-primary-soft);
}

.utility-row.restart-row:hover,
.mobile-restart:hover {
  background: var(--color-primary-soft);
  color: var(--color-primary-strong);
}

.utility-row.restart-row:disabled,
.mobile-restart:disabled {
  opacity: 0.55;
  cursor: not-allowed;
}

.utility-row.logout-row,
.mobile-logout {
  color: var(--color-danger);
}

.icon-button {
  width: 50px;
  height: 50px;
  border: 1px solid var(--color-border-strong);
  border-radius: var(--radius-sm);
  background: var(--color-surface);
  color: var(--color-text);
  display: inline-flex;
  align-items: center;
  justify-content: center;
}

.icon-button:hover {
  background: var(--color-bg-alt);
}

.locale-menu {
  display: grid;
  gap: 4px;
  padding: 6px;
  border: 1px solid var(--color-border);
  border-radius: var(--radius-sm);
  background: var(--color-surface);
  width: 100%;
  max-height: min(280px, calc(100vh - 220px));
  overflow-y: auto;
  overscroll-behavior: contain;
}

.locale-menu-item {
  width: 100%;
  border: none;
  background: transparent;
  color: var(--color-text);
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 8px 10px;
  border-radius: var(--radius-sm);
  text-align: left;
  min-width: 0;
  overflow-wrap: anywhere;
}

.locale-menu-item.active,
.locale-menu-item:hover {
  background: var(--color-bg-alt);
}

.update-banner-copy {
  display: flex;
  align-items: center;
  gap: 12px;
  flex-wrap: wrap;
  min-width: 0;
}

.update-banner-copy strong {
  font-size: var(--fs-md);
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

.mobile-brand {
  display: none;
}

.mobile-title {
  max-width: 46vw;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-weight: 800;
}

.top-status {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: 10px;
  flex: 1;
  min-width: 0;
  overflow: hidden;
  color: var(--color-text-secondary);
  font-size: var(--fs-sm);
}

.top-status strong {
  color: var(--color-text);
}

.connection-label {
  color: var(--color-text);
}

.status-pill {
  display: inline-flex;
  align-items: center;
  min-height: 24px;
  padding: 0 10px;
  border-radius: var(--radius-sm);
  border: 1px solid currentColor;
  font-weight: 800;
  font-size: var(--fs-xs);
}

.status-pill.online {
  color: var(--color-success);
  background: var(--color-success-soft);
}

.status-pill.offline {
  color: var(--color-danger);
  background: var(--color-danger-soft);
}

.top-separator {
  width: 1px;
  height: 22px;
  background: var(--color-border-strong);
}

.clock-chip {
  display: inline-flex;
  align-items: center;
  gap: 7px;
  color: var(--color-text);
}

.clock-chip .app-icon {
  color: var(--color-success);
}

/* Supporter badge chip in the top status bar. Two states: active (warm,
 * filled) when a valid key is stored, and a subtle outline CTA otherwise. */
.supporter-chip {
  display: inline-flex;
  align-items: center;
  gap: 7px;
  padding: 4px 12px;
  border-radius: var(--radius-sm);
  font-size: var(--fs-xs);
  font-weight: 700;
  text-decoration: none;
  border: 1px solid var(--color-border);
  transition: background 0.2s, border-color 0.2s, transform 0.2s;
  min-width: 0;
  max-width: 180px;
}

.supporter-chip span {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.supporter-chip .app-icon {
  width: 16px;
  height: 16px;
}

.supporter-chip.active {
  color: #fff;
  background: var(--color-primary);
  border-color: transparent;
  box-shadow: 0 4px 12px var(--newdesign-primary-soft);
}

.supporter-chip.active:hover {
  transform: translateY(-1px);
  box-shadow: 0 6px 16px var(--newdesign-primary-soft);
}

.supporter-chip.inactive {
  color: var(--color-text-secondary);
  background: transparent;
}

.supporter-chip.inactive:hover {
  color: var(--color-primary-strong);
  border-color: var(--newdesign-hover-border);
  background: var(--color-primary-soft);
}

.header-actions {
  display: flex;
  align-items: center;
  gap: 10px;
  min-width: 0;
}

.mobile-menu-toggle {
  display: none;
}

.mobile-overlay {
  position: fixed;
  inset: 0;
  z-index: 1200;
  background: rgba(0, 0, 0, 0.36);
  display: flex;
  justify-content: flex-end;
  padding: max(12px, env(safe-area-inset-top)) max(12px, env(safe-area-inset-right)) max(12px, env(safe-area-inset-bottom)) max(12px, env(safe-area-inset-left));
  pointer-events: auto;
}

.mobile-panel {
  width: min(340px, 100%);
  height: 100%;
  max-height: calc(100vh - 24px);
  min-height: 0;
  border-radius: var(--radius-sm);
  padding: 16px;
  display: flex;
  flex-direction: column;
  gap: 18px;
  overflow-y: auto;
  overflow-x: hidden;
  overscroll-behavior: contain;
  border: 1px solid var(--color-border);
  background: var(--color-sidebar);
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
  font-size: var(--fs-xs);
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
  border-radius: var(--radius-sm);
  color: var(--color-text);
  text-decoration: none;
  font-weight: 700;
  min-width: 0;
  overflow-wrap: anywhere;
}

.mobile-link.router-link-active {
  background: var(--color-primary);
  color: #fff;
  border-radius: var(--newdesign-radius-button, 6px);
}

.mobile-section-title {
  color: var(--color-text-secondary);
  font-size: var(--fs-xs);
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
  border-radius: var(--radius-sm);
  background: var(--color-surface);
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
  background: var(--color-primary);
  color: #fff;
  border-radius: var(--newdesign-radius-button, 6px);
}

.mobile-auth,
.mobile-logout,
.mobile-restart {
  width: 100%;
}

.mobile-logout {
  color: var(--color-danger);
}

/* Mobile action buttons (Restart / Logout / Login): dark charcoal fills with
   white text, matching the industrial-dashboard reference. Overlays the outline
   style defined further up. The green-restart tint and the red-logout tint are
   dropped here so all three read as a consistent dark action row. */
.mobile-restart,
.mobile-logout,
.mobile-auth {
  border: 1px solid var(--newdesign-border-strong, #d2d4d7) !important;
  border-radius: var(--newdesign-radius-button, 4px) !important;
  background: #3a3f45 !important;
  color: #fff !important;
  font-weight: 700;
  box-shadow: none !important;
}

.mobile-restart:hover:not(:disabled),
.mobile-logout:hover,
.mobile-auth:hover {
  background: #2c2f33 !important;
}

.mobile-restart:disabled {
  opacity: 0.55;
  cursor: not-allowed;
}

/* Dark mode: action buttons are nearly black per the dark-theme spec. */
:global([data-bs-theme="dark"]) .mobile-restart,
:global([data-bs-theme="dark"]) .mobile-logout,
:global([data-bs-theme="dark"]) .mobile-auth {
  background: #1a1c1f !important;
  border-color: #474c51 !important;
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

@media (max-width: 991px) {
  .desktop-sidebar {
    display: none;
  }

  .header-nav {
    left: 0;
    height: var(--newdesign-header-height-mobile);
    padding: 0 max(12px, env(safe-area-inset-right)) 0 max(12px, env(safe-area-inset-left));
  }

  .mobile-brand,
  .mobile-menu-toggle {
    display: inline-flex;
  }

  .top-status {
    display: none;
  }

  .update-banner {
    top: calc(var(--newdesign-header-height-mobile) + 12px);
    left: max(8px, env(safe-area-inset-left));
    right: max(8px, env(safe-area-inset-right));
    flex-direction: column;
    align-items: flex-start;
    max-height: min(45vh, 320px);
    overflow-y: auto;
  }

  .update-banner-actions {
    width: 100%;
  }
}

@media (max-width: 480px) {
  .mobile-overlay {
    padding: 0;
  }

  .mobile-panel {
    width: 100%;
    max-height: 100dvh;
    border-radius: 0;
    border-left: none;
    border-right: none;
    padding: max(16px, env(safe-area-inset-top)) max(16px, env(safe-area-inset-right)) max(16px, env(safe-area-inset-bottom)) max(16px, env(safe-area-inset-left));
  }

  .mobile-title {
    max-width: 58vw;
  }

  .header-actions {
    gap: 6px;
  }

  .icon-button {
    width: 44px;
    height: 44px;
  }

  .update-banner {
    top: calc(var(--newdesign-header-height-mobile) + 4px);
  }
}

@media (max-width: 1280px) {
  .connection-label,
  .top-separator,
  .clock-chip {
    display: none;
  }
}
</style>
