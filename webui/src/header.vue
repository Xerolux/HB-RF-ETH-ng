<template>
  <BNavbar type="dark" variant="primary" toggleable="md" class="mb-3 mt-3">
    <BNavbarBrand tag="h1" class="font-weight-bold">HB-RF-ETH-ng</BNavbarBrand>
    <BNavbarToggle target="nav-collapse" />
    <BCollapse id="nav-collapse" is-nav>
      <BNavbarNav>
        <BNavItem to="/">{{ t('nav.home') }}</BNavItem>
        <BNavItem to="/settings" v-if="loginStore.isLoggedIn">{{ t('nav.settings') }}</BNavItem>
        <BNavItem to="/firmware" v-if="loginStore.isLoggedIn">{{ t('nav.firmware') }}</BNavItem>
        <BNavItem to="/monitoring" v-if="loginStore.isLoggedIn">{{ t('nav.monitoring') }}</BNavItem>
        <BNavItem to="/analyzer" v-if="loginStore.isLoggedIn && sysInfoStore.enableAnalyzer && settingsStore.analyzerEnabled">{{ t('nav.analyzer') }}</BNavItem>
        <BNavItem to="/log" v-if="loginStore.isLoggedIn">{{ t('nav.log') }}</BNavItem>
        <BNavItem to="/about">{{ t('nav.about') }}</BNavItem>
      </BNavbarNav>
      <BNavbarNav class="ms-auto">
        <BNavItem to="/login" v-if="!loginStore.isLoggedIn" class="me-2">{{ t('nav.login') }}</BNavItem>
        <BNavItemDropdown :text="currentLocaleName" size="sm" variant="outline-light" class="me-2">
          <BDropdownItem
            v-for="locale in availableLocales"
            :key="locale.code"
            @click="changeLocale(locale.code)"
            :active="locale.code === currentLocale"
          >
            {{ locale.name }}
          </BDropdownItem>
        </BNavItemDropdown>
        <BNavItem
          href="#"
          @click.prevent="themeStore.toggleTheme"
          class="me-2"
          v-b-tooltip.hover
          :title="t('nav.toggleTheme')"
          :aria-label="t('nav.toggleTheme')"
        >
          {{ themeStore.theme === 'light' ? '🌙' : '☀️' }}
        </BNavItem>
        <BNavItem v-if="loginStore.isLoggedIn" @click.prevent="logout" href="#" class="me-2 text-warning font-weight-bold">
            {{ t('nav.logout') }}
        </BNavItem>
      </BNavbarNav>
    </BCollapse>
  </BNavbar>
</template>

<script setup>
import { computed, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useLoginStore, useThemeStore, useSettingsStore, useSysInfoStore } from './stores.js'
import { availableLocales } from './locales/index.js'

const { t, locale } = useI18n()
const router = useRouter()
const loginStore = useLoginStore()
const themeStore = useThemeStore()
const settingsStore = useSettingsStore()
const sysInfoStore = useSysInfoStore()

onMounted(async () => {
    if (loginStore.isLoggedIn) {
        await Promise.all([settingsStore.load(), sysInfoStore.update()])
    }
})

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
</script>
