<template>
  <div class="updates-page page-shell">
    <!-- Sub-tab switch between Firmware and WebUI (Korrekturauftrag §6).
         Each child route owns its own page-hero with the relevant version
         info, so this wrapper only renders the segmented control. The active
         tab is derived from the current path so direct URLs (/updates/webui)
         highlight correctly. -->
    <div class="tabs-container">
      <div class="segmented-control">
        <router-link
          v-for="tab in tabs"
          :key="tab.to"
          :to="tab.to"
          class="segment-btn"
          :class="{ active: isActive(tab) }"
        >
          <AppIcon :name="tab.icon" />
          <span class="segment-label">{{ tab.label }}</span>
        </router-link>
      </div>
    </div>

    <router-view />
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { useRoute } from 'vue-router'

const { t } = useI18n()
const route = useRoute()

const tabs = computed(() => [
  { to: '/updates/firmware', icon: 'firmware', label: t('updates.firmware') },
  { to: '/updates/webui', icon: 'firmware', label: t('updates.webui') }
])

const isActive = (tab) => {
  // Active sub-tab matches the current path prefix (so the firmware tab stays
  // highlighted while a sub-route is active too, if any are added later).
  return route.path === tab.to || route.path.startsWith(tab.to + '/')
}
</script>

<style scoped>
.updates-page {
  gap: 16px;
}

/* Reuse the same segmented-control look the Settings page uses. The styles
   for .tabs-container / .segmented-control / .segment-btn live in main.css
   (global) so both pages stay in sync. */
.segment-btn {
  text-decoration: none;
}
</style>
