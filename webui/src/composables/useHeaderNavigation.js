import { computed } from 'vue'

export const useHeaderNavigation = (t, loginStore) => {
  const navItems = computed(() => [
    { to: '/', icon: 'dashboard', label: t('nav.home'), group: 'overview', public: true },
    { to: '/monitoring', icon: 'monitoring', label: t('nav.monitoring'), group: 'overview' },
    { to: '/settings', icon: 'settings', label: t('nav.settings'), group: 'system' },
    { to: '/firmware', icon: 'firmware', label: t('nav.firmware'), group: 'system' },
    { to: '/webui', icon: 'firmware', label: 'WebUI', group: 'system' },
    { to: '/systemlog', icon: 'logs', label: t('nav.systemlog'), group: 'system' },
    { to: '/about', icon: 'info', label: t('nav.about'), group: 'info', public: true }
  ])

  const navGroups = computed(() => [
    { id: 'overview', label: t('nav.groupOverview') },
    { id: 'system', label: t('nav.groupSystem') },
    { id: 'info', label: t('nav.groupInfo') }
  ])

  const visibleNavItems = computed(() => navItems.value.filter((item) => item.public || loginStore.isLoggedIn))
  const visibleNavGroups = computed(() => navGroups.value
    .map((group) => ({
      ...group,
      items: visibleNavItems.value.filter((item) => item.group === group.id)
    }))
    .filter((group) => group.items.length > 0))

  return {
    navItems,
    navGroups,
    visibleNavItems,
    visibleNavGroups
  }
}
