<template>
  <div class="page-shell theme-page">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow"><AppIcon name="sun" /> {{ copy.eyebrow }}</span>
        <h1 class="hero-title">{{ copy.title }}</h1>
        <p class="hero-subtitle">{{ copy.subtitle }}</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><span class="color-dot" :style="{ background: themeStore.primaryColor }"></span>{{ themeStore.primaryColor }}</span>
        <span class="meta-chip"><AppIcon :name="resolvedDark ? 'moon' : 'sun'" /> {{ themeLabel }}</span>
      </div>
    </section>

    <section class="theme-grid">
      <article class="theme-card">
        <div class="card-heading">
          <span class="icon-badge soft"><AppIcon name="moon" /></span>
          <div><h2>{{ copy.scheme }}</h2><p>{{ copy.schemeHint }}</p></div>
        </div>
        <div class="scheme-selector">
          <button
            v-for="option in schemes"
            :key="option.value"
            type="button"
            class="scheme-option"
            :class="{ active: themeStore.theme === option.value }"
            @click="themeStore.setTheme(option.value)"
          >
            <AppIcon :name="option.icon" />
            <strong>{{ option.label }}</strong>
          </button>
        </div>
      </article>

      <article class="theme-card">
        <div class="card-heading">
          <span class="icon-badge info"><AppIcon name="sun" /></span>
          <div><h2>{{ copy.accent }}</h2><p>{{ copy.accentHint }}</p></div>
        </div>
        <div class="color-presets">
          <button
            v-for="preset in presets"
            :key="preset"
            type="button"
            class="color-preset"
            :class="{ active: themeStore.primaryColor === preset }"
            :style="{ background: preset }"
            :aria-label="preset"
            @click="themeStore.setPrimaryColor(preset)"
          ></button>
        </div>
        <label class="custom-color">
          <span>{{ copy.custom }}</span>
          <input
            type="color"
            :value="themeStore.primaryColor"
            @input="previewColor"
            @change="saveColor"
          />
          <code>{{ pendingColor }}</code>
        </label>
      </article>

      <article class="theme-card preview-card">
        <div class="card-heading">
          <span class="icon-badge success"><AppIcon name="check" /></span>
          <div><h2>{{ copy.preview }}</h2><p>{{ copy.previewHint }}</p></div>
        </div>
        <div class="preview-panel">
          <div class="preview-header">
            <span class="preview-logo"></span>
            <strong>HB-RF-ETH-ng</strong>
            <span class="preview-status">{{ copy.online }}</span>
          </div>
          <div class="preview-content">
            <div class="preview-metric"><span>CPU</span><strong>12%</strong><i style="width:12%"></i></div>
            <div class="preview-metric"><span>RAM</span><strong>43%</strong><i style="width:43%"></i></div>
            <button type="button" class="preview-button">{{ copy.action }}</button>
          </div>
        </div>
      </article>
    </section>

    <BAlert variant="info" :model-value="true">
      {{ copy.deviceWide }}
    </BAlert>
  </div>
</template>

<script setup>
import { computed, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useThemeStore } from './stores.js'

const { locale } = useI18n()
const themeStore = useThemeStore()
const pendingColor = ref(themeStore.primaryColor)

watch(() => themeStore.primaryColor, (value) => { pendingColor.value = value })

const translations = {
  de: {
    eyebrow: 'Darstellung', title: 'Theme konfigurieren',
    subtitle: 'Farbschema und Akzentfarbe werden direkt auf dem Gerät gespeichert.',
    scheme: 'Farbschema', schemeHint: 'Automatisch dem System folgen oder fest einstellen.',
    system: 'System', light: 'Hell', dark: 'Dunkel', accent: 'Akzentfarbe',
    accentHint: 'Wird für Navigation, Buttons und Hervorhebungen verwendet.', custom: 'Eigene Farbe',
    preview: 'Vorschau', previewHint: 'Beispiel für Karten und Aktionen.', online: 'Online', action: 'Aktion',
    deviceWide: 'Die Einstellung gilt geräteweit. Andere Browser übernehmen sie beim nächsten Öffnen automatisch.'
  },
  en: {
    eyebrow: 'Appearance', title: 'Configure Theme',
    subtitle: 'Color scheme and accent color are stored directly on the device.',
    scheme: 'Color scheme', schemeHint: 'Follow the operating system or choose a fixed mode.',
    system: 'System', light: 'Light', dark: 'Dark', accent: 'Accent color',
    accentHint: 'Used for navigation, buttons and highlights.', custom: 'Custom color',
    preview: 'Preview', previewHint: 'Example cards and actions.', online: 'Online', action: 'Action',
    deviceWide: 'This setting is device-wide. Other browsers automatically adopt it when opened.'
  }
}

const copy = computed(() => translations[locale.value] || translations.en)
const schemes = computed(() => [
  { value: 'system', label: copy.value.system, icon: 'settings' },
  { value: 'light', label: copy.value.light, icon: 'sun' },
  { value: 'dark', label: copy.value.dark, icon: 'moon' }
])
const presets = ['#f26a3d', '#3971e8', '#1ca971', '#8b5cf6', '#e89a24']
const resolvedDark = computed(() => document.documentElement.getAttribute('data-bs-theme') === 'dark')
const themeLabel = computed(() => schemes.value.find((item) => item.value === themeStore.theme)?.label || copy.value.system)

const previewColor = (event) => {
  pendingColor.value = event.target.value
  themeStore.setPrimaryColor(event.target.value, false)
}
const saveColor = (event) => themeStore.setPrimaryColor(event.target.value, true)
</script>

<style scoped>
.theme-page { display: flex; flex-direction: column; gap: 20px; }
.color-dot { width: 12px; height: 12px; border-radius: 50%; display: inline-block; }
.theme-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 18px; }
.theme-card { border: 1px solid var(--color-border-light); background: var(--color-surface); border-radius: var(--radius-xl); box-shadow: var(--shadow-sm); padding: 24px; }
.preview-card { grid-column: 1 / -1; }
.card-heading { display: flex; gap: 12px; align-items: flex-start; margin-bottom: 20px; }
.card-heading h2 { font-size: 1.25rem; }
.card-heading p { color: var(--color-text-secondary); margin-top: 4px; }
.scheme-selector { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }
.scheme-option { border: 1px solid var(--color-border); background: var(--color-bg-alt); color: var(--color-text); border-radius: var(--radius-md); min-height: 88px; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 8px; cursor: pointer; }
.scheme-option.active { border-color: var(--color-primary); color: var(--color-primary); background: var(--color-primary-soft); box-shadow: inset 0 0 0 1px var(--color-primary); }
.color-presets { display: flex; flex-wrap: wrap; gap: 14px; margin-bottom: 20px; }
.color-preset { width: 46px; height: 46px; border-radius: 50%; border: 4px solid var(--color-surface); box-shadow: 0 0 0 1px var(--color-border-strong); cursor: pointer; }
.color-preset.active { box-shadow: 0 0 0 3px var(--color-text), 0 0 0 5px var(--color-surface); }
.custom-color { display: grid; grid-template-columns: 1fr auto auto; gap: 12px; align-items: center; border-top: 1px solid var(--color-border-light); padding-top: 18px; }
.custom-color input { width: 52px; height: 38px; border: 0; background: transparent; cursor: pointer; }
.preview-panel { border: 1px solid var(--color-border); border-radius: var(--radius-lg); overflow: hidden; background: var(--color-bg); }
.preview-header { display: flex; align-items: center; gap: 10px; padding: 14px 18px; background: var(--color-surface); border-bottom: 1px solid var(--color-border-light); }
.preview-logo { width: 24px; height: 24px; border-radius: 8px; background: var(--color-primary); }
.preview-status { margin-left: auto; color: var(--color-success); font-size: .82rem; font-weight: 700; }
.preview-content { display: grid; grid-template-columns: repeat(2, 1fr) auto; gap: 14px; align-items: end; padding: 20px; }
.preview-metric { position: relative; padding: 14px; border-radius: var(--radius-md); background: var(--color-surface); display: flex; justify-content: space-between; overflow: hidden; }
.preview-metric i { position: absolute; left: 0; bottom: 0; height: 4px; background: var(--color-primary); }
.preview-button { border: 0; border-radius: var(--radius-md); background: var(--color-primary); color: white; min-height: 48px; padding: 0 22px; font-weight: 700; }
@media (max-width: 760px) { .theme-grid { grid-template-columns: 1fr; } .preview-card { grid-column: auto; } .preview-content { grid-template-columns: 1fr; } }
@media (max-width: 480px) { .scheme-selector { grid-template-columns: 1fr; } .custom-color { grid-template-columns: 1fr auto; } .custom-color code { grid-column: 1 / -1; } }
</style>
