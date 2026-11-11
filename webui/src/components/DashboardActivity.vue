<template>
<section class="card-glass session-history" :aria-label="t('dashboard.history')">
  <div class="history-heading"><div><h2>{{ t('dashboard.history') }}</h2><p>{{ t('dashboard.historyHint') }}</p></div><AppIcon name="activity" /></div>
  <div class="history-grid">
<div v-for="metric in metrics" :key="metric.key" class="history-metric" :class="metric.key">
    <div class="history-label"><span>{{ t(metric.label) }}</span><strong>{{ latest(metric.key) }}</strong></div>
    <svg v-if="samples.length > 1" viewBox="0 0 600 100" preserveAspectRatio="none" role="img" :aria-label="`${t(metric.label)}: ${latest(metric.key)}`"><path class="history-guide" d="M0 25H600 M0 50H600 M0 75H600" /><polyline :points="points(metric.key)" /></svg>
    <div v-else class="history-empty">{{ t('dashboard.collecting') }}</div>
    <div class="history-scale"><span>{{ formatTime(samples[0]) }}</span><span>0–100%</span><span>{{ formatTime(samples.at(-1)) }}</span></div>
  </div>
</div>
</section>
</template>
<script setup>
import { useI18n } from 'vue-i18n'
const props = defineProps({ samples: { type: Array, default: () => [] } })
const { t, locale } = useI18n()
const metrics = [{ key: 'cpu', label: 'sysinfo.cpuUsage' }, { key: 'memory', label: 'sysinfo.memoryUsage' }]
const clamp = value => Number.isFinite(value) ? Math.min(100, Math.max(0, value)) : 0
const latest = key => Number.isFinite(props.samples.at(-1)?.[key]) ? `${Math.round(clamp(props.samples.at(-1)[key]))}%` : '—'
const points = key => {
  const start = props.samples[0]?.time || 0
  const duration = Math.max(1, (props.samples.at(-1)?.time || 0) - start)
  return props.samples.map(sample => `${((sample.time - start) / duration) * 600},${98 - clamp(sample[key]) * 0.96}`).join(' ')
}
const formatTime = sample => sample ? new Date(sample.time).toLocaleTimeString(locale.value, { hour: '2-digit', minute: '2-digit', second: '2-digit' }) : '—'
</script>
<style scoped>
.session-history { padding: var(--card-padding); }
.history-heading, .history-label, .history-scale { display: flex; align-items: center; justify-content: space-between; gap: var(--space-3); }
.history-heading h2 { font-size: var(--fs-md); margin: 0; }
.history-heading p { color: var(--color-text-secondary); font-size: var(--fs-xs); margin: var(--space-2) 0 var(--space-4); }
.history-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: var(--space-5); }
.history-metric { min-width: 0; color: var(--color-primary); }
.history-metric.memory { color: var(--color-success); }
.history-label { font-size: var(--fs-sm); }
.history-label > span, .history-scale { color: var(--color-text-secondary); }
.history-label strong { font-variant-numeric: tabular-nums; }
.history-metric svg { display: block; width: 100%; height: 100px; margin: var(--space-3) 0; overflow: hidden; }
.history-metric polyline { fill: none; stroke: currentColor; stroke-width: 2; vector-effect: non-scaling-stroke; stroke-linejoin: round; }
.history-guide { stroke: var(--color-border); stroke-width: 1; vector-effect: non-scaling-stroke; }
.history-scale { font-size: var(--fs-2xs); font-variant-numeric: tabular-nums; }
.history-empty { height: 100px; display: grid; place-items: center; color: var(--color-text-muted); font-size: var(--fs-xs); margin: var(--space-3) 0; }
@media (max-width: 600px) { .history-grid { grid-template-columns: 1fr; } }
</style>
