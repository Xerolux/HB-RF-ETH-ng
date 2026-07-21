import { computed } from 'vue'
import { useExperimentalStore } from '../stores.js'
import { DOCUMENTATION_URL } from './useDocsLink.js'

export const useHeaderNavigation = (t, loginStore) => {
  const experimentalStore = useExperimentalStore()

  const navItems = computed(() => {
    const items = [
      { to: '/', icon: 'dashboard', label: t('nav.home'), group: 'overview', public: true },
      { to: '/monitoring', icon: 'monitoring', label: t('nav.monitoring'), group: 'overview' },
      { to: '/settings', icon: 'settings', label: t('nav.settings'), group: 'system' },
      { to: '/system-overview', icon: 'cpu', label: t('sysinfo.system'), group: 'system' },
      // The standalone "Design wechseln" menu item is intentionally removed
      // (Korrekturauftrag §5): theme selection lives only under Settings →
      // Design (embedded ThemeSettings). The header sun/moon toggle button
      // still flips light/dark quickly and is unaffected; /theme remains a
      // reachable route for bookmarks but is not advertised in the sidebar.
      // Firmware + WebUI merged under /updates (Korrekturauftrag §6).
      { to: '/updates/firmware', icon: 'firmware', label: t('nav.updates'), group: 'system' },
      { to: '/systemlog', icon: 'logs', label: t('nav.systemlog'), group: 'system' },
      { to: '/about', icon: 'info', label: t('nav.about'), group: 'info', public: true },
      // External documentation link (Korrekturauftrag §8). Lives in the info
      // group; opens in a new tab. The URL is configured in useDocsLink.js.
      { to: DOCUMENTATION_URL, icon: 'externalLink', label: t('nav.documentation'), group: 'info', public: true, external: true }
    ]
    // Experimental entries only show when the user opted in via Settings →
    // Allgemein (Korrekturauftrag §7). Values already stored on the device
    // are preserved — only the UI is hidden.
    return experimentalStore.showExperimental
      ? items.filter((item) => item.experimental !== false)
      : items.filter((item) => !item.experimental)
  })

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
