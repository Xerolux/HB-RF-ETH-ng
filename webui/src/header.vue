<template>
  <BNavbar type="dark" variant="primary" toggleable="sm" class="mb-3 mt-3 rounded">
    <BNavbarBrand tag="h1" class="font-weight-bold">HB-RF-ETH-ng</BNavbarBrand>
    <BNavbarToggle target="nav-collapse" />
    <BCollapse id="nav-collapse" is-nav>
      <BNavbarNav>
        <BNavItem href="/">{{ t('nav.home') }}</BNavItem>
        <BNavItem href="/settings" v-if="loginStore.isLoggedIn">{{ t('nav.settings') }}</BNavItem>
        <BNavItem href="/firmware" v-if="loginStore.isLoggedIn">{{ t('nav.firmware') }}</BNavItem>
        <BNavItem href="/monitoring" v-if="loginStore.isLoggedIn">{{
          t('nav.monitoring')
        }}</BNavItem>
        <BNavItem href="/about">{{ t('nav.about') }}</BNavItem>
      </BNavbarNav>
      <BNavbarNav class="ms-auto">
        <BNavItem href="/login" v-if="!loginStore.isLoggedIn" class="me-2">{{
          t('nav.login')
        }}</BNavItem>
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
        <BNavItem @click="themeStore.toggleTheme" class="me-2" style="cursor: pointer">
          {{ themeStore.theme === 'light' ? '🌙' : '☀️' }}
        </BNavItem>
        <BNavForm v-if="loginStore.isLoggedIn">
          <BButton size="sm" variant="outline-light" @click.prevent="logout">{{
            t('nav.logout')
          }}</BButton>
        </BNavForm>
      </BNavbarNav>
    </BCollapse>
  </BNavbar>
</template>

<script setup>
import { computed } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useLoginStore, useThemeStore } from './stores.js'
import { availableLocales } from './locales/index.js'

const { t, locale } = useI18n()
const router = useRouter()
const loginStore = useLoginStore()
const themeStore = useThemeStore()

const currentLocale = computed(() => locale.value)
const currentLocaleName = computed(() => {
  const current = availableLocales.find((l) => l.code === locale.value)
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
