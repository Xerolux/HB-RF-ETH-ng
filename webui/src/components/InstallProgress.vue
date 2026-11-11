<template>
  <div class="install-progress" :aria-label="t('updateCenter.installation')">
    <ol class="install-steps">
      <li v-for="(step, index) in steps" :key="step.state" :class="{ current: index === active, complete: index < active }" :aria-current="index === active ? 'step' : undefined">
        <span class="step-marker"><AppIcon v-if="index < active" name="check" /><span v-else>{{ index + 1 }}</span></span><span>{{ t(step.label) }}</span>
      </li>
    </ol>
    <div class="install-progress-copy" role="status"><strong>{{ stateText }}</strong><span>{{ percent }}%</span></div>
    <div class="install-track" role="progressbar" :aria-label="stateText" :aria-valuenow="percent" aria-valuemin="0" aria-valuemax="100"><span :style="{ width: percent + '%' }"></span></div>
    <p>{{ t(install.target === 'firmware' ? 'updateCenter.firmwareFinish' : 'updateCenter.webuiFinish') }}</p>
  </div>
</template>
<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
const props = defineProps({ install: { type: Object, required: true } })
const { t } = useI18n()
const steps = [
  { state: 'preparing', label: 'updateCenter.prepare' },
  { state: 'downloading', label: 'updateCenter.download' },
  { state: 'verifying', label: 'updateCenter.verify' },
  { state: 'finalizing', label: 'updateCenter.activate' }
]
const active = computed(() => props.install.state === 'success' ? steps.length : steps.findIndex(step => step.state === props.install.state))
const percent = computed(() => { const value = Number(props.install.progress); return Number.isFinite(value) ? Math.round(Math.max(0, Math.min(100, value))) : 0 })
const stateText = computed(() => t(({ preparing: 'firmware.installPreparing', downloading: 'firmware.installDownloading', verifying: 'firmware.installVerifying', finalizing: 'firmware.installFinalizing', success: 'common.success' })[props.install.state] || 'updateCenter.installation'))
</script>
<style scoped>
.install-progress { display: flex; flex-direction: column; gap: var(--space-3); }
.install-steps { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: var(--space-2); list-style: none; padding: 0; margin: 0; }
.install-steps li { display: flex; flex-direction: column; align-items: center; text-align: center; gap: var(--space-2); font-size: var(--fs-xs); color: var(--color-text-secondary); overflow-wrap: anywhere; }
.step-marker { width: 28px; height: 28px; display: grid; place-items: center; border: 1px solid var(--color-border); border-radius: var(--radius-sm); font-variant-numeric: tabular-nums; }
.current { color: var(--color-primary) !important; }
.current .step-marker, .complete .step-marker { border-color: var(--color-primary); background: var(--color-primary-soft); color: var(--color-primary); }
.install-progress-copy { display: flex; justify-content: space-between; gap: var(--space-3); font-size: var(--fs-sm); font-variant-numeric: tabular-nums; }
.install-track { height: 8px; background: var(--color-bg-alt); overflow: hidden; border-radius: var(--radius-sm); }
.install-track > span { display: block; height: 100%; background: var(--color-primary); }
.install-progress p { margin: 0; color: var(--color-text-secondary); font-size: var(--fs-xs); }
@media (prefers-reduced-motion: no-preference) { .install-track > span { transition: width 250ms ease; } }
</style>
