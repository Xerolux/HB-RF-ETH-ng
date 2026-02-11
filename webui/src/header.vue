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
            <span class="notification-title">{{ t('update.available') }}</span>
            <span class="notification-subtitle">v{{ updateStore.latestVersion }}</span>
          </div>
          <div class="actions">
            <BButton size="sm" variant="primary" to="/firmware" class="action-btn" @click="mobileMenuOpen = false">
              {{ t('update.updateNow') }}
            </BButton>
            <button @click="dismissUpdate" class="close-btn" aria-label="Close">
              ✕
            </button>
          </div>
        </div>
      </div>
    </Transition>

    <nav class="navbar-glass">
      <div class="navbar-container">
        <!-- Brand -->
        <router-link to="/" class="brand" @click="mobileMenuOpen = false">
          <div class="brand-logo">
            <span class="brand-icon">📡</span>
          </div>
          <span class="brand-text">HB-RF-ETH-ng</span>
          <span v-if="updateStore.shouldShowUpdateBadge" class="update-dot"></span>
        </router-link>

        <!-- Right actions always visible (theme + hamburger on mobile) -->
        <div class="navbar-right-mobile">
          <!-- Theme Toggle (always visible) -->
          <div class="action-icon-btn" @click="themeStore.toggleTheme" :title="t('nav.toggleTheme')">
            <span>{{ themeStore.theme === 'light' ? '🌙' : '☀️' }}</span>
          </div>

          <!-- Hamburger Toggle (Mobile) -->
          <button
            class="navbar-toggle-custom"
            :class="{ 'is-open': mobileMenuOpen }"
            @click="mobileMenuOpen = !mobileMenuOpen"
            aria-label="Toggle navigation"
          >
            <span class="hamburger-line"></span>
            <span class="hamburger-line"></span>
            <span class="hamburger-line"></span>
          </button>
        </div>

        <!-- Desktop Navigation -->
        <div class="nav-content-wrapper desktop-nav">
          <!-- Navigation Links -->
          <div class="nav-links">
            <router-link to="/" class="nav-link-item" active-class="active" exact>{{ t('nav.home') }}</router-link>

            <!-- Settings Dropdown -->
            <div v-if="loginStore.isLoggedIn" class="settings-dropdown" @mouseenter="settingsOpen = true" @mouseleave="settingsOpen = false">
              <button class="nav-link-item" :class="{ 'active': isSettingsRoute }">
                {{ t('nav.settings') }}
                <span v-if="updateStore.shouldShowUpdateBadge" class="menu-badge"></span>
                <svg class="dropdown-chevron" width="10" height="6" viewBox="0 0 10 6"><path d="M1 1l4 4 4-4" stroke="currentColor" stroke-width="1.5" fill="none"/></svg>
              </button>
              <Transition name="dropdown-fade">
                <div v-show="settingsOpen" class="dropdown-panel">
                  <router-link to="/settings?tab=general" class="dropdown-link" @click="settingsOpen = false">{{ t('settings.tabGeneral') }}</router-link>
                  <router-link to="/settings?tab=network" class="dropdown-link" @click="settingsOpen = false">{{ t('settings.tabNetwork') }}</router-link>
                  <router-link to="/settings?tab=time" class="dropdown-link" @click="settingsOpen = false">{{ t('settings.tabTime') }}</router-link>
                  <router-link to="/settings?tab=backup" class="dropdown-link" @click="settingsOpen = false">{{ t('settings.tabBackup') }}</router-link>
                  <div class="dropdown-divider"></div>
                  <router-link to="/firmware" class="dropdown-link" @click="settingsOpen = false">
                    {{ t('nav.firmware') }}
                    <span v-if="updateStore.shouldShowUpdateBadge" class="menu-badge"></span>
                  </router-link>
                  <router-link to="/monitoring" class="dropdown-link" @click="settingsOpen = false">{{ t('nav.monitoring') }}</router-link>
                  <router-link to="/systemlog" class="dropdown-link" @click="settingsOpen = false">{{ t('nav.systemlog') }}</router-link>
                </div>
              </Transition>
            </div>

            <router-link to="/about" class="nav-link-item" active-class="active">{{ t('nav.about') }}</router-link>
          </div>

          <!-- Right Side Actions (Desktop) -->
          <div class="nav-actions">
            <div v-if="updateStore.shouldShowUpdateBadge && !showBanner" class="action-icon-btn" @click="showBanner = true" title="Show update">
              <span>🚀</span>
            </div>

            <!-- Language Selector -->
            <div class="locale-dropdown" @mouseenter="localeOpen = true" @mouseleave="localeOpen = false">
              <button class="locale-btn">
                {{ currentLocaleFlag }}
              </button>
              <Transition name="dropdown-fade">
                <div v-show="localeOpen" class="dropdown-panel dropdown-right">
                  <button
                    v-for="loc in availableLocales"
                    :key="loc.code"
                    @click="changeLocale(loc.code); localeOpen = false"
                    :class="['dropdown-link', { active: loc.code === currentLocale }]"
                  >
                    <span class="locale-flag">{{ loc.flag || '' }}</span>
                    {{ loc.name }}
                  </button>
                </div>
              </Transition>
            </div>

            <!-- Theme Toggle -->
            <div class="action-icon-btn" @click="themeStore.toggleTheme" :title="t('nav.toggleTheme')">
              <span>{{ themeStore.theme === 'light' ? '🌙' : '☀️' }}</span>
            </div>

            <!-- Login / Logout -->
            <router-link
              v-if="!loginStore.isLoggedIn"
              to="/login"
              class="auth-btn"
            >
              {{ t('nav.login') }}
            </router-link>
            <div
              v-if="loginStore.isLoggedIn"
              @click="logout"
              class="action-icon-btn logout-btn"
              :title="t('nav.logout')"
            >
              <span>🚪</span>
            </div>
          </div>
        </div>
      </div>
    </nav>

    <!-- Mobile Menu Overlay -->
    <Transition name="mobile-menu">
      <div v-if="mobileMenuOpen" class="mobile-menu-overlay" @click.self="mobileMenuOpen = false">
        <div class="mobile-menu-panel">
          <!-- Mobile Nav Links -->
          <div class="mobile-nav-section">
            <router-link to="/" class="mobile-nav-link" @click="mobileMenuOpen = false">
              <span class="mobile-nav-icon">🏠</span>
              {{ t('nav.home') }}
            </router-link>

            <template v-if="loginStore.isLoggedIn">
              <!-- Settings Group -->
              <div class="mobile-nav-group">
                <button class="mobile-nav-group-header" @click="mobileSettingsExpanded = !mobileSettingsExpanded">
                  <span class="mobile-nav-icon">⚙️</span>
                  {{ t('nav.settings') }}
                  <span v-if="updateStore.shouldShowUpdateBadge" class="menu-badge"></span>
                  <svg class="mobile-chevron" :class="{ 'expanded': mobileSettingsExpanded }" width="12" height="7" viewBox="0 0 12 7"><path d="M1 1l5 5 5-5" stroke="currentColor" stroke-width="2" fill="none"/></svg>
                </button>
                <Transition name="expand">
                  <div v-show="mobileSettingsExpanded" class="mobile-nav-subitems">
                    <router-link to="/settings?tab=general" class="mobile-nav-sublink" @click="mobileMenuOpen = false">{{ t('settings.tabGeneral') }}</router-link>
                    <router-link to="/settings?tab=network" class="mobile-nav-sublink" @click="mobileMenuOpen = false">{{ t('settings.tabNetwork') }}</router-link>
                    <router-link to="/settings?tab=time" class="mobile-nav-sublink" @click="mobileMenuOpen = false">{{ t('settings.tabTime') }}</router-link>
                    <router-link to="/settings?tab=backup" class="mobile-nav-sublink" @click="mobileMenuOpen = false">{{ t('settings.tabBackup') }}</router-link>
                    <div class="mobile-divider"></div>
                    <router-link to="/firmware" class="mobile-nav-sublink" @click="mobileMenuOpen = false">
                      {{ t('nav.firmware') }}
                      <span v-if="updateStore.shouldShowUpdateBadge" class="menu-badge"></span>
                    </router-link>
                    <router-link to="/monitoring" class="mobile-nav-sublink" @click="mobileMenuOpen = false">{{ t('nav.monitoring') }}</router-link>
                    <router-link to="/systemlog" class="mobile-nav-sublink" @click="mobileMenuOpen = false">{{ t('nav.systemlog') }}</router-link>
                  </div>
                </Transition>
              </div>
            </template>

            <router-link to="/about" class="mobile-nav-link" @click="mobileMenuOpen = false">
              <span class="mobile-nav-icon">ℹ️</span>
              {{ t('nav.about') }}
            </router-link>
          </div>

          <!-- Mobile Actions -->
          <div class="mobile-actions-section">
            <!-- Language (Collapsible) -->
            <div class="mobile-nav-group">
              <button class="mobile-nav-group-header" @click="mobileLanguageExpanded = !mobileLanguageExpanded">
                <span class="mobile-nav-icon">🌍</span>
                {{ t('nav.language') }}
                <span class="current-lang-code">{{ currentLocale.toUpperCase() }}</span>
                <svg class="mobile-chevron" :class="{ 'expanded': mobileLanguageExpanded }" width="12" height="7" viewBox="0 0 12 7"><path d="M1 1l5 5 5-5" stroke="currentColor" stroke-width="2" fill="none"/></svg>
              </button>
              <Transition name="expand">
                <div v-show="mobileLanguageExpanded" class="mobile-nav-subitems">
                  <button
                    v-for="loc in availableLocales"
                    :key="loc.code"
                    @click="changeLocale(loc.code); mobileMenuOpen = false"
                    class="mobile-nav-sublink mobile-lang-btn"
                    :class="{ 'active': loc.code === currentLocale }"
                  >
                    <span class="mobile-flag">{{ loc.flag || '' }}</span>
                    {{ loc.name }}
                  </button>
                </div>
              </Transition>
            </div>

            <!-- Login / Logout -->
            <router-link
              v-if="!loginStore.isLoggedIn"
              to="/login"
              class="mobile-auth-btn"
              @click="mobileMenuOpen = false"
            >
              {{ t('nav.login') }}
            </router-link>
            <button
              v-if="loginStore.isLoggedIn"
              @click="logout; mobileMenuOpen = false"
              class="mobile-logout-btn"
            >
              <span>🚪</span>
              {{ t('nav.logout') }}
            </button>
          </div>
        </div>
      </div>
    </Transition>
  </header>
</template>

<script setup>
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useLoginStore, useThemeStore, useUpdateStore, useSysInfoStore } from './stores.js'
import { availableLocales } from './locales/index.js'

const { t, locale } = useI18n()
const router = useRouter()
const route = useRoute()
const loginStore = useLoginStore()
const themeStore = useThemeStore()
const updateStore = useUpdateStore()
const sysInfoStore = useSysInfoStore()

const showBanner = ref(true)
const dismissedVersion = ref(localStorage.getItem('dismissedUpdate'))

// Mobile menu state
const mobileMenuOpen = ref(false)
const mobileSettingsExpanded = ref(false)
const mobileLanguageExpanded = ref(false)

// Desktop dropdown state
const settingsOpen = ref(false)
const localeOpen = ref(false)

// Computed: is current route a settings-related route?
const isSettingsRoute = computed(() => {
  const path = route.path
  return path.startsWith('/settings') || path === '/firmware' || path === '/monitoring' || path === '/systemlog'
})

// Close mobile menu on route change
watch(() => route.path, () => {
  mobileMenuOpen.value = false
})

// Prevent body scroll when mobile menu is open
watch(mobileMenuOpen, (isOpen) => {
  if (isOpen) {
    document.body.style.overflow = 'hidden'
  } else {
    document.body.style.overflow = ''
  }
})

// Cleanup on unmount
onUnmounted(() => {
  document.body.style.overflow = ''
})

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
  mobileMenuOpen.value = false
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
  background: rgba(255, 255, 255, 0.85);
  backdrop-filter: blur(20px);
  -webkit-backdrop-filter: blur(20px);
  border-radius: var(--radius-xl);
  box-shadow: var(--shadow-lg);
  padding: 0;
  margin-bottom: var(--spacing-xl);
  border: 1px solid rgba(255, 255, 255, 0.5);
}

[data-bs-theme="dark"] .navbar-glass {
  background: rgba(28, 28, 30, 0.85);
  border: 1px solid rgba(255, 255, 255, 0.1);
}

.navbar-container {
  display: flex;
  align-items: center;
  justify-content: space-between;
  width: 100%;
  padding: var(--spacing-sm) var(--spacing-md);
  flex-wrap: nowrap;
}

/* Brand */
.brand {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
  text-decoration: none;
  margin-right: var(--spacing-xl);
  flex-shrink: 0;
}

.brand-logo {
  width: 36px;
  height: 36px;
  background: linear-gradient(135deg, var(--color-primary) 0%, var(--color-primary-dark) 100%);
  border-radius: 10px;
  display: flex;
  align-items: center;
  justify-content: center;
  box-shadow: var(--shadow-sm);
}

.brand-icon {
  font-size: 1.125rem;
  filter: drop-shadow(0 2px 4px rgba(0,0,0,0.1));
}

.brand-text {
  font-weight: 700;
  font-size: 1.125rem;
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

/* Mobile right section (theme toggle + hamburger) */
.navbar-right-mobile {
  display: none;
  align-items: center;
  gap: 4px;
}

/* Custom Hamburger */
.navbar-toggle-custom {
  border: none;
  padding: 8px;
  background: transparent;
  display: flex;
  flex-direction: column;
  gap: 5px;
  width: 44px;
  height: 44px;
  justify-content: center;
  align-items: center;
  cursor: pointer;
  border-radius: var(--radius-md);
  transition: background 0.2s;
}

.navbar-toggle-custom:active {
  background: var(--color-bg);
}

.hamburger-line {
  width: 22px;
  height: 2px;
  background-color: var(--color-text);
  border-radius: 2px;
  transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
  transform-origin: center;
}

.navbar-toggle-custom.is-open .hamburger-line:nth-child(1) {
  transform: translateY(7px) rotate(45deg);
}

.navbar-toggle-custom.is-open .hamburger-line:nth-child(2) {
  opacity: 0;
  transform: scaleX(0);
}

.navbar-toggle-custom.is-open .hamburger-line:nth-child(3) {
  transform: translateY(-7px) rotate(-45deg);
}

/* ===== Desktop Navigation ===== */
.nav-content-wrapper {
  display: flex;
  width: 100%;
  justify-content: space-between;
  align-items: center;
}

.nav-links {
  display: flex;
  align-items: center;
  gap: 4px;
}

.nav-link-item {
  font-size: 0.9375rem;
  font-weight: 600;
  padding: 0.5rem 1rem;
  border-radius: var(--radius-full);
  color: var(--color-text-secondary);
  text-decoration: none;
  transition: all 0.2s;
  cursor: pointer;
  border: none;
  background: transparent;
  display: inline-flex;
  align-items: center;
  gap: 4px;
}

.nav-link-item:hover {
  background-color: var(--color-bg);
  color: var(--color-primary);
}

.nav-link-item.active {
  background-color: var(--color-primary-light);
  color: var(--color-primary);
  box-shadow: 0 0 0 1px rgba(255, 107, 53, 0.15);
}

[data-bs-theme="dark"] .nav-link-item.active {
  background-color: rgba(255, 107, 53, 0.2);
}

.dropdown-chevron {
  margin-left: 2px;
  transition: transform 0.2s;
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

/* Settings Dropdown */
.settings-dropdown {
  position: relative;
}

.dropdown-panel {
  position: absolute;
  top: calc(100% + 8px);
  left: 0;
  min-width: 200px;
  background: var(--color-surface);
  border-radius: var(--radius-lg);
  box-shadow: var(--shadow-xl);
  padding: 8px;
  z-index: 2000;
  border: 1px solid var(--color-border-light);
}

.dropdown-panel.dropdown-right {
  left: auto;
  right: 0;
}

.dropdown-link {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 14px;
  border-radius: var(--radius-md);
  color: var(--color-text);
  text-decoration: none;
  font-weight: 500;
  font-size: 0.9375rem;
  transition: background 0.15s;
  border: none;
  background: transparent;
  width: 100%;
  cursor: pointer;
  text-align: left;
}

.dropdown-link:hover {
  background-color: var(--color-bg);
}

.dropdown-link.active {
  color: var(--color-primary);
  background-color: var(--color-primary-light);
}

.dropdown-divider {
  height: 1px;
  background: var(--color-border-light);
  margin: 4px 0;
}

/* Dropdown transitions */
.dropdown-fade-enter-active,
.dropdown-fade-leave-active {
  transition: all 0.2s cubic-bezier(0.16, 1, 0.3, 1);
}

.dropdown-fade-enter-from,
.dropdown-fade-leave-to {
  opacity: 0;
  transform: translateY(-8px);
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
  border: none;
}

.action-icon-btn:hover {
  background-color: var(--color-bg);
}

.logout-btn:hover {
  background-color: var(--color-danger-light);
  color: var(--color-danger);
}

.locale-dropdown {
  position: relative;
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
  border: none;
}

.locale-btn:hover {
  background-color: var(--color-bg);
}

.auth-btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 0.5rem 1.25rem;
  font-weight: 600;
  font-size: 0.9375rem;
  border-radius: var(--radius-full);
  background-color: var(--color-primary);
  color: white;
  text-decoration: none;
  transition: all 0.2s;
}

.auth-btn:hover {
  background-color: var(--color-primary-hover);
}

/* ===== Mobile Menu ===== */
.mobile-menu-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.4);
  backdrop-filter: blur(4px);
  -webkit-backdrop-filter: blur(4px);
  z-index: 9999;
  display: flex;
  justify-content: flex-end;
}

.mobile-menu-panel {
  width: 85%;
  max-width: 320px;
  height: 100%;
  background: var(--color-surface);
  box-shadow: -4px 0 24px rgba(0, 0, 0, 0.15);
  display: flex;
  flex-direction: column;
  overflow-y: auto;
  -webkit-overflow-scrolling: touch;
  padding: env(safe-area-inset-top, 20px) 0 env(safe-area-inset-bottom, 20px);
}

[data-bs-theme="dark"] .mobile-menu-panel {
  background: var(--color-surface);
  box-shadow: -4px 0 24px rgba(0, 0, 0, 0.4);
}

.mobile-nav-section {
  flex: 1;
  padding: var(--spacing-lg) var(--spacing-md);
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.mobile-nav-link {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  padding: 14px 16px;
  border-radius: var(--radius-lg);
  color: var(--color-text);
  text-decoration: none;
  font-weight: 600;
  font-size: 1rem;
  transition: background 0.15s;
  min-height: var(--touch-target);
}

.mobile-nav-link:hover,
.mobile-nav-link:active {
  background: var(--color-bg);
}

.mobile-nav-link.router-link-active {
  background: var(--color-primary-light);
  color: var(--color-primary);
}

.mobile-nav-icon {
  font-size: 1.25rem;
  width: 28px;
  text-align: center;
}

/* Mobile Settings Group */
.mobile-nav-group {
  display: flex;
  flex-direction: column;
}

.mobile-nav-group-header {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  padding: 14px 16px;
  border-radius: var(--radius-lg);
  color: var(--color-text);
  font-weight: 600;
  font-size: 1rem;
  background: transparent;
  border: none;
  cursor: pointer;
  min-height: var(--touch-target);
  width: 100%;
  text-align: left;
  transition: background 0.15s;
}

.mobile-nav-group-header:active {
  background: var(--color-bg);
}

.mobile-chevron {
  margin-left: auto;
  transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);
  color: var(--color-text-secondary);
}

.mobile-chevron.expanded {
  transform: rotate(180deg);
}

.mobile-nav-subitems {
  padding: 0 0 0 44px;
  display: flex;
  flex-direction: column;
  gap: 1px;
}

.mobile-nav-sublink {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 12px 16px;
  border-radius: var(--radius-md);
  color: var(--color-text-secondary);
  text-decoration: none;
  font-weight: 500;
  font-size: 0.9375rem;
  transition: all 0.15s;
  min-height: var(--touch-target);
}

.mobile-nav-sublink:hover,
.mobile-nav-sublink:active {
  background: var(--color-bg);
  color: var(--color-text);
}

.mobile-nav-sublink.router-link-active {
  color: var(--color-primary);
  background: var(--color-primary-light);
}

.mobile-divider {
  height: 1px;
  background: var(--color-border-light);
  margin: 4px 0;
}

/* Mobile Actions Section */
.mobile-actions-section {
  padding: var(--spacing-md);
  border-top: 1px solid var(--color-border-light);
  display: flex;
  flex-direction: column;
  gap: var(--spacing-md);
}

.current-lang-code {
  margin-left: auto;
  margin-right: 8px;
  font-size: 0.8rem;
  font-weight: 600;
  color: var(--color-text-secondary);
  background: var(--color-bg);
  padding: 2px 6px;
  border-radius: 4px;
}

.mobile-lang-btn {
  width: 100%;
  border: none;
  background: transparent;
  cursor: pointer;
  text-align: left;
}

.mobile-flag {
  width: 24px;
  display: inline-block;
  text-align: center;
}

.mobile-auth-btn {
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 14px;
  border-radius: var(--radius-lg);
  background: var(--color-primary);
  color: white;
  font-weight: 600;
  font-size: 1rem;
  text-decoration: none;
  min-height: var(--touch-target);
}

.mobile-logout-btn {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: var(--spacing-sm);
  padding: 14px;
  border-radius: var(--radius-lg);
  background: var(--color-danger-light);
  color: var(--color-danger);
  font-weight: 600;
  font-size: 1rem;
  border: none;
  cursor: pointer;
  min-height: var(--touch-target);
}

/* Mobile Menu Transitions */
.mobile-menu-enter-active {
  transition: opacity 0.3s ease;
}

.mobile-menu-leave-active {
  transition: opacity 0.2s ease;
}

.mobile-menu-enter-from,
.mobile-menu-leave-to {
  opacity: 0;
}

.mobile-menu-enter-active .mobile-menu-panel {
  animation: slideInRight 0.3s cubic-bezier(0.16, 1, 0.3, 1);
  will-change: transform;
}

.mobile-menu-leave-active .mobile-menu-panel {
  animation: slideOutRight 0.2s ease forwards;
  will-change: transform;
}

@keyframes slideInRight {
  from { transform: translateX(100%); }
  to { transform: translateX(0); }
}

@keyframes slideOutRight {
  from { transform: translateX(0); }
  to { transform: translateX(100%); }
}

/* Expand transition for sub-items */
.expand-enter-active,
.expand-leave-active {
  transition: all 0.3s ease;
  max-height: 500px;
  opacity: 1;
  overflow: hidden;
}

.expand-enter-from,
.expand-leave-to {
  max-height: 0;
  opacity: 0;
  overflow: hidden;
}

/* Update Notification */
.update-notification {
  position: absolute;
  top: 1rem;
  right: 1rem;
  width: auto;
  max-width: 340px;
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
  flex-shrink: 0;
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

/* Banner Transition */
.slide-down-enter-active,
.slide-down-leave-active {
  transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
}

.slide-down-enter-from,
.slide-down-leave-to {
  opacity: 0;
  transform: translateY(-20px);
}

/* ===== Responsive: Mobile (<992px) ===== */
@media (max-width: 991px) {
  .navbar-right-mobile {
    display: flex;
  }

  .desktop-nav {
    display: none !important;
  }

  .navbar-glass {
    border-radius: var(--radius-lg);
    margin-bottom: var(--spacing-md);
  }

  .brand-text {
    font-size: 1rem;
  }
}

/* ===== Desktop (>=992px) ===== */
@media (min-width: 992px) {
  .navbar-toggle-custom {
    display: none;
  }

  .navbar-right-mobile {
    display: none;
  }

  .brand-logo {
    width: 40px;
    height: 40px;
    border-radius: 12px;
  }

  .brand-icon {
    font-size: 1.25rem;
  }

  .brand-text {
    font-size: 1.25rem;
  }
}
</style>
