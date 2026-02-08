<template>
  <header class="app-header">
    <!-- Update Notification Banner (Floating Style) -->
    <Transition name="slide-down">
      <div v-if="updateStore.shouldShowUpdateBadge && showBanner" class="update-notification">
        <div class="notification-content">
          <div class="icon-circle">
            <span class="update-icon">🚀</span>
          </div>
          <div class="text-content">
            <span class="notification-title">{{ t('update.available') || 'Update Available' }}</span>
            <span class="notification-subtitle">v{{ updateStore.latestVersion }}</span>
          </div>
          <div class="actions">
            <BButton size="sm" variant="primary" to="/firmware" class="action-btn">
              {{ t('update.updateNow') || 'Update' }}
            </BButton>
            <button @click="dismissUpdate" class="close-btn" aria-label="Close">
              ✕
            </button>
          </div>
        </div>
      </div>
    </Transition>

    <BNavbar toggleable="lg" class="navbar-glass">
      <div class="navbar-container">
        <!-- Brand -->
        <BNavbarBrand to="/" class="brand">
          <div class="brand-logo">
            <span class="brand-icon">📡</span>
          </div>
          <span class="brand-text">HB-RF-ETH-ng</span>
          <span v-if="updateStore.shouldShowUpdateBadge" class="update-dot"></span>
        </BNavbarBrand>

        <!-- Toggle Button (Mobile) -->
        <BNavbarToggle target="nav-collapse" class="navbar-toggle-custom">
          <span class="hamburger-line"></span>
          <span class="hamburger-line"></span>
          <span class="hamburger-line"></span>
        </BNavbarToggle>

        <!-- Collapsible Content -->
        <BCollapse id="nav-collapse" is-nav>
          <div class="nav-content-wrapper">
            <!-- Navigation Links -->
            <BNavbarNav class="nav-links">
              <BNavItem to="/" active-class="active">{{ t('nav.home') }}</BNavItem>

              <!-- Settings Dropdown -->
              <BNavItemDropdown v-if="loginStore.isLoggedIn" class="settings-dropdown" no-caret>
                <template #button-content>
                  <span class="nav-link" :class="{ active: router.currentRoute.value.path === '/settings' }">
                    {{ t('nav.settings') }}
                  </span>
                </template>
                <BDropdownItem to="/settings?tab=general">{{ t('settings.tabGeneral') || 'General' }}</BDropdownItem>
                <BDropdownItem to="/settings?tab=network">{{ t('settings.tabNetwork') || 'Network' }}</BDropdownItem>
                <BDropdownItem to="/settings?tab=time">{{ t('settings.tabTime') || 'Time' }}</BDropdownItem>
                <BDropdownItem to="/settings?tab=backup">{{ t('settings.tabBackup') || 'Backup' }}</BDropdownItem>
              </BNavItemDropdown>

              <BNavItem to="/firmware" v-if="loginStore.isLoggedIn" active-class="active">
                {{ t('nav.firmware') }}
                <span v-if="updateStore.shouldShowUpdateBadge" class="menu-badge"></span>
              </BNavItem>
              <BNavItem to="/monitoring" v-if="loginStore.isLoggedIn" active-class="active">{{ t('nav.monitoring') }}</BNavItem>
              <BNavItem to="/about" active-class="active">{{ t('nav.about') }}</BNavItem>
            </BNavbarNav>

            <!-- Right Side Actions -->
            <BNavbarNav class="nav-actions">
              <!-- Update Indicator (small) -->
              <div v-if="updateStore.shouldShowUpdateBadge && !showBanner" class="action-icon-btn" @click="showBanner = true" title="Show update">
                <span>🚀</span>
              </div>

              <!-- Language Selector -->
              <BNavItemDropdown class="locale-dropdown" no-caret>
                <template #button-content>
                  <div class="locale-btn">
                    {{ currentLocaleFlag }}
                  </div>
                </template>
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
              <div class="action-icon-btn" @click="themeStore.toggleTheme" :title="t('nav.toggleTheme')">
                <span>{{ themeStore.theme === 'light' ? '🌙' : '☀️' }}</span>
              </div>

              <!-- Login / Logout -->
              <BButton
                v-if="!loginStore.isLoggedIn"
                to="/login"
                variant="primary"
                size="sm"
                class="auth-btn"
              >
                {{ t('nav.login') }}
              </BButton>
              <div
                v-if="loginStore.isLoggedIn"
                @click="logout"
                class="action-icon-btn logout-btn"
                :title="t('nav.logout')"
              >
                <span>🚪</span>
              </div>
            </BNavbarNav>
          </div>
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
const currentLocaleFlag = computed(() => {
  const current = availableLocales.find(l => l.code === locale.value)
  return current ? (current.flag || current.code.toUpperCase()) : 'EN'
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

  // Re-check every 24 hours
  setInterval(() => {
    if (sysInfoStore.currentVersion) {
      updateStore.checkForUpdate(sysInfoStore.currentVersion)
    }
  }, 24 * 60 * 60 * 1000)
})
</script>

<style scoped>
.app-header {
  position: relative;
  z-index: 1000;
}

/* Glass Navbar */
.navbar-glass {
  /* Styles are mostly inherited from global .navbar in main.css */
  padding: 0 !important; /* Let inner container handle padding */
}

.navbar-container {
  display: flex;
  align-items: center;
  justify-content: space-between;
  width: 100%;
  padding: var(--spacing-sm) var(--spacing-md);
}

/* Brand */
.brand {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
  text-decoration: none;
  margin-right: var(--spacing-xl);
}

.brand-logo {
  width: 40px;
  height: 40px;
  background: linear-gradient(135deg, var(--color-primary) 0%, var(--color-primary-dark) 100%);
  border-radius: 12px;
  display: flex;
  align-items: center;
  justify-content: center;
  box-shadow: var(--shadow-sm);
}

.brand-icon {
  font-size: 1.25rem;
  filter: drop-shadow(0 2px 4px rgba(0,0,0,0.1));
}

.brand-text {
  font-weight: 700;
  font-size: 1.25rem;
  color: var(--color-text);
  letter-spacing: -0.02em;
}

.update-dot {
  width: 8px;
  height: 8px;
  background-color: var(--color-warning);
  border-radius: 50%;
  margin-left: 4px;
  box-shadow: 0 0 0 2px var(--color-surface);
}

/* Custom Hamburger */
.navbar-toggle-custom {
  border: none;
  padding: 8px;
  background: transparent;
  display: flex;
  flex-direction: column;
  gap: 5px;
  width: 40px;
  height: 40px;
  justify-content: center;
  align-items: center;
}

.hamburger-line {
  width: 24px;
  height: 2px;
  background-color: var(--color-text);
  border-radius: 2px;
  transition: all 0.2s;
}

/* Nav Content */
.nav-content-wrapper {
  display: flex;
  width: 100%;
  justify-content: space-between;
  align-items: center;
}

.nav-links {
  display: flex;
  gap: 4px;
}

:deep(.nav-link) {
  font-size: 0.9375rem;
  padding: 0.5rem 1rem !important;
  border-radius: var(--radius-full) !important;
}

.menu-badge {
  display: inline-block;
  width: 6px;
  height: 6px;
  background-color: var(--color-warning);
  border-radius: 50%;
  margin-left: 6px;
  vertical-align: middle;
}

/* Right Actions */
.nav-actions {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
}

.action-icon-btn {
  width: 40px;
  height: 40px;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 50%;
  cursor: pointer;
  transition: all 0.2s;
  background: transparent;
  font-size: 1.125rem;
  color: var(--color-text);
}

.action-icon-btn:hover {
  background-color: var(--color-bg);
}

.logout-btn:hover {
  background-color: var(--color-danger-light);
  color: var(--color-danger);
}

.locale-btn {
  width: 40px;
  height: 40px;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 50%;
  cursor: pointer;
  background: transparent;
  font-size: 1.25rem;
  transition: all 0.2s;
}

.locale-btn:hover {
  background-color: var(--color-bg);
}

:deep(.dropdown-toggle) {
  padding: 0;
  border: none;
  background: transparent !important;
}

:deep(.dropdown-toggle::after) {
  display: none;
}

:deep(.dropdown-menu) {
  border: none;
  box-shadow: var(--shadow-xl);
  border-radius: var(--radius-lg);
  margin-top: 10px;
  padding: 8px;
}

:deep(.dropdown-item) {
  border-radius: var(--radius-md);
  padding: 8px 16px;
  font-weight: 500;
}

:deep(.dropdown-item:active) {
  background-color: var(--color-primary);
}

.auth-btn {
  border-radius: var(--radius-full);
  padding: 0.5rem 1.25rem;
  font-weight: 600;
}

/* Update Notification */
.update-notification {
  position: absolute;
  top: calc(100% + 10px);
  right: 0;
  left: 0;
  margin: 0 auto;
  width: 90%;
  max-width: 400px;
  background: var(--color-surface);
  border-radius: var(--radius-xl);
  box-shadow: var(--shadow-xl);
  padding: 16px;
  z-index: 2000;
  border: 1px solid rgba(0,0,0,0.05);
}

.notification-content {
  display: flex;
  align-items: center;
  gap: 12px;
}

.icon-circle {
  width: 40px;
  height: 40px;
  background-color: var(--color-primary-light);
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
}

.text-content {
  flex: 1;
  display: flex;
  flex-direction: column;
}

.notification-title {
  font-weight: 700;
  font-size: 0.9375rem;
  color: var(--color-text);
}

.notification-subtitle {
  font-size: 0.8125rem;
  color: var(--color-text-secondary);
}

.actions {
  display: flex;
  align-items: center;
  gap: 12px;
}

.close-btn {
  background: transparent;
  border: none;
  color: var(--color-text-secondary);
  font-size: 1.25rem;
  cursor: pointer;
  padding: 4px;
  line-height: 1;
}

.close-btn:hover {
  color: var(--color-text);
}

/* Transitions */
.slide-down-enter-active,
.slide-down-leave-active {
  transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
}

.slide-down-enter-from,
.slide-down-leave-to {
  opacity: 0;
  transform: translateY(-20px);
}

@media (max-width: 991px) {
  .nav-content-wrapper {
    flex-direction: column;
    align-items: flex-start;
    padding-top: var(--spacing-md);
  }

  .nav-links {
    flex-direction: column;
    width: 100%;
    gap: var(--spacing-xs);
  }

  .nav-actions {
    margin-top: var(--spacing-md);
    width: 100%;
    justify-content: flex-end;
  }
}
</style>
