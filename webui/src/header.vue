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

      <div class="device-chip desktop-only" :title="deviceName">
        <AppIcon name="router" />
        <span>{{ deviceName }}</span>
      </div>

      <div class="desktop-nav">
        <router-link to="/" class="nav-item" active-class="active">{{ t('nav.home') }}</router-link>
        <router-link v-if="loginStore.isLoggedIn" to="/settings" class="nav-item" active-class="active">
          {{ t('nav.settings') }}
        </router-link>
        <router-link v-if="loginStore.isLoggedIn" to="/firmware" class="nav-item" active-class="active">
          {{ t('nav.firmware') }}
          <span v-if="updateStore.shouldShowUpdateBadge" class="mini-dot"></span>
        </router-link>
        <router-link v-if="loginStore.isLoggedIn" to="/monitoring" class="nav-item" active-class="active">{{ t('nav.monitoring') }}</router-link>
        <router-link v-if="loginStore.isLoggedIn" to="/systemlog" class="nav-item" active-class="active">{{ t('nav.systemlog') }}</router-link>
        <router-link to="/about" class="nav-item" active-class="active">{{ t('nav.about') }}</router-link>
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
        <button v-else class="icon-button logout-button desktop-only" :title="t('nav.logout')" @click="logout">
          <AppIcon name="power" />
        </button>

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
            <router-link to="/" class="mobile-link" @click="closeMobileMenu">
              <AppIcon name="dashboard" />
              {{ t('nav.home') }}
            </router-link>
            <router-link v-if="loginStore.isLoggedIn" to="/settings" class="mobile-link" @click="closeMobileMenu">
              <AppIcon name="settings" />
              {{ t('nav.settings') }}
            </router-link>
            <router-link v-if="loginStore.isLoggedIn" to="/firmware" class="mobile-link" @click="closeMobileMenu">
              <AppIcon name="firmware" />
              {{ t('nav.firmware') }}
            </router-link>
            <router-link v-if="loginStore.isLoggedIn" to="/monitoring" class="mobile-link" @click="closeMobileMenu">
              <AppIcon name="monitoring" />
              {{ t('nav.monitoring') }}
            </router-link>
            <router-link v-if="loginStore.isLoggedIn" to="/systemlog" class="mobile-link" @click="closeMobileMenu">
              <AppIcon name="logs" />
              {{ t('nav.systemlog') }}
            </router-link>
            <router-link to="/about" class="mobile-link" @click="closeMobileMenu">
              <AppIcon name="info" />
              {{ t('nav.about') }}
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
let updateCheckTimer = null

const currentLocale = computed(() => locale.value)
const deviceName = computed(() => sysInfoStore.hostname || 'HB-RF-ETH-ng')

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
  gap: 20px;
  padding: 12px 16px;
  border-radius: 28px;
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

.device-chip {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  min-width: 0;
  max-width: 220px;
  padding: 8px 12px;
  border-radius: var(--radius-pill);
  color: var(--color-primary-strong);
  background: var(--color-primary-soft);
  font-weight: 800;
  font-size: 0.86rem;
}

.device-chip span {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.desktop-nav {
  display: flex;
  align-items: center;
  gap: 6px;
  flex: 1;
  flex-wrap: wrap;
}

.nav-item {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 10px 14px;
  border-radius: var(--radius-pill);
  color: var(--color-text-secondary);
  text-decoration: none;
  font-weight: 700;
  font-size: 0.9rem;
  transition: all var(--transition-fast);
}

.nav-item:hover,
.nav-item.active {
  color: var(--color-text);
  background: rgba(127, 127, 127, 0.18);
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
}

.auth-button {
  background: linear-gradient(135deg, var(--color-primary), var(--color-primary-strong));
  color: white;
}

.logout-button {
  color: var(--color-danger);
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
  min-width: 200px;
  padding: 10px;
  border-radius: 22px;
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

.mobile-only {
  display: none;
}

.mobile-overlay {
  position: fixed;
  inset: 0;
  background: rgba(10, 14, 20, 0.38);
  backdrop-filter: blur(8px);
  display: flex;
  justify-content: flex-end;
  padding: 12px;
}

.mobile-panel {
  width: min(340px, 100%);
  height: 100%;
  min-height: 0;
  border-radius: 28px;
  padding: 16px;
  display: flex;
  flex-direction: column;
  gap: 18px;
  overflow-y: auto;
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
  border-radius: 16px;
  color: var(--color-text);
  text-decoration: none;
  font-weight: 700;
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
  grid-template-columns: repeat(3, 1fr);
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
}

.mobile-locale.active {
  background: var(--color-primary-soft);
  color: var(--color-primary-strong);
}

.mobile-auth,
.mobile-logout {
  width: 100%;
}

.mobile-logout {
  background: var(--color-danger-soft);
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
