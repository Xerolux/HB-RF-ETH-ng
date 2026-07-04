import { computed } from 'vue'

export const useHeaderNavigation = (t, loginStore) => {
  const navItems = computed(() => [
    { to: '/', icon: 'dashboard', label: t('nav.home'), public: true },
    { to: '/settings', icon: 'settings', label: t('nav.settings') },
    { to: '/firmware', icon: 'firmware', label: t('nav.firmware') },
    { to: '/monitoring', icon: 'monitoring', label: t('nav.monitoring') },
    { to: '/systemlog', icon: 'logs', label: t('nav.systemlog') },
    { to: '/about', icon: 'info', label: t('nav.about'), public: true }
  ])

  const visibleNavItems = computed(() => navItems.value.filter((item) => item.public || loginStore.isLoggedIn))

  return {
    navItems,
    visibleNavItems
  }
}
