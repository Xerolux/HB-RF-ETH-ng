<template>
  <section class="update-overview card-glass" :aria-label="t('updateCenter.comparison')" :aria-busy="statusLoading && !statusLoaded">
    <div class="version-comparison">
      <div><span>{{ t('firmware.installedLabel') }}</span><strong>{{ installed || running || '—' }}</strong></div>
      <AppIcon name="arrowRight" />
      <div><span>{{ t('updateCenter.availableVersion') }}</span><strong>{{ reliable ? latest || '—' : '—' }}</strong></div>
    </div>
    <div class="update-feedback" role="status">
      <span v-if="statusLoading && !statusLoaded">{{ t('common.loading') }}</span>
      <template v-else-if="statusLoadError"><AppIcon name="alert" /><span>{{ t('updateCenter.loadError') }}</span><button class="btn btn-sm btn-outline-secondary" :disabled="statusLoading" @click="retry">{{ t('updateCenter.retry') }}</button></template>
      <span v-else-if="checking">{{ t('firmware.checking') }}</span>
      <span v-else-if="!updateStatus.everChecked">{{ t('firmware.neverChecked') }} {{ t('updateCenter.searchHint') }}</span>
      <span v-else-if="!reliable">{{ t('updateCenter.searchHint') }}</span>
      <template v-else><AppIcon :name="available ? 'download' : 'check'" /><span>{{ available ? t('updateCenter.updateReady') : t('updateCenter.current') }}</span><span class="pill-chip">{{ updateStatus.channel === 'beta' ? t('firmware.channelBeta') : t('firmware.channelStable') }}</span></template>
    </div>
  </section>
</template>
<script setup>
import { computed, onBeforeUnmount } from 'vue'
import { useI18n } from 'vue-i18n'
import { useUpdateSearch } from '../composables/useUpdateSearch'
const props = defineProps({ target: { type: String, required: true }, installed: { type: String, default: '' } })
const { t } = useI18n()
const { updateStatus, channel, checking, statusLoading, statusLoadError, statusLoaded, loadUpdateStatus, resumeInstallPolling } = useUpdateSearch(t)
const firmware = computed(() => props.target === 'firmware')
const running = computed(() => firmware.value ? updateStatus.value.runningFirmware : updateStatus.value.runningWebui)
const latest = computed(() => firmware.value ? updateStatus.value.latestFirmware : updateStatus.value.latestWebui)
const available = computed(() => firmware.value ? updateStatus.value.firmwareUpdateAvailable : updateStatus.value.webuiUpdateAvailable)
const reliable = computed(() => statusLoaded.value && !statusLoadError.value && !checking.value && updateStatus.value.state !== 'running' && updateStatus.value.everChecked && !updateStatus.value.lastError && !updateStatus.value.lastSkipReason && channel.value === updateStatus.value.channel && !!latest.value)
let mounted = true
onBeforeUnmount(() => { mounted = false })
const retry = async () => { try { await loadUpdateStatus(); if (mounted) resumeInstallPolling() } catch { /* Retain the inline error. */ } }
</script>
<style scoped>
.update-overview { padding: var(--card-padding); display: flex; flex-direction: column; gap: var(--space-4); }
.version-comparison { display: grid; grid-template-columns: minmax(0, 1fr) auto minmax(0, 1fr); align-items: center; gap: var(--space-4); }
.version-comparison > div { display: flex; flex-direction: column; gap: var(--space-2); min-width: 0; }
.version-comparison span { color: var(--color-text-secondary); font-size: var(--fs-xs); }
.version-comparison strong { font-size: var(--fs-lg); overflow-wrap: anywhere; font-variant-numeric: tabular-nums; }
.version-comparison > .app-icon { color: var(--color-text-muted); }
.update-feedback { border-top: 1px solid var(--color-border); padding-top: var(--space-3); display: flex; flex-wrap: wrap; align-items: center; gap: var(--space-2); color: var(--color-text-secondary); font-size: var(--fs-sm); }
.update-feedback > .app-icon { flex-shrink: 0; }
</style>
