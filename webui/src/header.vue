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

    <aside class="desktop-sidebar">
      <router-link to="/" class="brand" @click="closeMobileMenu">
        <span class="brand-orbit" aria-hidden="true">
          <span></span><span></span><span></span>
        </span>
        <span class="brand-copy">
          <strong>HB-RF-ETH-ng</strong>
          <small>{{ deviceName }}</small>
        </span>
      </router-link>

      <nav class="side-nav" aria-label="Main navigation">
        <router-link
          v-for="item in visibleNavItems"
          :key="item.to"
          :to="item.to"
          class="nav-item"
          active-class="active"
          @click="closeMobileMenu"
        >
          <AppIcon :name="item.icon" />
          <span>{{ item.label }}</span>
          <span v-if="item.to === '/firmware' && updateStore.shouldShowUpdateBadge" class="mini-dot"></span>
        </router-link>
      </nav>

      <div class="sidebar-footer">
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
        <button class="utility-row" type="button" @click="themeStore.toggleTheme">
          <AppIcon :name="themeStore.theme === 'light' ? 'moon' : 'sun'" />
          <span>{{ t('nav.toggleTheme') }}</span>
        </button>
        <router-link v-if="!loginStore.isLoggedIn" to="/login" class="utility-row">{{ t('nav.login') }}</router-link>
        <button v-else class="utility-row" type="button" @click="logout">
          <AppIcon name="power" />
          <span>{{ t('nav.logout') }}</span>
        </button>
      </div>
    </aside>

    <nav class="header-nav">
      <router-link to="/" class="mobile-brand" @click="closeMobileMenu">
        <span class="brand-orbit small" aria-hidden="true">
          <span></span><span></span><span></span>
        </span>
        <span class="mobile-title">{{ deviceName }}</span>
      </router-link>

      <div class="top-status">
        <span class="connection-label">{{ t('sysinfo.ethernetStatus') }}</span>
        <span class="status-pill" :class="sysInfoStore.ethernetConnected ? 'online' : 'offline'">
          {{ sysInfoStore.ethernetConnected ? 'LAN' : t('sysinfo.offline') }}
        </span>
        <span class="top-separator"></span>
        <span>{{ t('sysinfo.uptime') }}: <strong>{{ uptimeShort }}</strong></span>
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
            <router-link
              v-for="item in visibleNavItems"
              :key="item.to"
              :to="item.to"
              class="mobile-link"
              @click="closeMobileMenu"
            >
              <AppIcon :name="item.icon" />
              {{ item.label }}
            </router-link>
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

          <router-link v-if="!loginStore.isLoggedIn" to="/login" class="auth-button mobile-auth" @click="closeMobileMenu">
            {{ t('nav.login') }}
          </router-link>
          <button v-else class="mobile-logout" @click="logout">
            <AppIcon name="power" />
            {{ t('nav.logout') }}
          </button>
        </div>
      </div>
    </Transition>
  </header>
</template>

<script setup>
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useLoginStore, useThemeStore, useUpdateStore, useSysInfoStore } from './stores.js'
import { availableLocales } from './locales/index.js'

const { t, locale } = useI18n()
const router = useRouter()
const loginStore = useLoginStore()
const themeStore = useThemeStore()
const updateStore = useUpdateStore()
const sysInfoStore = useSysInfoStore()

const showBanner = ref(true)
const localeOpen = ref(false)
const mobileMenuOpen = ref(false)
const now = ref(new Date())
let updateCheckTimer = null
let clockTimer = null

const currentLocale = computed(() => locale.value)
const deviceName = computed(() => sysInfoStore.hostname || 'HB-RF-ETH-ng')
const navItems = computed(() => [
  { to: '/', icon: 'dashboard', label: t('nav.home'), public: true },
  { to: '/settings', icon: 'settings', label: t('nav.settings') },
  { to: '/firmware', icon: 'firmware', label: t('nav.firmware') },
  { to: '/monitoring', icon: 'monitoring', label: t('nav.monitoring') },
  { to: '/systemlog', icon: 'logs', label: t('nav.systemlog') },
  { to: '/about', icon: 'info', label: t('nav.about'), public: true }
])
const visibleNavItems = computed(() => navItems.value.filter((item) => item.public || loginStore.isLoggedIn))
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

  if (localStorage.getItem('dismissedUpdate') === updateStore.latestVersion) {
    showBanner.value = false
  }

  updateCheckTimer = setInterval(() => {
    if (loginStore.isLoggedIn && sysInfoStore.currentVersion) {
      updateStore.checkForUpdate(sysInfoStore.currentVersion, { cached: true })
    }
  }, 24 * 60 * 60 * 1000)

  clockTimer = setInterval(() => {
    now.value = new Date()
  }, 30000)
})

onUnmounted(() => {
  document.body.style.overflow = ''
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
  top: 12px;
  left: 384px;
  right: 24px;
  z-index: 1003;
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
  width: 360px;
  padding: 24px 12px 20px;
  background: var(--color-sidebar);
  border-right: 1px solid var(--color-border);
  display: flex;
  flex-direction: column;
  gap: 28px;
  pointer-events: auto;
}

.header-nav {
  position: fixed;
  top: 0;
  left: 360px;
  right: 0;
  height: 88px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 18px;
  padding: 0 24px;
  color: var(--color-text);
  background: var(--color-bg);
  border-bottom: 1px solid var(--color-border);
  pointer-events: auto;
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

.brand-orbit {
  position: relative;
  width: 52px;
  height: 52px;
  flex: 0 0 auto;
  display: inline-flex;
}

.brand-orbit.small {
  width: 34px;
  height: 34px;
}

.brand-orbit span {
  position: absolute;
  width: 30px;
  height: 30px;
  border-radius: 999px;
  background: rgba(242, 106, 61, 0.72);
}

.brand-orbit.small span {
  width: 20px;
  height: 20px;
}

.brand-orbit span:nth-child(1) {
  left: 3px;
  top: 5px;
}

.brand-orbit span:nth-child(2) {
  right: 4px;
  top: 5px;
}

.brand-orbit span:nth-child(3) {
  left: 12px;
  bottom: 2px;
}

.brand-copy {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.brand-copy strong {
  font-size: 2.25rem;
  line-height: 1;
  letter-spacing: 0;
  font-weight: 800;
  white-space: nowrap;
}

.brand-copy small {
  color: var(--color-text-secondary);
  font-size: 0.95rem;
  max-width: 230px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.side-nav {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.nav-item {
  min-height: 42px;
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 0 22px;
  border-radius: var(--radius-sm);
  color: var(--color-text-secondary);
  text-decoration: none;
  font-weight: 650;
  transition: background var(--transition-fast), color var(--transition-fast);
}

.nav-item:hover,
.nav-item.active {
  color: white;
  background: var(--color-primary);
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
}

.utility-row,
.auth-button,
.mobile-logout {
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
  font-weight: 650;
}

.utility-row:hover,
.auth-button:hover,
.mobile-logout:hover {
  color: var(--color-text);
  background: var(--color-surface);
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
  color: var(--color-text-secondary);
  font-size: 0.95rem;
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
  font-size: 0.8rem;
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

.header-actions {
  display: flex;
  align-items: center;
  gap: 10px;
}

.mobile-menu-toggle {
  display: none;
}

.mobile-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0, 0, 0, 0.36);
  display: flex;
  justify-content: flex-end;
  padding: 12px;
  pointer-events: auto;
}

.mobile-panel {
  width: min(340px, 100%);
  height: 100%;
  min-height: 0;
  border-radius: var(--radius-sm);
  padding: 16px;
  display: flex;
  flex-direction: column;
  gap: 18px;
  overflow-y: auto;
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
  font-size: 0.82rem;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.mobile-links,
.mobile-section {
  display: flex;
  flex-direction: column;
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
}

.mobile-link.router-link-active {
  background: var(--color-primary);
  color: white;
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
  grid-template-columns: repeat(3, 1fr);
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
}

.mobile-locale.active {
  background: var(--color-primary);
  color: white;
}

.mobile-auth,
.mobile-logout {
  width: 100%;
}

.mobile-logout {
  color: var(--color-danger);
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
    height: 72px;
    padding: 0 16px;
  }

  .mobile-brand,
  .mobile-menu-toggle {
    display: inline-flex;
  }

  .top-status {
    display: none;
  }

  .update-banner {
    left: 8px;
    right: 8px;
    top: 78px;
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
