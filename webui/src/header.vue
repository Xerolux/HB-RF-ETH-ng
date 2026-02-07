<template>
  <header class="app-header">
    <!-- Update Notification Banner -->
    <Transition name="banner">
      <div v-if="updateStore.shouldShowUpdateBadge" class="update-banner">
        <div class="update-banner-content">
          <span class="update-icon">🔄</span>
          <span class="update-text">
            {{ t('update.available') || 'New version available:' }}
            <strong>{{ updateStore.latestVersion }}</strong>
          </span>
          <BButton size="sm" variant="light" to="/firmware" class="update-btn">
            {{ t('update.updateNow') || 'Update' }}
          </BButton>
          <BButton size="sm" variant="light" @click="dismissUpdate" class="update-close">
            ✕
          </BButton>
        </div>
      </div>
    </Transition>

    <BNavbar toggleable="lg" class="navbar-custom">
      <div class="navbar-content">
        <!-- Brand -->
        <BNavbarBrand to="/" class="brand">
          <span class="brand-icon">📡</span>
          <span class="brand-text">HB-RF-ETH-ng</span>
          <span v-if="updateStore.shouldShowUpdateBadge" class="update-badge" :title="t('update.available') || 'Update available'">
            <span class="pulse"></span>
          </span>
        </BNavbarBrand>

        <!-- Toggle Button (Mobile) -->
        <BNavbarToggle target="nav-collapse" class="navbar-toggle">
          <span class="hamburger"></span>
        </BNavbarToggle>

        <!-- Collapsible Content -->
        <BCollapse id="nav-collapse" is-nav class="navbar-collapse">
          <!-- Navigation Links -->
          <BNavbarNav class="nav-links">
            <BNavItem to="/">{{ t('nav.home') }}</BNavItem>
            <BNavItem to="/settings" v-if="loginStore.isLoggedIn">{{ t('nav.settings') }}</BNavItem>
            <BNavItem to="/firmware" v-if="loginStore.isLoggedIn">
              {{ t('nav.firmware') }}
              <span v-if="updateStore.shouldShowUpdateBadge" class="nav-update-dot"></span>
            </BNavItem>
            <BNavItem to="/monitoring" v-if="loginStore.isLoggedIn">{{ t('nav.monitoring') }}</BNavItem>
            <BNavItem to="/about">{{ t('nav.about') }}</BNavItem>
          </BNavbarNav>

          <!-- Right Side Actions -->
          <BNavbarNav class="nav-actions ms-auto">
            <!-- Update Indicator (small) -->
            <div v-if="updateStore.shouldShowUpdateBadge && !showBanner" class="update-indicator" @click="showBanner = true">
              <span class="update-indicator-icon">🔄</span>
            </div>

            <!-- Language Selector -->
            <BNavItemDropdown :text="currentLocaleName" variant="light" class="dropdown-locale">
              <BDropdownItem
                v-for="locale in availableLocales"
                :key="locale.code"
                @click="changeLocale(locale.code)"
                :active="locale.code === currentLocale"
              >
                <span class="locale-flag">{{ locale.flag || '' }}</span>
                {{ locale.name }}
              </BDropdownItem>
            </BNavItemDropdown>

            <!-- Theme Toggle -->
            <BNavItem
              @click.prevent="themeStore.toggleTheme"
              class="nav-icon-btn"
              :title="t('nav.toggleTheme')"
              :aria-label="t('nav.toggleTheme')"
            >
              <span class="theme-icon">{{ themeStore.theme === 'light' ? '🌙' : '☀️' }}</span>
            </BNavItem>

            <!-- Login / Logout -->
            <BNavItem to="/login" v-if="!loginStore.isLoggedIn" class="nav-login">
              {{ t('nav.login') }}
            </BNavItem>
            <BNavItem v-if="loginStore.isLoggedIn" @click.prevent="logout" class="nav-logout">
              {{ t('nav.logout') }}
            </BNavItem>
          </BNavbarNav>
        </BCollapse>
      </div>
    </BNavbar>
  </header>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
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
const dismissedVersion = ref(localStorage.getItem('dismissedUpdate'))

const currentLocale = computed(() => locale.value)
const currentLocaleName = computed(() => {
  const current = availableLocales.find(l => l.code === locale.value)
  return current ? current.name : 'English'
})

const changeLocale = (newLocale) => {
  locale.value = newLocale
  localStorage.setItem('locale', newLocale)
}

const logout = () => {
  loginStore.logout()
  router.push('/login')
}

const dismissUpdate = () => {
  showBanner.value = false
  localStorage.setItem('dismissedUpdate', updateStore.latestVersion)
}

// Check for updates on mount
onMounted(async () => {
  // Check if banner was dismissed
  if (dismissedVersion.value === updateStore.latestVersion) {
    showBanner.value = false
  }

  // Get current version from sysInfo
  if (sysInfoStore.currentVersion) {
    await updateStore.checkForUpdate(sysInfoStore.currentVersion)
  } else {
    // Fetch sysInfo first
    try {
      await sysInfoStore.update()
      await updateStore.checkForUpdate(sysInfoStore.currentVersion)
    } catch (e) {
      console.error('Failed to load sys info for update check', e)
    }
  }

  // Re-check every hour
  setInterval(() => {
    if (sysInfoStore.currentVersion) {
      updateStore.checkForUpdate(sysInfoStore.currentVersion)
    }
  }, 60 * 60 * 1000)
})
</script>

<style scoped>
.app-header {
  margin-bottom: var(--spacing-lg);
}

/* Update Banner */
.update-banner {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border-radius: var(--radius-lg);
  padding: var(--spacing-md) var(--spacing-lg);
  margin-bottom: var(--spacing-md);
  box-shadow: var(--shadow-md);
}

.update-banner-content {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  color: white;
  flex-wrap: wrap;
}

.update-icon {
  font-size: 1.5rem;
  animation: rotate 2s linear infinite;
}

@keyframes rotate {
  from { transform: rotate(0deg); }
  to { transform: rotate(360deg); }
}

.update-text {
  flex: 1;
  font-size: 0.9375rem;
}

.update-btn {
  padding: 0.375rem 1rem;
  font-weight: 600;
  border: none;
}

.update-close {
  padding: 0.375rem 0.625rem;
  width: 32px;
  height: 32px;
  display: flex;
  align-items: center;
  justify-content: center;
  border: none;
  opacity: 0.8;
}

.update-close:hover {
  opacity: 1;
}

/* Banner transition */
.banner-enter-active,
.banner-leave-active {
  transition: all var(--transition-normal);
}

.banner-enter-from,
.banner-leave-to {
  opacity: 0;
  transform: translateY(-10px);
}

/* Update Badge on Brand */
.update-badge {
  position: relative;
  width: 12px;
  height: 12px;
  margin-left: var(--spacing-xs);
}

.update-badge .pulse {
  position: absolute;
  width: 100%;
  height: 100%;
  background-color: #ffd700;
  border-radius: 50%;
  animation: pulse 2s infinite;
}

.update-badge .pulse::before,
.update-badge .pulse::after {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background-color: #ffd700;
  border-radius: 50%;
  animation: pulse 2s infinite;
}

.update-badge .pulse::after {
  animation-delay: 0.5s;
}

@keyframes pulse {
  0% {
    transform: scale(1);
    opacity: 1;
  }
  50% {
    transform: scale(1.5);
    opacity: 0.5;
  }
  100% {
    transform: scale(1);
    opacity: 1;
  }
}

/* Nav Update Dot */
.nav-update-dot {
  position: relative;
}

.nav-update-dot::after {
  content: '';
  position: absolute;
  top: 4px;
  right: -4px;
  width: 8px;
  height: 8px;
  background-color: #ffd700;
  border-radius: 50%;
  box-shadow: 0 0 0 2px rgba(255, 107, 53, 0.3);
  animation: blink 1.5s infinite;
}

@keyframes blink {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}

/* Update Indicator (small icon in nav) */
.update-indicator {
  padding: 0.5rem;
  border-radius: var(--radius-md);
  cursor: pointer;
  transition: all var(--transition-fast);
}

.update-indicator:hover {
  background-color: rgba(255, 255, 255, 0.15);
}

.update-indicator-icon {
  font-size: 1.125rem;
  animation: rotate 3s linear infinite;
}

/* Navbar */
.navbar-custom {
  background: linear-gradient(135deg, var(--color-primary) 0%, var(--color-primary-dark) 100%);
  border-radius: var(--radius-lg);
  box-shadow: var(--shadow-lg);
  padding: var(--spacing-sm) var(--spacing-md);
  backdrop-filter: blur(10px);
}

.navbar-content {
  display: flex;
  align-items: center;
  width: 100%;
}

/* Brand */
.brand {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
  color: white !important;
  text-decoration: none;
  position: relative;
}

.brand-icon {
  font-size: 1.5rem;
  filter: drop-shadow(0 2px 4px rgba(0, 0, 0, 0.2));
}

.brand-text {
  font-weight: 700;
  font-size: 1.25rem;
  letter-spacing: -0.025em;
}

/* Toggle Button (Hamburger) */
.navbar-toggle {
  border: none;
  padding: var(--spacing-sm);
  margin-left: auto;
}

.navbar-toggle .navbar-toggler-icon {
  background-image: none;
}

.hamburger {
  display: block;
  width: 24px;
  height: 2px;
  background: white;
  position: relative;
  transition: all var(--transition-fast);
}

.hamburger::before,
.hamburger::after {
  content: '';
  position: absolute;
  width: 24px;
  height: 2px;
  background: white;
  left: 0;
  transition: all var(--transition-fast);
}

.hamburger::before { top: -8px; }
.hamburger::after { top: 8px; }

.navbar-toggle[aria-expanded="true"] .hamburger {
  background: transparent;
}

.navbar-toggle[aria-expanded="true"] .hamburger::before {
  transform: rotate(45deg);
  top: 0;
}

.navbar-toggle[aria-expanded="true"] .hamburger::after {
  transform: rotate(-45deg);
  top: 0;
}

/* Collapse */
.navbar-collapse {
  margin-top: var(--spacing-md);
}

@media (min-width: 992px) {
  .navbar-collapse {
    margin-top: 0;
  }

  .navbar-toggle {
    display: none;
  }
}

/* Nav Links */
.nav-links {
  gap: var(--spacing-xs);
}

.nav-links .nav-link {
  color: rgba(255, 255, 255, 0.9) !important;
  padding: 0.625rem 1rem;
  border-radius: var(--radius-md);
  transition: all var(--transition-fast);
  font-weight: 500;
  min-height: var(--touch-target);
  display: flex;
  align-items: center;
}

.nav-links .nav-link:hover,
.nav-links .nav-link.active {
  background-color: rgba(255, 255, 255, 0.15);
  color: white !important;
  transform: translateY(-1px);
}

/* Nav Actions */
.nav-actions {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
  margin-top: var(--spacing-md);
}

@media (min-width: 992px) {
  .nav-actions {
    margin-top: 0;
  }
}

/* Icon Buttons */
.nav-icon-btn {
  padding: 0.625rem;
  border-radius: var(--radius-md);
  transition: all var(--transition-fast);
  min-width: var(--touch-target);
  display: flex;
  align-items: center;
  justify-content: center;
}

.nav-icon-btn:hover {
  background-color: rgba(255, 255, 255, 0.15);
}

.theme-icon {
  font-size: 1.25rem;
  filter: drop-shadow(0 1px 2px rgba(0, 0, 0, 0.2));
}

/* Login/Logout Links */
.nav-login,
.nav-logout {
  padding: 0.625rem 1.25rem;
  background-color: rgba(255, 255, 255, 0.15);
  border-radius: var(--radius-md);
  font-weight: 600;
  transition: all var(--transition-fast);
  min-height: var(--touch-target);
  display: flex;
  align-items: center;
}

.nav-login:hover,
.nav-logout:hover {
  background-color: rgba(255, 255, 255, 0.25);
  transform: translateY(-1px);
}

/* Dropdown */
.dropdown-locale ::v-deep(.btn) {
  background-color: rgba(255, 255, 255, 0.15);
  border: none;
  color: white;
  padding: 0.5rem 1rem;
  font-weight: 500;
  border-radius: var(--radius-md);
}

.dropdown-locale ::v-deep(.btn:hover) {
  background-color: rgba(255, 255, 255, 0.25);
}

.dropdown-locale ::v-deep(.dropdown-menu) {
  border: 1px solid var(--color-border);
  border-radius: var(--radius-md);
  box-shadow: var(--shadow-lg);
  padding: var(--spacing-xs);
  max-height: 300px;
  overflow-y: auto;
}

.dropdown-locale ::v-deep(.dropdown-item) {
  padding: 0.625rem 1rem;
  border-radius: var(--radius-sm);
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
}

.dropdown-locale ::v-deep(.dropdown-item:hover),
.dropdown-locale ::v-deep(.dropdown-item.active) {
  background-color: var(--color-primary-light);
}

.locale-flag {
  font-size: 1.125rem;
}

/* Mobile Adjustments */
@media (max-width: 991px) {
  .navbar-collapse {
    background-color: rgba(0, 0, 0, 0.1);
    border-radius: var(--radius-md);
    padding: var(--spacing-md);
  }

  .nav-links,
  .nav-actions {
    flex-direction: column;
    width: 100%;
  }

  .nav-links .nav-link,
  .nav-icon-btn,
  .nav-login,
  .nav-logout,
  .update-indicator {
    width: 100%;
    justify-content: center;
  }
}
</style>
